#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* PolicySpecInputDirectory = TEXT("Json/Input/PolicySpecs");

	FString ResolvePolicySpecJsonFilePath(const FString& policySpecJsonFilePath)
	{
		FString normalizedPath = policySpecJsonFilePath.TrimStartAndEnd();

		if (normalizedPath.IsEmpty())
		{
			return FString{};
		}

		FPaths::NormalizeFilename(normalizedPath);

		if (FPaths::GetExtension(normalizedPath).IsEmpty())
		{
			normalizedPath = FPaths::SetExtension(normalizedPath, TEXT("json"));
		}

		if (FPaths::IsRelative(normalizedPath) && FPaths::GetPath(normalizedPath).IsEmpty())
		{
			normalizedPath = FPaths::Combine(PolicySpecInputDirectory, normalizedPath);
		}

		if (FPaths::IsRelative(normalizedPath))
		{
			return FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectDir(), normalizedPath)
			);
		}

		return normalizedPath;
	}

	bool TryReadVectorObject(const TSharedPtr<FJsonObject>& object, FVector& outVector)
	{
		if (!object.IsValid())
			return false;

		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		if (!object->TryGetNumberField(TEXT("x"), x) || !object->TryGetNumberField(TEXT("y"), y))
			return false;

		object->TryGetNumberField(TEXT("z"), z);
		outVector = FVector(x, y, z);
		return true;
	}

	void ReadVectorArrayField(const TSharedPtr<FJsonObject>& object, const TCHAR* fieldName, TArray<FVector>& outVectors)
	{
		outVectors.Reset();

		const TArray<TSharedPtr<FJsonValue>>* values = nullptr;
		if (!object.IsValid() || !object->TryGetArrayField(fieldName, values) || values == nullptr)
			return;

		for (const TSharedPtr<FJsonValue>& value : *values)
		{
			if (!value.IsValid())
				continue;

			FVector vector;
			if (TryReadVectorObject(value->AsObject(), vector))
			{
				outVectors.Add(vector);
			}
		}
	}
}



DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotHttpPolicy, Log, All);

UDeliveryBot_HttpPolicyComponent::UDeliveryBot_HttpPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeliveryBot_HttpPolicyComponent::BeginPlay()
{
	Super::BeginPlay();

	bIsEndingPlay = false;
}

void UDeliveryBot_HttpPolicyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsEndingPlay = true;

	CancelActiveRequest();
	CancelActiveGridRequest();
	CancelActiveEpisodeStartRequest();
	CancelActiveEpisodeConfigUpdateRequest();
	CancelActivePolicyCatalogSourcesRequest();
	CancelActivePolicyCatalogRequest();
	CancelActivePolicySpecUpdateRequest();

	OnPolicyResponse.Clear();
	OnParsedPolicyResponse.Clear();
	OnGridResponse.Clear();
	OnEpisodeStartResponse.Clear();
	OnEpisodeConfigUpdateResponse.Clear();
	OnPolicyCatalogSourcesResponse.Clear();
	OnParsedPolicyCatalogSourcesResponse.Clear();
	OnPolicyCatalogResponse.Clear();
	OnParsedPolicyCatalogResponse.Clear();
	OnPolicySpecUpdateResponse.Clear();
	
	Super::EndPlay(EndPlayReason);
}

bool UDeliveryBot_HttpPolicyComponent::SendObservationJson(const FString& observationJson)
{
	if (observationJson.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Observation JSON is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy request skipped because component is ending play."));
		return false;
	}

	if (bRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy request skipped because previous request is still in flight."));
		return false;
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(PolicyServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(observationJson);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);
	request->OnProcessRequestComplete().BindLambda([weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
	{
		const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
		const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};
		AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
		{
			if (!weakThis.IsValid())
				return;

			UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
			component->bRequestInFlight = false;
			component->ActiveRequest.Reset();

			if (component->bIsEndingPlay)
				return;

			if (component->bLogPolicyResponseBody)
			{
				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Policy response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);
			}

			component->OnPolicyResponse.Broadcast(bWasSuccessful, responseCode, responseBody);
			FDeliveryBotHttpPolicyResponseInfo responseInfo;
			if (bWasSuccessful && responseCode >= 200 && responseCode < 300)
			{
				component->TryParsePolicyResponseJson(responseBody, responseInfo);
			}
			else
			{
				responseInfo.RawResponseBody = responseBody;
				responseInfo.ErrorMessage = TEXT("HTTP request failed.");
			}

			component->OnParsedPolicyResponse.Broadcast(responseInfo);
		});
	}
	);

	bRequestInFlight = true;
	ActiveRequest = request;

	if (!request->ProcessRequest())
	{
		bRequestInFlight = false;
		ActiveRequest.Reset();
		return false;
	}

	return true;
}

