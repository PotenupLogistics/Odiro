#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioLlmAuthoring, Log, All);

void UScenarioLlmAuthoringSubsystem::Deinitialize()
{
	CancelPendingRequest();
	Super::Deinitialize();
}

bool UScenarioLlmAuthoringSubsystem::GenerateProjectScenarioFromPrompt(
	const FString& prompt,
	const FString& projectScenarioJsonPath,
	const int32 episodeCount)
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
	if (!TryBuildRequestBody(prompt, projectScenarioJsonPath, episodeCount, requestBody, failureResult))
	{
		CompleteRequest(failureResult);
		return false;
	}
	PendingEpisodeCount = episodeCount > 0 ? episodeCount : DefaultEpisodeCount;

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
		PendingEpisodeCount = 0;
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
	PendingEpisodeCount = 0;
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

	FString scenarioJsonPath;
	FString responseMessage;
	FString runId;
	if (!TryReadScenarioPathFromResponse(responseBody, scenarioJsonPath, responseMessage, runId))
	{
		FScenarioLlmGenerationResult result;
		result.HttpStatusCode = responseCode;
		result.Message = responseMessage.IsEmpty()
			? TEXT("LLM generation response did not include a project scenario.json path.")
			: responseMessage;
		result.Diagnostics.Add(result.Message);
		if (!responseBody.IsEmpty())
		{
			result.Diagnostics.Add(FString::Printf(TEXT("Response: %s"), *TruncateForDiagnostic(responseBody)));
		}
		CompleteRequest(result);
		return;
	}

	FScenarioLlmGenerationResult result;
	result.bSuccess = true;
	result.HttpStatusCode = responseCode;
	result.Message = responseMessage.IsEmpty()
		? TEXT("LLM generation completed for project scenario.json.")
		: responseMessage;
	result.ProjectScenarioJsonPath = scenarioJsonPath;
	result.EpisodeCount = PendingEpisodeCount;
	result.RunId = runId;
	CompleteRequest(result);
}

void UScenarioLlmAuthoringSubsystem::CompleteRequest(const FScenarioLlmGenerationResult& result)
{
	LatestResult = result;
	PendingEpisodeCount = 0;
	if (result.bSuccess)
	{
		UE_LOG(
			LogScenarioLlmAuthoring,
			Log,
			TEXT("LLM generation completed | Episodes: %d | Scenario: %s | RunId: %s"),
			result.EpisodeCount,
			*result.ProjectScenarioJsonPath,
			*result.RunId);
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
	const FString& projectScenarioJsonPath,
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

	const FString resolvedScenarioJsonPath = ResolveProjectScenarioJsonPath(projectScenarioJsonPath);
	if (!IsProjectScenarioJsonPath(resolvedScenarioJsonPath))
	{
		outFailure.Message = TEXT("LLM generation requires a <UserProject>/scenario.json path.");
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

	const FString projectPath = FPaths::GetPath(resolvedScenarioJsonPath);
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("prompt"), trimmedPrompt);
	rootObject->SetStringField(TEXT("project_path"), projectPath);
	rootObject->SetStringField(TEXT("scenario_path"), resolvedScenarioJsonPath);
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

bool UScenarioLlmAuthoringSubsystem::TryReadScenarioPathFromResponse(
	const FString& responseBody,
	FString& outScenarioJsonPath,
	FString& outMessage,
	FString& outRunId)
{
	outScenarioJsonPath.Reset();
	outMessage.Reset();
	outRunId.Reset();

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outMessage = TEXT("LLM generation response was not valid JSON.");
		return false;
	}

	rootObject->TryGetStringField(TEXT("message"), outMessage);
	rootObject->TryGetStringField(TEXT("run_id"), outRunId);

	FString scenarioJsonPath;
	if (!rootObject->TryGetStringField(TEXT("scenario_path"), scenarioJsonPath))
	{
		rootObject->TryGetStringField(TEXT("project_scenario_path"), scenarioJsonPath);
	}
	if (scenarioJsonPath.IsEmpty())
	{
		const TSharedPtr<FJsonObject>* projectObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("project"), projectObject) && projectObject && projectObject->IsValid())
		{
			(*projectObject)->TryGetStringField(TEXT("scenario_path"), scenarioJsonPath);
		}
	}

	outScenarioJsonPath = ResolveProjectScenarioJsonPath(scenarioJsonPath);
	if (!IsProjectScenarioJsonPath(outScenarioJsonPath))
	{
		outMessage = TEXT("LLM generation response scenario_path must point to <UserProject>/scenario.json.");
		outScenarioJsonPath.Reset();
		return false;
	}

	return true;
}

FString UScenarioLlmAuthoringSubsystem::ResolveProjectScenarioJsonPath(const FString& projectScenarioJsonPath)
{
	FString resolvedPath = projectScenarioJsonPath.TrimStartAndEnd();
	resolvedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (resolvedPath.IsEmpty())
	{
		return FString();
	}

	if (FPaths::IsRelative(resolvedPath))
	{
		resolvedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), resolvedPath);
	}
	FPaths::NormalizeFilename(resolvedPath);
	return resolvedPath;
}

bool UScenarioLlmAuthoringSubsystem::IsProjectScenarioJsonPath(const FString& projectScenarioJsonPath)
{
	return FPaths::GetCleanFilename(projectScenarioJsonPath).Equals(TEXT("scenario.json"), ESearchCase::IgnoreCase);
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
