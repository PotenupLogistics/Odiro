#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/ScenarioDocumentJson.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioLlmAuthoring, Log, All);

namespace
{
	// Appends schema validation messages to the generation diagnostics shown in the editor.
	void AppendScenarioSchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics,
		TArray<FString>& outDiagnostics)
	{
		for (const FScenarioSchemaDiagnostic& diagnostic : schemaDiagnostics)
		{
			if (!diagnostic.Message.IsEmpty())
			{
				outDiagnostics.Add(diagnostic.Message);
			}
			else if (!diagnostic.Code.IsEmpty())
			{
				outDiagnostics.Add(diagnostic.Code);
			}
		}
	}
}

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
		result.Message = TEXT("이미 시나리오 생성 요청을 처리하고 있습니다.");
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
	PendingProjectScenarioJsonPath = ResolveProjectScenarioJsonPath(projectScenarioJsonPath);

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
		PendingProjectScenarioJsonPath.Reset();
		FScenarioLlmGenerationResult result;
		result.Message = TEXT("시나리오 생성 HTTP 요청을 시작하지 못했습니다.");
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
	PendingProjectScenarioJsonPath.Reset();
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
			TEXT("시나리오 생성 요청이 실패했습니다. HTTP %d"),
			responseCode);
		result.Diagnostics.Add(result.Message);
		if (!responseBody.IsEmpty())
		{
			result.Diagnostics.Add(FString::Printf(
				TEXT("응답: %s"),
				*TruncateForDiagnostic(responseBody)));
		}
		CompleteRequest(result);
		return;
	}

	const FString scenarioJsonPath = PendingProjectScenarioJsonPath;
	TArray<FString> diagnostics;
	if (!TryWriteScenarioResponseToProjectFile(responseBody, scenarioJsonPath, diagnostics))
	{
		FScenarioLlmGenerationResult result;
		result.HttpStatusCode = responseCode;
		result.Message = TEXT("시나리오 생성 응답에서 유효한 project scenario.json을 만들지 못했습니다.");
		result.Diagnostics.Add(result.Message);
		result.Diagnostics.Append(diagnostics);
		if (!responseBody.IsEmpty())
		{
			result.Diagnostics.Add(FString::Printf(TEXT("응답: %s"), *TruncateForDiagnostic(responseBody)));
		}
		CompleteRequest(result);
		return;
	}

	FScenarioLlmGenerationResult result;
	result.bSuccess = true;
	result.HttpStatusCode = responseCode;
	result.Message = TEXT("시나리오 생성이 완료되어 project scenario.json에 저장되었습니다.");
	result.ProjectScenarioJsonPath = scenarioJsonPath;
	result.EpisodeCount = PendingEpisodeCount;
	CompleteRequest(result);
}

void UScenarioLlmAuthoringSubsystem::CompleteRequest(const FScenarioLlmGenerationResult& result)
{
	LatestResult = result;
	PendingEpisodeCount = 0;
	PendingProjectScenarioJsonPath.Reset();
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
		outFailure.Message = TEXT("프롬프트를 입력해야 합니다.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	const FString resolvedScenarioJsonPath = ResolveProjectScenarioJsonPath(projectScenarioJsonPath);
	if (!IsProjectScenarioJsonPath(resolvedScenarioJsonPath))
	{
		outFailure.Message = TEXT("시나리오 생성에는 <UserProject>/scenario.json 경로가 필요합니다.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	(void)episodeCount;

	TArray<FString> diagnostics;
	if (!BuildScenarioGenerateV2RequestJson(trimmedPrompt, outBody, diagnostics))
	{
		outFailure.Message = diagnostics.IsEmpty()
			? TEXT("시나리오 생성 요청 JSON을 직렬화하지 못했습니다.")
			: diagnostics[0];
		outFailure.Diagnostics.Add(outFailure.Message);
		for (int32 index = 1; index < diagnostics.Num(); ++index)
		{
			outFailure.Diagnostics.Add(diagnostics[index]);
		}
		return false;
	}

	return true;
}

bool UScenarioLlmAuthoringSubsystem::BuildScenarioGenerateV2RequestJson(
	const FString& prompt,
	FString& outRequestJson,
	TArray<FString>& outDiagnostics)
{
	outRequestJson.Reset();
	outDiagnostics.Reset();

	const FString trimmedPrompt = prompt.TrimStartAndEnd();
	if (trimmedPrompt.IsEmpty())
	{
		outDiagnostics.Add(TEXT("프롬프트를 입력해야 합니다."));
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("prompt"), trimmedPrompt);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outRequestJson);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		outDiagnostics.Add(TEXT("시나리오 생성 요청 JSON을 직렬화하지 못했습니다."));
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

bool UScenarioLlmAuthoringSubsystem::TryWriteScenarioResponseToProjectFile(
	const FString& responseBody,
	const FString& projectScenarioJsonPath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	const FString resolvedScenarioJsonPath = ResolveProjectScenarioJsonPath(projectScenarioJsonPath);
	if (!IsProjectScenarioJsonPath(resolvedScenarioJsonPath))
	{
		outDiagnostics.Add(TEXT("시나리오 생성에는 <UserProject>/scenario.json 대상 경로가 필요합니다."));
		return false;
	}

	const FScenarioDocumentParseResult parseResult =
		FScenarioDocumentJson::ParseProjectScenarioFromString(responseBody);
	AppendScenarioSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("project scenario.json 검증에 실패했습니다."));
		return false;
	}

	const FString directory = FPaths::GetPath(resolvedScenarioJsonPath);
	if (!directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*directory, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("project scenario.json 디렉터리를 만들지 못했습니다: %s"), *directory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		responseBody,
		*resolvedScenarioJsonPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("생성된 project scenario.json을 저장하지 못했습니다: '%s'"), *resolvedScenarioJsonPath));
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