void UDeliveryBot_HttpPolicyComponent::CancelActiveRequest()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}

	bRequestInFlight = false;
}

bool UDeliveryBot_HttpPolicyComponent::TryParsePolicyResponseJson(const FString& responseBody, FDeliveryBotHttpPolicyResponseInfo& outResponseInfo) const
{
	outResponseInfo = FDeliveryBotHttpPolicyResponseInfo{};
	outResponseInfo.RawResponseBody = responseBody;

	if (responseBody.IsEmpty())
	{
		outResponseInfo.ErrorMessage = TEXT("Response body is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outResponseInfo.ErrorMessage = TEXT("Failed to parse policy response JSON.");
		return false;
	}

	outResponseInfo.Sequence = rootObject->GetIntegerField(TEXT("sequence"));
	rootObject->TryGetStringField(TEXT("status"), outResponseInfo.Status);

	rootObject->TryGetNumberField(TEXT("episodeVersion"), outResponseInfo.EpisodeVersion);
	rootObject->TryGetNumberField(TEXT("configVersion"), outResponseInfo.ConfigVersion);
	rootObject->TryGetNumberField(TEXT("gridVersion"), outResponseInfo.GridVersion);

	const TSharedPtr<FJsonObject>* actionObject = nullptr;
	if (rootObject->TryGetObjectField(TEXT("action"), actionObject) && actionObject && actionObject->IsValid())
	{
		outResponseInfo.bHasAction = true;

		(*actionObject)->TryGetNumberField(TEXT("steering"), outResponseInfo.Action.Steering);
		(*actionObject)->TryGetNumberField(TEXT("throttle"), outResponseInfo.Action.Throttle);
		(*actionObject)->TryGetNumberField(TEXT("brake"), outResponseInfo.Action.Brake);
		(*actionObject)->TryGetNumberField(TEXT("targetSpeedKmh"), outResponseInfo.Action.TargetSpeedKmh);
		(*actionObject)->TryGetStringField(TEXT("direction"), outResponseInfo.Action.Direction);
	}

	const TSharedPtr<FJsonObject>* debugObject = nullptr;
	if (rootObject->TryGetObjectField(TEXT("debug"), debugObject) && debugObject && debugObject->IsValid())
	{
		(*debugObject)->TryGetStringField(TEXT("policyName"), outResponseInfo.Debug.PolicyName);
		(*debugObject)->TryGetStringField(TEXT("reason"), outResponseInfo.Debug.Reason);
		(*debugObject)->TryGetStringField(TEXT("pathStatus"), outResponseInfo.Debug.PathStatus);
		(*debugObject)->TryGetNumberField(TEXT("pathLength"), outResponseInfo.Debug.PathLength);

		double lookAheadX = 0.0;
		double lookAheadY = 0.0;
		double lookAheadZ = 0.0;
		if ((*debugObject)->TryGetNumberField(TEXT("lookAheadWorldX"), lookAheadX)
			&& (*debugObject)->TryGetNumberField(TEXT("lookAheadWorldY"), lookAheadY))
		{
			(*debugObject)->TryGetNumberField(TEXT("lookAheadWorldZ"), lookAheadZ);
			outResponseInfo.Debug.LookAheadWorldLocationCm = FVector(lookAheadX, lookAheadY, lookAheadZ);
			outResponseInfo.Debug.bHasLookAheadWorldLocation = true;
		}

		ReadVectorArrayField(*debugObject, TEXT("pathWorldPoints"), outResponseInfo.Debug.PathWorldPointsCm);
	}

	if (!outResponseInfo.Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		outResponseInfo.ErrorMessage = TEXT("Policy response status is not ok.");
		return false;
	}

	if (!outResponseInfo.bHasAction)
	{
		outResponseInfo.ErrorMessage = TEXT("Policy response has no action object.");
		return false;
	}
	return true;
}

bool UDeliveryBot_HttpPolicyComponent::SendGridJson(const FString& gridJson)
{
	if (gridJson.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Grid JSON is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Grid request skipped because component is ending play."));
		return false;
	}

	if (bGridRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Grid request skipped because previous grid request is still in flight."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(GridServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(gridJson);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bGridRequestInFlight = false;
				component->ActiveGridRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Grid response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);

				component->OnGridResponse.Broadcast(bWasSuccessful, responseCode, responseBody);
			});
		}
	);

	bGridRequestInFlight = true;
	ActiveGridRequest = request;

	if (!request->ProcessRequest())
	{
		bGridRequestInFlight = false;
		ActiveGridRequest.Reset();
		return false;
	}

	return true;
}

