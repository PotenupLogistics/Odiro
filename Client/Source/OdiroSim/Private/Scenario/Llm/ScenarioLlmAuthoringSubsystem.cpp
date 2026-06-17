#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/UserProjectDataTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioLlmAuthoring, Log, All);

namespace
{
	const TCHAR* ExpectedScenarioSchema = TEXT("scenario");

	void AppendScenarioDiagnostics(
		TArray<FString>& target,
		const TArray<FScenarioCompileDiagnostic>& source)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : source)
		{
			target.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}
	}
}

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
	if (!TryValidateAndSaveScenario(responseBody, responseCode, result))
	{
		CompleteRequest(result);
		return;
	}

	result.bSuccess = true;
	result.HttpStatusCode = responseCode;
	result.Message = FString::Printf(
		TEXT("Generated scenario saved: %s"),
		*result.SavedScenarioJsonPath);
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
			TEXT("LLM generation completed | ScenarioId: %s | Scenario: %s"),
			*result.ScenarioId,
			*result.SavedScenarioJsonPath);
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
	const int32 scenarioCount,
	FString& outBody,
	FScenarioLlmGenerationResult& outFailure) const
{
	(void)scenarioCount;
	outBody.Reset();
	outFailure = FScenarioLlmGenerationResult{};

	const FString trimmedPrompt = prompt.TrimStartAndEnd();
	if (trimmedPrompt.IsEmpty())
	{
		outFailure.Message = TEXT("Prompt must not be empty.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("prompt"), trimmedPrompt);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outBody);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		outFailure.Message = TEXT("Failed to serialize LLM generation request JSON.");
		outFailure.Diagnostics.Add(outFailure.Message);
		return false;
	}

	return true;
}

void UScenarioLlmAuthoringSubsystem::SetTargetProjectPath(const FString& projectPath)
{
	TargetProjectPath = NormalizePath(projectPath.TrimStartAndEnd());
}

FString UScenarioLlmAuthoringSubsystem::GetResolvedTargetProjectPath() const
{
	return ResolveTargetProjectPath();
}

bool UScenarioLlmAuthoringSubsystem::TryValidateAndSaveScenario(
	const FString& responseBody,
	const int32 responseCode,
	FScenarioLlmGenerationResult& outResult) const
{
	outResult = FScenarioLlmGenerationResult{};
	outResult.HttpStatusCode = responseCode;

	const FString targetProjectPath = ResolveTargetProjectPath();
	if (targetProjectPath.IsEmpty())
	{
		outResult.Message = TEXT("User project path is required before saving generated scenario.");
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}
	if (!FPaths::DirectoryExists(targetProjectPath))
	{
		outResult.Message = FString::Printf(TEXT("User project root does not exist: %s"), *targetProjectPath);
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}

	outResult.ResolvedSavedScenarioJsonPath = ResolveTargetScenarioPath();
	outResult.SavedScenarioJsonPath = outResult.ResolvedSavedScenarioJsonPath;

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
		outResult.Message = TEXT("LLM generation response is not valid scenario JSON.");
		outResult.Diagnostics.Add(outResult.Message);
		outResult.Diagnostics.Add(FString::Printf(
			TEXT("Response: %s"),
			*TruncateForDiagnostic(responseBody)));
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema) || schema != ExpectedScenarioSchema)
	{
		outResult.Diagnostics.Add(FString::Printf(
			TEXT("scenario schema must be '%s'."),
			ExpectedScenarioSchema));
	}

	int32 version = 0;
	if (!rootObject->TryGetNumberField(TEXT("version"), version) || version != 1)
	{
		outResult.Diagnostics.Add(TEXT("scenario version must be 1."));
	}

	if (!rootObject->TryGetStringField(TEXT("scenario_id"), outResult.ScenarioId) || outResult.ScenarioId.TrimStartAndEnd().IsEmpty())
	{
		outResult.Diagnostics.Add(TEXT("scenario_id must not be empty."));
	}

	const TCHAR* RequiredObjectFields[] = {
		TEXT("corridor"),
		TEXT("obstacles"),
		TEXT("pedestrians"),
		TEXT("robot"),
	};
	for (const TCHAR* fieldName : RequiredObjectFields)
	{
		const TSharedPtr<FJsonValue> fieldValue = rootObject->TryGetField(fieldName);
		if (!fieldValue.IsValid() || fieldValue->Type != EJson::Object)
		{
			outResult.Diagnostics.Add(FString::Printf(TEXT("%s must be an object."), fieldName));
		}
	}

	if (!outResult.Diagnostics.IsEmpty())
	{
		outResult.Message = TEXT("Generated scenario validation failed.");
		return false;
	}

	FString normalizedScenarioJson;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&normalizedScenarioJson);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outResult.Message = TEXT("Failed to serialize normalized scenario JSON.");
		outResult.Diagnostics.Add(outResult.Message);
		return false;
	}

	TArray<FScenarioCompileDiagnostic> saveDiagnostics;
	if (!FUserProjectDataJson::SaveRootJsonFile(
			outResult.ResolvedSavedScenarioJsonPath,
			normalizedScenarioJson,
			ExpectedScenarioSchema,
			saveDiagnostics))
	{
		AppendScenarioDiagnostics(outResult.Diagnostics, saveDiagnostics);
		outResult.Message = FString::Printf(
			TEXT("Failed to save generated scenario: %s"),
			*outResult.ResolvedSavedScenarioJsonPath);
		return false;
	}

	return true;
}

FString UScenarioLlmAuthoringSubsystem::ResolveTargetProjectPath() const
{
	FString projectPath = NormalizePath(TargetProjectPath.TrimStartAndEnd());
	if (projectPath.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("OdiroProject="), projectPath);
		projectPath = NormalizePath(projectPath.TrimStartAndEnd());
	}

	if (projectPath.IsEmpty())
	{
		return FString();
	}

	if (FPaths::IsRelative(projectPath))
	{
		projectPath = FPaths::ConvertRelativePathToFull(projectPath);
	}

	return NormalizePath(projectPath);
}

FString UScenarioLlmAuthoringSubsystem::ResolveTargetScenarioPath() const
{
	const FString projectPath = ResolveTargetProjectPath();
	if (projectPath.IsEmpty())
	{
		return FString();
	}

	return NormalizePath(FPaths::Combine(projectPath, TEXT("scenario.json")));
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

FString UScenarioLlmAuthoringSubsystem::NormalizePath(FString path)
{
	path.TrimStartAndEndInline();
	FPaths::NormalizeFilename(path);
	return path;
}
