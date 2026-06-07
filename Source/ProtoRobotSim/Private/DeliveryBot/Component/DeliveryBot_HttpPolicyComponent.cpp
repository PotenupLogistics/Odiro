#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

	OnPolicyResponse.Clear();
	OnParsedPolicyResponse.Clear();
	OnGridResponse.Clear();
	OnEpisodeStartResponse.Clear();
	OnEpisodeConfigUpdateResponse.Clear();
	
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

			UE_LOG(
				LogDeliveryBotHttpPolicy,
				Log,
				TEXT("Policy response | Success: %s, Code: %d, Body: %s"),
				bWasSuccessful ? TEXT("true") : TEXT("false"),
				responseCode,
				*responseBody
			);

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