bool UDeliveryBot_HttpPolicyComponent::SendEpisodeStartJson(const FString& episodeStartJson)
{
	if (episodeStartJson.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode start JSON is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode start request skipped because component is ending play."));
		return false;
	}

	if (bEpisodeStartRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode start request skipped because previous request is still in flight."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(EpisodeStartServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(episodeStartJson);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bEpisodeStartRequestInFlight = false;
				component->ActiveEpisodeStartRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Episode start response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);

				component->OnEpisodeStartResponse.Broadcast(bWasSuccessful, responseCode, responseBody);
			});
		}
	);

	bEpisodeStartRequestInFlight = true;
	ActiveEpisodeStartRequest = request;

	if (!request->ProcessRequest())
	{
		bEpisodeStartRequestInFlight = false;
		ActiveEpisodeStartRequest.Reset();
		return false;
	}

	return true;
}

bool UDeliveryBot_HttpPolicyComponent::SendEpisodeConfigUpdateJson(const FString& configUpdateJson)
{
	if (configUpdateJson.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode config update JSON is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode config update request skipped because component is ending play."));
		return false;
	}

	if (bEpisodeConfigUpdateRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Episode config update request skipped because previous request is still in flight."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(EpisodeConfigUpdateServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(configUpdateJson);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bEpisodeConfigUpdateRequestInFlight = false;
				component->ActiveEpisodeConfigUpdateRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Episode config update response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);

				component->OnEpisodeConfigUpdateResponse.Broadcast(bWasSuccessful, responseCode, responseBody);
			});
		}
	);

	bEpisodeConfigUpdateRequestInFlight = true;
	ActiveEpisodeConfigUpdateRequest = request;

	if (!request->ProcessRequest())
	{
		bEpisodeConfigUpdateRequestInFlight = false;
		ActiveEpisodeConfigUpdateRequest.Reset();
		return false;
	}

	return true;
}

bool UDeliveryBot_HttpPolicyComponent::RequestPolicyCatalogSources()
{
	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy catalog sources request skipped because component is ending play."));
		return false;
	}

	if (bPolicyCatalogSourcesRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy catalog sources request skipped because previous request is still in flight."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(PolicyCatalogSourcesServerUrl);
	request->SetVerb(TEXT("GET"));
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bPolicyCatalogSourcesRequestInFlight = false;
				component->ActivePolicyCatalogSourcesRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Policy catalog sources response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);

				component->OnPolicyCatalogSourcesResponse.Broadcast(bWasSuccessful, responseCode, responseBody);

				FDeliveryBotPolicyCatalogSourcesInfo sourcesInfo;
				sourcesInfo.bWasSuccessful = bWasSuccessful;
				sourcesInfo.ResponseCode = responseCode;
				sourcesInfo.RawResponseBody = responseBody;

				if (bWasSuccessful && responseCode >= 200 && responseCode < 300)
				{
					component->TryParsePolicyCatalogSourcesJson(responseBody, sourcesInfo);
				}
				else
				{
					sourcesInfo.ErrorMessage = TEXT("Policy catalog sources HTTP request failed.");
				}

				component->OnParsedPolicyCatalogSourcesResponse.Broadcast(sourcesInfo);
			});
		}
	);

	bPolicyCatalogSourcesRequestInFlight = true;
	ActivePolicyCatalogSourcesRequest = request;

	if (!request->ProcessRequest())
	{
		bPolicyCatalogSourcesRequestInFlight = false;
		ActivePolicyCatalogSourcesRequest.Reset();
		return false;
	}

	return true;
}

