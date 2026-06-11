#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/SimulationSetupTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioLlmAuthoring, Log, All);

namespace
{
	const TCHAR* ExpectedRunQueueSchema = TEXT("episode_run_queue");
}

void UScenarioLlmAuthoringSubsystem::Deinitialize()
{
	CancelPendingRequest();
	Super::Deinitialize();
}

bool UScenarioLlmAuthoringSubsystem::GenerateEpisodeFromPrompt(const FString& prompt, const int32 episodeCount)
{
	if (PendingHttpRequest.IsValid())
	{
		FScenarioLlmGenerationResult result;
		result.Message = TEXT("LLM generation request is already pending.");
		result.Diagnostics.Add(result.Message);
		CompleteRequest(result);
		return false;
	}

	FString requestBody;
	FScenarioLlmGenerationResult failureResult;
	if (!TryBuildRequestBody(prompt, episodeCount, requestBody, failureResult))
	{
		CompleteRequest(failureResult);
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(BuildUrl(BaseUrl, GenerateEndpoint));
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	request->SetContentAsString(requestBody);
	request->SetTimeout(RequestTimeoutSeconds);

	TWeakObjectPtr<UScenarioLlmAuthoringSubsystem> weakThis = this;
	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			if (!weakThis.IsValid())
			{
				return;
			}

			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString();
			weakThis->HandleGenerateResponse(responseCode, responseBody, bWasSuccessful);
		});

	PendingHttpRequest = request;
	if (!request->ProcessRequest())
	{
		PendingHttpRequest.Reset();
		FScenarioLlmGenerationResult result;
		result.Message = TEXT("Failed to start LLM generation HTTP request.");
		result.Diagnostics.Add(result.Message);
		CompleteRequest(result);
		return false;
	}

	UE_LOG(LogScenarioLlmAuthoring, Log, TEXT("LLM generation request started | Url: %s"), *request->GetURL());
	return true;
}

void UScenarioLlmAuthoringSubsystem::CancelPendingRequest()
{
	if (!PendingHttpRequest.IsValid())
	{
		return;
	}

	PendingHttpRequest->OnProcessRequestComplete().Unbind();
	PendingHttpRequest->CancelRequest();
	PendingHttpRequest.Reset();
}

void UScenarioLlmAuthoringSubsystem::HandleGenerateResponse(
	const int32 responseCode,
	const FString& responseBody,
	const bool bWasSuccessful)
{
	PendingHttpRequest.Reset();

	if (!bWasSuccessful || responseCode < 200 || responseCode >= 300)
	{
		FScenarioLlmGenerationResult result;
		result.HttpStatusCode = responseCode;
		result.Message = FString::Printf(
			TEXT("LLM generation request failed. HTTP %d"),
			responseCode);
		result.Diagnostics.Add(result.Message);
		if (!responseBody.IsEmpty())
		{
			result.Diagnostics.Add(FString::Printf(
				TEXT("Response: %s"),
				*TruncateForDiagnostic(responseBody)));
		}
		CompleteRequest(result);
		return;
	}

	FScenarioLlmGenerationResult result;
	if (!TryValidateAndSaveRunQueue(responseBody, responseCode, result))
	{
		CompleteRequest(result);
		return;
	}

	result.bSuccess = true;
	result.HttpStatusCode = responseCode;
	result.Message = FString::Printf(
		TEXT("Generated RunQueue saved: %s"),
		*result.SavedRunQueueJsonPath);
	CompleteRequest(result);
}

void UScenarioLlmAuthoringSubsystem::CompleteRequest(const FScenarioLlmGenerationResult& result)
{
	LatestResult = result;
	if (result.bSuccess)
	{
		UE_LOG(
			LogScenarioLlmAuthoring,
			Log,
			TEXT("LLM generation completed | Runs: %d | RunQueue: %s"),
			result.RunCount,
			*result.SavedRunQueueJsonPath);
	}
	else
	{
		UE_LOG(
			LogScenarioLlmAuthoring,
			Warning,
			TEXT("LLM generation failed | HTTP: %d | Message: %s"),
			result.HttpStatusCode,
			*result.Message);
	}

	OnGenerationCompleted.Broadcast(LatestResult);
}

