#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioLlmAuthoring, Log, All);

void UScenarioLlmAuthoringSubsystem::Deinitialize()
{
	CancelPendingRequest();
	Super::Deinitialize();
}

bool UScenarioLlmAuthoringSubsystem::GenerateScenariosFromPrompt(const FString& prompt, const int32 scenarioCount)
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
	if (!TryBuildRequestBody(prompt, scenarioCount, requestBody, failureResult))
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
	result.HttpStatusCode = responseCode;
	result.Message = TEXT("Legacy LLM generation response is no longer supported. Use the scenario_template and experiment flow.");
	result.Diagnostics.Add(result.Message);
	if (!responseBody.IsEmpty())
	{
		result.Diagnostics.Add(FString::Printf(TEXT("Response: %s"), *TruncateForDiagnostic(responseBody)));
	}
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
			TEXT("LLM generation completed | Runs: %d | FirstScenario: %s"),
			result.RunCount,
			*result.FirstScenarioSourceJsonPath);
	}
	else
	{
		UE_LOG(
			LogScenarioLlmAuthoring,
			Warning,
			TEXT("LLM generation unavailable | HTTP: %d | Message: %s"),
			result.HttpStatusCode,
			*result.Message);
	}

	OnGenerationCompleted.Broadcast(LatestResult);
}

bool UScenarioLlmAuthoringSubsystem::TryBuildRequestBody(
	const FString& prompt,
	const int32 scenarioCount,
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

	const int32 resolvedScenarioCount = scenarioCount > 0 ? scenarioCount : DefaultScenarioCount;
	if (resolvedScenarioCount <= 0)
	{
		outFailure.Message = TEXT("Scenario count must be greater than zero.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("prompt"), trimmedPrompt);
	// Proto-AI currently expects this request key; rename it with the server contract.
	rootObject->SetNumberField(TEXT("episode_count"), resolvedScenarioCount);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outBody);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		outFailure.Message = TEXT("Failed to serialize LLM generation request JSON.");
		outFailure.Diagnostics.Add(outFailure.Message);
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