bool UDeliveryBot_HttpPolicyComponent::TryParsePolicyCatalogSourcesJson(const FString& responseBody,
	FDeliveryBotPolicyCatalogSourcesInfo& outSourcesInfo) const
{
	outSourcesInfo.RawResponseBody = responseBody;

	if (responseBody.IsEmpty())
	{
		outSourcesInfo.ErrorMessage = TEXT("Policy catalog sources response body is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outSourcesInfo.ErrorMessage = TEXT("Failed to parse policy catalog sources JSON.");
		return false;
	}

	rootObject->TryGetStringField(TEXT("status"), outSourcesInfo.Status);
	rootObject->TryGetStringField(TEXT("activeCatalogId"), outSourcesInfo.ActiveCatalogId);

	const TArray<TSharedPtr<FJsonValue>>* sourceValues = nullptr;
	if (!rootObject->TryGetArrayField(TEXT("sources"), sourceValues) || sourceValues == nullptr)
	{
		outSourcesInfo.ErrorMessage = TEXT("Policy catalog sources response has no sources array.");
		return false;
	}

	outSourcesInfo.Sources.Reset();

	for (const TSharedPtr<FJsonValue>& sourceValue : *sourceValues)
	{
		if (!sourceValue.IsValid())
			continue;

		const TSharedPtr<FJsonObject> sourceObject = sourceValue->AsObject();
		if (!sourceObject.IsValid())
			continue;

		FDeliveryBotPolicyCatalogSourceEntryInfo entryInfo;
		sourceObject->TryGetStringField(TEXT("catalogId"), entryInfo.CatalogId);
		sourceObject->TryGetNumberField(TEXT("catalogVersion"), entryInfo.CatalogVersion);
		sourceObject->TryGetStringField(TEXT("displayName"), entryInfo.DisplayName);
		sourceObject->TryGetStringField(TEXT("relativePath"), entryInfo.RelativePath);
		sourceObject->TryGetNumberField(TEXT("policyCount"), entryInfo.PolicyCount);

		if (entryInfo.CatalogId.IsEmpty())
			continue;

		outSourcesInfo.Sources.Add(entryInfo);
	}

	if (!outSourcesInfo.Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		outSourcesInfo.ErrorMessage = TEXT("Policy catalog sources status is not ok.");
		return false;
	}

	if (outSourcesInfo.Sources.IsEmpty())
	{
		outSourcesInfo.ErrorMessage = TEXT("Policy catalog sources list is empty.");
		return false;
	}

	return true;
}

// 사용자가 고른 catalogId를 Python에 보내고, 선택된 정책 catalog 전체를 응답으로 받음
bool UDeliveryBot_HttpPolicyComponent::RequestPolicyCatalogSource(const FString& catalogId)
{
	if (catalogId.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy catalog source request skipped because catalogId is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy catalog source request skipped because component is ending play."));
		return false;
	}

	if (bPolicyCatalogRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy catalog source request skipped because previous request is still in flight."));
		return false;
	}

	TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("catalogId"), catalogId);

	FString requestBody;
	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&requestBody);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Failed to serialize policy catalog source request body."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(PolicyCatalogSourceServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(requestBody);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bPolicyCatalogRequestInFlight = false;
				component->ActivePolicyCatalogRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				component->OnPolicyCatalogResponse.Broadcast(bWasSuccessful, responseCode, responseBody);

				FDeliveryBotPolicyCatalogInfo catalogInfo;
				catalogInfo.bWasSuccessful = bWasSuccessful;
				catalogInfo.ResponseCode = responseCode;
				catalogInfo.RawResponseBody = responseBody;

				if (bWasSuccessful && responseCode >= 200 && responseCode < 300)
				{
					component->TryParsePolicyCatalogJson(responseBody, catalogInfo);
				}
				else
				{
					catalogInfo.ErrorMessage = TEXT("Policy catalog HTTP request failed.");
				}

				component->OnParsedPolicyCatalogResponse.Broadcast(catalogInfo);
			});
		}
	);

	bPolicyCatalogRequestInFlight = true;
	ActivePolicyCatalogRequest = request;

	if (!request->ProcessRequest())
	{
		bPolicyCatalogRequestInFlight = false;
		ActivePolicyCatalogRequest.Reset();
		return false;
	}

	return true;
}