bool UScenarioLlmAuthoringSubsystem::TryBuildRequestBody(
	const FString& prompt,
	const int32 episodeCount,
	FString& outBody,
	FScenarioLlmGenerationResult& outFailure) const
{
	outBody.Reset();
	outFailure = FScenarioLlmGenerationResult{};

	const FString trimmedPrompt = prompt.TrimStartAndEnd();
	if (trimmedPrompt.IsEmpty())
	{
		outFailure.Message = TEXT("Prompt must not be empty.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	const int32 resolvedEpisodeCount = episodeCount > 0 ? episodeCount : DefaultEpisodeCount;
	if (resolvedEpisodeCount <= 0)
	{
		outFailure.Message = TEXT("Episode count must be greater than zero.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("prompt"), trimmedPrompt);
	rootObject->SetNumberField(TEXT("episode_count"), resolvedEpisodeCount);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outBody);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		outFailure.Message = TEXT("Failed to serialize LLM generation request JSON.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	return true;
}

bool UScenarioLlmAuthoringSubsystem::TryValidateAndSaveRunQueue(
	const FString& responseBody,
	const int32 responseCode,
	FScenarioLlmGenerationResult& outResult) const
{
	outResult = FScenarioLlmGenerationResult{};
	outResult.HttpStatusCode = responseCode;
	outResult.SavedRunQueueJsonPath = LatestRunQueueJsonPath;
	outResult.ResolvedSavedRunQueueJsonPath = FSimulationSetupJson::ResolveProjectPath(LatestRunQueueJsonPath);

	if (responseBody.TrimStartAndEnd().IsEmpty())
	{
		outResult.Message = TEXT("LLM generation response body is empty.");
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outResult.Message = TEXT("LLM generation response is not valid RunQueue JSON.");
		outResult.Diagnostics.Add(outResult.Message);
		outResult.Diagnostics.Add(FString::Printf(
			TEXT("Response: %s"),
			*TruncateForDiagnostic(responseBody)));
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema) || schema != ExpectedRunQueueSchema)
	{
		outResult.Diagnostics.Add(FString::Printf(
			TEXT("RunQueue schema must be '%s'."),
			ExpectedRunQueueSchema));
	}

	double version = 0.0;
	if (!rootObject->TryGetNumberField(TEXT("version"), version) || version < 1.0)
	{
		outResult.Diagnostics.Add(TEXT("RunQueue version must be >= 1."));
	}

	const TSharedPtr<FJsonValue> runsValue = rootObject->TryGetField(TEXT("runs"));
	if (!runsValue.IsValid() || runsValue->Type != EJson::Array)
	{
		outResult.Diagnostics.Add(TEXT("RunQueue runs must be an array."));
	}

	const TArray<TSharedPtr<FJsonValue>> runValues =
		runsValue.IsValid() && runsValue->Type == EJson::Array
			? runsValue->AsArray()
			: TArray<TSharedPtr<FJsonValue>>();
	outResult.RunCount = runValues.Num();
	if (runValues.IsEmpty())
	{
		outResult.Diagnostics.Add(TEXT("RunQueue must contain at least one run."));
	}

	for (int32 index = 0; index < runValues.Num(); ++index)
	{
		const TSharedPtr<FJsonValue>& runValue = runValues[index];
		if (!runValue.IsValid() || runValue->Type != EJson::Object)
		{
			outResult.Diagnostics.Add(FString::Printf(TEXT("runs[%d] must be an object."), index));
			continue;
		}

		const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
		if (!runObject.IsValid())
		{
			outResult.Diagnostics.Add(FString::Printf(TEXT("runs[%d] could not be read as an object."), index));
			continue;
		}

		FString episodeSetupPath;
		FString deliveryBotSetupPath;
		FString policySpecPath;
		runObject->TryGetStringField(TEXT("episode_setup"), episodeSetupPath);
		runObject->TryGetStringField(TEXT("delivery_bot_setup"), deliveryBotSetupPath);
		if (!runObject->TryGetStringField(TEXT("policy_spec"), policySpecPath))
		{
			runObject->TryGetStringField(TEXT("policy_spec_json_path"), policySpecPath);
		}
		episodeSetupPath = episodeSetupPath.TrimStartAndEnd();
		deliveryBotSetupPath = deliveryBotSetupPath.TrimStartAndEnd();
		policySpecPath = policySpecPath.TrimStartAndEnd();

		if (episodeSetupPath.IsEmpty())
		{
			outResult.Diagnostics.Add(FString::Printf(TEXT("runs[%d].episode_setup must not be empty."), index));
		}
		else
		{
			if (!episodeSetupPath.StartsWith(TEXT("Json/Input/")))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("runs[%d].episode_setup must start with Json/Input/: %s"),
					index,
					*episodeSetupPath));
			}

			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(episodeSetupPath)))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("EpisodeSetup file does not exist: %s"),
					*episodeSetupPath));
			}
		}

		if (deliveryBotSetupPath.IsEmpty())
		{
			outResult.Diagnostics.Add(FString::Printf(TEXT("runs[%d].delivery_bot_setup must not be empty."), index));
		}
		else
		{
			if (!deliveryBotSetupPath.StartsWith(TEXT("Json/Input/")))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("runs[%d].delivery_bot_setup must start with Json/Input/: %s"),
					index,
					*deliveryBotSetupPath));
			}

			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(deliveryBotSetupPath)))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("DeliveryBotSetup file does not exist: %s"),
					*deliveryBotSetupPath));
			}
		}

		if (!policySpecPath.IsEmpty())
		{
			if (!policySpecPath.StartsWith(TEXT("Json/Input/")))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("runs[%d].policy_spec must start with Json/Input/: %s"),
					index,
					*policySpecPath));
			}

			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(policySpecPath)))
			{
				outResult.Diagnostics.Add(FString::Printf(
					TEXT("PolicySpec file does not exist: %s"),
					*policySpecPath));
			}
		}

		if (index == 0)
		{
			outResult.FirstEpisodeSetupJsonPath = episodeSetupPath;
			outResult.FirstDeliveryBotSetupJsonPath = deliveryBotSetupPath;
		}
	}

	if (!outResult.Diagnostics.IsEmpty())
	{
		outResult.Message = TEXT("Generated RunQueue validation failed.");
		return false;
	}

	const FString outputFilePath = outResult.ResolvedSavedRunQueueJsonPath;
	const FString outputDirectory = FPaths::GetPath(outputFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		outResult.Message = FString::Printf(TEXT("Failed to create RunQueue output directory: %s"), *outputDirectory);
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
			responseBody,
			*outputFilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outResult.Message = FString::Printf(TEXT("Failed to save generated RunQueue: %s"), *outputFilePath);
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}

	return true;
}

FString UScenarioLlmAuthoringSubsystem::BuildUrl(const FString& baseUrl, const FString& endpoint)
{
	FString trimmedBaseUrl = baseUrl.TrimStartAndEnd();
	while (trimmedBaseUrl.EndsWith(TEXT("/")))
	{
		trimmedBaseUrl.LeftChopInline(1);
	}

	FString trimmedEndpoint = endpoint.TrimStartAndEnd();
	if (!trimmedEndpoint.StartsWith(TEXT("/")))
	{
		trimmedEndpoint = FString::Printf(TEXT("/%s"), *trimmedEndpoint);
	}

	return trimmedBaseUrl + trimmedEndpoint;
}

FString UScenarioLlmAuthoringSubsystem::TruncateForDiagnostic(const FString& value)
{
	constexpr int32 MaxDiagnosticChars = 1000;
	if (value.Len() <= MaxDiagnosticChars)
	{
		return value;
	}

	return value.Left(MaxDiagnosticChars) + TEXT("...");
}