// Python 응답 JSON에서 policies 배열을 Unreal 구조체로 변환함
bool UDeliveryBot_HttpPolicyComponent::TryParsePolicyCatalogJson(const FString& responseBody, FDeliveryBotPolicyCatalogInfo& outCatalogInfo) const
{
	outCatalogInfo.RawResponseBody = responseBody;

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outCatalogInfo.ErrorMessage = TEXT("Failed to parse policy catalog JSON.");
		return false;
	}

	rootObject->TryGetStringField(TEXT("status"), outCatalogInfo.Status);
	rootObject->TryGetStringField(TEXT("activeCatalogId"), outCatalogInfo.ActiveCatalogId);
	rootObject->TryGetStringField(TEXT("catalogId"), outCatalogInfo.CatalogId);
	rootObject->TryGetNumberField(TEXT("catalogVersion"), outCatalogInfo.CatalogVersion);
	rootObject->TryGetStringField(TEXT("displayName"), outCatalogInfo.DisplayName);
	rootObject->TryGetStringField(TEXT("description"), outCatalogInfo.Description);

	const TArray<TSharedPtr<FJsonValue>>* policyValues = nullptr;
	if (!rootObject->TryGetArrayField(TEXT("policies"), policyValues) || policyValues == nullptr)
	{
		outCatalogInfo.ErrorMessage = TEXT("Policy catalog response has no policies array.");
		return false;
	}

	outCatalogInfo.Policies.Reset();

	for (const TSharedPtr<FJsonValue>& policyValue : *policyValues)
	{
		const TSharedPtr<FJsonObject> policyObject = policyValue.IsValid() ? policyValue->AsObject() : nullptr;
		if (!policyObject.IsValid())
			continue;

		FDeliveryBotPolicyCatalogPolicyInfo policyInfo;
		policyObject->TryGetStringField(TEXT("policyId"), policyInfo.PolicyId);
		policyObject->TryGetStringField(TEXT("displayName"), policyInfo.DisplayName);
		policyObject->TryGetStringField(TEXT("description"), policyInfo.Description);
		policyObject->TryGetStringField(TEXT("category"), policyInfo.Category);
		policyObject->TryGetBoolField(TEXT("defaultEnabled"), policyInfo.bDefaultEnabled);
		policyObject->TryGetNumberField(TEXT("defaultPriority"), policyInfo.DefaultPriority);
		policyObject->TryGetBoolField(TEXT("requiresGrid"), policyInfo.bRequiresGrid);
		policyObject->TryGetBoolField(TEXT("requiresGoal"), policyInfo.bRequiresGoal);

		if (!policyInfo.PolicyId.IsEmpty())
		{
			outCatalogInfo.Policies.Add(policyInfo);
		}
	}

	outCatalogInfo.bWasSuccessful = outCatalogInfo.Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase)
		&& !outCatalogInfo.Policies.IsEmpty();

	return outCatalogInfo.bWasSuccessful;
}

void UDeliveryBot_HttpPolicyComponent::CancelActiveGridRequest()
{
	if (ActiveGridRequest.IsValid())
	{
		ActiveGridRequest->OnProcessRequestComplete().Unbind();
		ActiveGridRequest->CancelRequest();
		ActiveGridRequest.Reset();
	}

	bGridRequestInFlight = false;
}

void UDeliveryBot_HttpPolicyComponent::CancelActiveEpisodeStartRequest()
{
	if (ActiveEpisodeStartRequest.IsValid())
	{
		ActiveEpisodeStartRequest->OnProcessRequestComplete().Unbind();
		ActiveEpisodeStartRequest->CancelRequest();
		ActiveEpisodeStartRequest.Reset();
	}

	bEpisodeStartRequestInFlight = false;
}

void UDeliveryBot_HttpPolicyComponent::CancelActiveEpisodeConfigUpdateRequest()
{
	if (ActiveEpisodeConfigUpdateRequest.IsValid())
	{
		ActiveEpisodeConfigUpdateRequest->OnProcessRequestComplete().Unbind();
		ActiveEpisodeConfigUpdateRequest->CancelRequest();
		ActiveEpisodeConfigUpdateRequest.Reset();
	}

	bEpisodeConfigUpdateRequestInFlight = false;
}

void UDeliveryBot_HttpPolicyComponent::CancelActivePolicyCatalogSourcesRequest()
{
	if (ActivePolicyCatalogSourcesRequest.IsValid())
	{
		ActivePolicyCatalogSourcesRequest->OnProcessRequestComplete().Unbind();
		ActivePolicyCatalogSourcesRequest->CancelRequest();
		ActivePolicyCatalogSourcesRequest.Reset();
	}

	bPolicyCatalogSourcesRequestInFlight = false;
}


void UDeliveryBot_HttpPolicyComponent::CancelActivePolicyCatalogRequest()
{
	if (ActivePolicyCatalogRequest.IsValid())
	{
		ActivePolicyCatalogRequest->OnProcessRequestComplete().Unbind();
		ActivePolicyCatalogRequest->CancelRequest();
		ActivePolicyCatalogRequest.Reset();
	}

	bPolicyCatalogRequestInFlight = false;
}

// 임시 값임
bool UDeliveryBot_HttpPolicyComponent::SendDefaultRuntimePolicySpecUpdate()
{
	const FString policySpecUpdateJson = TEXT(R"({
		"policySpec": {
			"catalogId": "default_delivery",
			"catalogVersion": 1,
			"enabledPolicies": [
				{ "policyId": "front_obstacle_stop", "priority": 10 },
				{ "policyId": "reroute_when_blocked", "priority": 20 },
				{ "policyId": "front_obstacle_slowdown", "priority": 30 },
				{ "policyId": "normal_path_follow", "priority": 100 }
			]
		}
	})");

	return SendPolicySpecUpdateJson(policySpecUpdateJson);
}

bool UDeliveryBot_HttpPolicyComponent::SendPolicySpecUpdateJson(const FString& policySpecUpdateJson)
{
	if (policySpecUpdateJson.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update JSON is empty."));
		return false;
	}

	if (bIsEndingPlay)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update skipped because component is ending play."));
		return false;
	}

	if (bPolicySpecUpdateRequestInFlight)
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update skipped because previous request is still in flight."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();

	request->SetURL(PolicySpecUpdateServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(policySpecUpdateJson);
	request->SetTimeout(RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString{};

			AsyncTask(ENamedThreads::GameThread, [weakThis, responseCode, responseBody, bWasSuccessful]()
			{
				if (!weakThis.IsValid())
					return;

				UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
				component->bPolicySpecUpdateRequestInFlight = false;
				component->ActivePolicySpecUpdateRequest.Reset();

				if (component->bIsEndingPlay)
					return;

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Log,
					TEXT("Policy spec update response | Success: %s, Code: %d, Body: %s"),
					bWasSuccessful ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody
				);

				component->OnPolicySpecUpdateResponse.Broadcast(bWasSuccessful, responseCode, responseBody);
			});
		}
	);

	bPolicySpecUpdateRequestInFlight = true;
	ActivePolicySpecUpdateRequest = request;

	if (!request->ProcessRequest())
	{
		bPolicySpecUpdateRequestInFlight = false;
		ActivePolicySpecUpdateRequest.Reset();
		return false;
	}

	return true;
}

void UDeliveryBot_HttpPolicyComponent::CancelActivePolicySpecUpdateRequest()
{
	if (ActivePolicySpecUpdateRequest.IsValid())
	{
		ActivePolicySpecUpdateRequest->OnProcessRequestComplete().Unbind();
		ActivePolicySpecUpdateRequest->CancelRequest();
		ActivePolicySpecUpdateRequest.Reset();
	}

	bPolicySpecUpdateRequestInFlight = false;
}

bool UDeliveryBot_HttpPolicyComponent::SendRuntimePolicySpecUpdateByPolicyIds(
	const FString& catalogId,
	int32 catalogVersion,
	const TArray<FString>& enabledPolicyIds
)
{
	if (catalogId.IsEmpty() || enabledPolicyIds.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update skipped. CatalogId or policy list is empty."));
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> policySpecObject = MakeShared<FJsonObject>();

	policySpecObject->SetStringField(TEXT("catalogId"), catalogId);
	policySpecObject->SetNumberField(TEXT("catalogVersion"), catalogVersion);

	TArray<TSharedPtr<FJsonValue>> enabledPoliciesArray;

	for (int32 index = 0; index < enabledPolicyIds.Num(); ++index)
	{
		const FString policyId = enabledPolicyIds[index].TrimStartAndEnd();

		if (policyId.IsEmpty())
		{
			continue;
		}

		TSharedRef<FJsonObject> policyObject = MakeShared<FJsonObject>();
		policyObject->SetStringField(TEXT("policyId"), policyId);
		policyObject->SetNumberField(TEXT("priority"), (index + 1) * 10);

		enabledPoliciesArray.Add(MakeShared<FJsonValueObject>(policyObject));
	}

	if (enabledPoliciesArray.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update skipped. No valid policy ids."));
		return false;
	}

	policySpecObject->SetArrayField(TEXT("enabledPolicies"), enabledPoliciesArray);
	rootObject->SetObjectField(TEXT("policySpec"), policySpecObject);

	FString policySpecUpdateJson;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&policySpecUpdateJson);

	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec update JSON serialization failed."));
		return false;
	}

	return SendPolicySpecUpdateJson(policySpecUpdateJson);
}

bool UDeliveryBot_HttpPolicyComponent::SendNormalOnlyRuntimePolicySpecUpdate()
{
	TArray<FString> enabledPolicyIds;
	enabledPolicyIds.Add(TEXT("normal_path_follow"));

	return SendRuntimePolicySpecUpdateByPolicyIds(
		TEXT("default_delivery"),
		1,
		enabledPolicyIds
	);
}

bool UDeliveryBot_HttpPolicyComponent::LoadPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath,	FString& outPolicySpecUpdateJson) const
{
	outPolicySpecUpdateJson.Reset();

	const FString resolvedFilePath = ResolvePolicySpecJsonFilePath(policySpecJsonFilePath);

	if (resolvedFilePath.IsEmpty())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec file path is empty."));
		return false;
	}

	if (!FPaths::FileExists(resolvedFilePath))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec file does not exist: %s"), *resolvedFilePath);
		return false;
	}

	if (!FFileHelper::LoadFileToString(outPolicySpecUpdateJson, *resolvedFilePath))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Failed to load policy spec file: %s"), *resolvedFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(outPolicySpecUpdateJson);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec file has invalid JSON: %s"), *resolvedFilePath);
		outPolicySpecUpdateJson.Reset();
		return false;
	}

	const TSharedPtr<FJsonObject>* policySpecObject = nullptr;
	if (!rootObject->TryGetObjectField(TEXT("policySpec"), policySpecObject)	|| policySpecObject == nullptr || !policySpecObject->IsValid())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Policy spec file has no policySpec object: %s"), *resolvedFilePath);
		outPolicySpecUpdateJson.Reset();
		return false;
	}

	return true;
}

bool UDeliveryBot_HttpPolicyComponent::SendPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath)
{
	FString policySpecUpdateJson;

	if (!LoadPolicySpecUpdateJsonFile(policySpecJsonFilePath, policySpecUpdateJson))
	{
		return false;
	}

	return SendPolicySpecUpdateJson(policySpecUpdateJson);
}




