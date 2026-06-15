#include "Platform/PlatformRunAnalysisSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/ExperimentSettingTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlatformRunAnalysis, Log, All);

namespace
{
	const int32 RawResponsePreviewCharacterLimit = 6000;
	const int32 SupportedRunArtifactVersion = 1;

	FString JoinLines(const TArray<FString>& Lines)
	{
		return FString::Join(Lines, TEXT("\n"));
	}

	FString TruncateText(const FString& Text, int32 CharacterLimit)
	{
		if (Text.Len() <= CharacterLimit)
		{
			return Text;
		}

		return Text.Left(CharacterLimit) + TEXT("\n...");
	}

	FString NormalizeResolvedPath(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		FPaths::NormalizeFilename(Path);
		FPaths::CollapseRelativeDirectories(Path);
		return Path;
	}

	FString TryExtractKnownProjectRelativeTail(FString Path)
	{
		Path = NormalizeResolvedPath(Path);

		static const TCHAR* KnownRoots[] = {
			TEXT("Json/Experiments/"),
			TEXT("Saved/SimulationRuns/"),
			TEXT("Saved/AnalysisLogs/")
		};

		for (const TCHAR* KnownRoot : KnownRoots)
		{
			const int32 RootIndex = Path.Find(KnownRoot, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (RootIndex != INDEX_NONE)
			{
				return Path.Mid(RootIndex);
			}
		}

		return FString();
	}

	FString ResolveAnalysisPath(FString Path)
	{
		Path = Path.TrimStartAndEnd();
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));

		const FString ResolvedPath = NormalizeResolvedPath(FExperimentSettingJson::ResolveProjectPath(Path));
		if (FPaths::FileExists(ResolvedPath))
		{
			return ResolvedPath;
		}

		const FString ProjectRelativeTail = TryExtractKnownProjectRelativeTail(Path);
		if (!ProjectRelativeTail.IsEmpty())
		{
			return NormalizeResolvedPath(FPaths::Combine(FPaths::ProjectDir(), ProjectRelativeTail));
		}

		return ResolvedPath;
	}

	bool RequireExistingFile(
		const FString& FieldName,
		const FString& FilePath,
		TArray<FString>& OutDiagnostics)
	{
		if (FilePath.IsEmpty())
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s must not be empty."), *FieldName));
			return false;
		}

		if (!FPaths::FileExists(FilePath))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s file not found: %s"), *FieldName, *FilePath));
			return false;
		}

		return true;
	}

	bool ParseJsonObject(
		const FString& Json,
		const FString& Description,
		TSharedPtr<FJsonObject>& OutObject,
		TArray<FString>& OutDiagnostics)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s JSON parse failed."), *Description));
			return false;
		}

		return true;
	}

	bool ValidateSchemaAndVersion(
		const FJsonObject& Object,
		const FString& Description,
		const FString& ExpectedSchema,
		TArray<FString>& OutDiagnostics)
	{
		bool bValid = true;

		FString Schema;
		if (!Object.TryGetStringField(TEXT("schema"), Schema)
			|| !Schema.Equals(ExpectedSchema, ESearchCase::CaseSensitive))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s schema must be '%s'."), *Description, *ExpectedSchema));
			bValid = false;
		}

		double Version = 0.0;
		if (!Object.TryGetNumberField(TEXT("version"), Version)
			|| static_cast<int32>(Version) != SupportedRunArtifactVersion)
		{
			OutDiagnostics.Add(FString::Printf(
				TEXT("%s version must be %d."),
				*Description,
				SupportedRunArtifactVersion));
			bValid = false;
		}

		return bValid;
	}

	bool LoadSchemaObjectFile(
		const FString& FilePath,
		const FString& Description,
		const FString& ExpectedSchema,
		TSharedPtr<FJsonObject>& OutObject,
		TArray<FString>& OutDiagnostics)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("%s read failed: %s"), *Description, *FilePath));
			return false;
		}

		return ParseJsonObject(Json, Description, OutObject, OutDiagnostics)
			&& ValidateSchemaAndVersion(*OutObject, Description, ExpectedSchema, OutDiagnostics);
	}

	bool LoadEventJsonLines(
		const FString& FilePath,
		TArray<TSharedPtr<FJsonValue>>& OutEvents,
		TArray<FString>& OutDiagnostics)
	{
		OutEvents.Reset();

		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("episode_events read failed: %s"), *FilePath));
			return false;
		}

		bool bValid = true;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString Line = Lines[LineIndex].TrimStartAndEnd();
			if (Line.IsEmpty())
			{
				continue;
			}

			TSharedPtr<FJsonObject> EventObject;
			TArray<FString> EventDiagnostics;
			const FString Description = FString::Printf(TEXT("episode_events line %d"), LineIndex + 1);
			if (!ParseJsonObject(Line, Description, EventObject, EventDiagnostics)
				|| !ValidateSchemaAndVersion(*EventObject, Description, TEXT("episode_event"), EventDiagnostics))
			{
				for (const FString& Diagnostic : EventDiagnostics)
				{
					OutDiagnostics.Add(Diagnostic);
				}
				bValid = false;
				continue;
			}

			OutEvents.Add(MakeShared<FJsonValueObject>(EventObject));
		}

		return bValid;
	}

	FString GetRunDirectoryFromEpisodeResult(const FString& EpisodeResultPath)
	{
		const FString EpisodeDirectory = FPaths::GetPath(EpisodeResultPath);
		const FString EpisodesDirectory = FPaths::GetPath(EpisodeDirectory);
		return FPaths::GetPath(EpisodesDirectory);
	}

	bool TryGetObjectField(
		const FJsonObject& JsonObject,
		const FString& FieldName,
		TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();

		const TSharedPtr<FJsonValue> JsonValue = JsonObject.TryGetField(FieldName);
		if (!JsonValue.IsValid() || JsonValue->Type != EJson::Object)
		{
			return false;
		}

		OutObject = JsonValue->AsObject();
		return OutObject.IsValid();
	}

	bool TryGetStringFieldAny(
		const TSharedPtr<FJsonObject>& Object,
		const FString& SnakeFieldName,
		const FString& CamelFieldName,
		FString& OutValue)
	{
		OutValue.Reset();
		if (!Object.IsValid())
		{
			return false;
		}

		return Object->TryGetStringField(SnakeFieldName, OutValue)
			|| Object->TryGetStringField(CamelFieldName, OutValue);
	}

	FString JsonValueToDisplayString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			return TEXT("null");
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			return FString::SanitizeFloat(Value->AsNumber());
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Array:
			return TEXT("[...]");
		case EJson::Object:
			return TEXT("{...}");
		case EJson::None:
		case EJson::Null:
		default:
			break;
		}

		return TEXT("null");
	}

	void AppendStringFieldLine(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Label,
		const FString& FieldName,
		TArray<FString>& Lines)
	{
		if (!Object.IsValid())
		{
			return;
		}

		FString Value;
		if (Object->TryGetStringField(FieldName, Value) && !Value.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("%s: %s"), *Label, *Value));
		}
	}

	void AppendStringFieldLineAny(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Label,
		const FString& SnakeFieldName,
		const FString& CamelFieldName,
		TArray<FString>& Lines)
	{
		FString Value;
		if (TryGetStringFieldAny(Object, SnakeFieldName, CamelFieldName, Value) && !Value.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("%s: %s"), *Label, *Value));
		}
	}

	void AppendNumberFieldLine(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Label,
		const FString& FieldName,
		TArray<FString>& Lines)
	{
		if (!Object.IsValid())
		{
			return;
		}

		double Value = 0.0;
		if (Object->TryGetNumberField(FieldName, Value))
		{
			Lines.Add(FString::Printf(TEXT("%s: %.2f"), *Label, Value));
		}
	}

	void AppendFindings(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> FindingsValue = RootObject->TryGetField(TEXT("findings"));
		if (!FindingsValue.IsValid() || FindingsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Findings = FindingsValue->AsArray();
		if (Findings.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Findings"));

		for (const TSharedPtr<FJsonValue>& FindingValue : Findings)
		{
			if (!FindingValue.IsValid() || FindingValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Finding = FindingValue->AsObject();
			if (!Finding.IsValid())
			{
				continue;
			}

			FString Title;
			FString Message;
			Finding->TryGetStringField(TEXT("title"), Title);
			Finding->TryGetStringField(TEXT("message"), Message);
			const FString Prefix = Title.IsEmpty() ? TEXT("-") : FString::Printf(TEXT("- %s:"), *Title);
			Lines.Add(Message.IsEmpty() ? Prefix : FString::Printf(TEXT("%s %s"), *Prefix, *Message));
		}
	}

	void AppendSuggestions(
		const TSharedPtr<FJsonObject>& RootObject,
		const FString& FieldName,
		const FString& Title,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> SuggestionsValue = RootObject->TryGetField(FieldName);
		if (!SuggestionsValue.IsValid() || SuggestionsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Suggestions = SuggestionsValue->AsArray();
		if (Suggestions.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(Title);

		for (const TSharedPtr<FJsonValue>& SuggestionValue : Suggestions)
		{
			if (!SuggestionValue.IsValid() || SuggestionValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Suggestion = SuggestionValue->AsObject();
			if (!Suggestion.IsValid())
			{
				continue;
			}

			FString Param;
			FString Reason;
			Suggestion->TryGetStringField(TEXT("param"), Param);
			Suggestion->TryGetStringField(TEXT("reason"), Reason);

			const FString Current = JsonValueToDisplayString(Suggestion->TryGetField(TEXT("current")));
			const FString Suggested = JsonValueToDisplayString(Suggestion->TryGetField(TEXT("suggested")));
			Lines.Add(FString::Printf(TEXT("- %s: %s -> %s"), *Param, *Current, *Suggested));
			if (!Reason.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  %s"), *Reason));
			}
		}
	}

	void AppendWarnings(const TSharedPtr<FJsonObject>& RootObject, TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> WarningsValue = RootObject->TryGetField(TEXT("warnings"));
		if (!WarningsValue.IsValid() || WarningsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Warnings = WarningsValue->AsArray();
		if (Warnings.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Warnings"));
		for (const TSharedPtr<FJsonValue>& WarningValue : Warnings)
		{
			if (WarningValue.IsValid() && WarningValue->Type == EJson::String)
			{
				Lines.Add(FString::Printf(TEXT("- %s"), *WarningValue->AsString()));
			}
		}
	}

	FPlatformRunAnalysisResponse MakeResponse(
		bool bSuccess,
		int32 ResponseCode,
		const FString& DisplayText,
		const FString& ErrorMessage,
		const FString& ResponseBody)
	{
		FPlatformRunAnalysisResponse Response;
		Response.bSuccess = bSuccess;
		Response.ResponseCode = ResponseCode;
		Response.DisplayText = DisplayText;
		Response.ErrorMessage = ErrorMessage;
		Response.ResponseBody = ResponseBody;
		return Response;
	}
}

void UPlatformRunAnalysisSubsystem::Deinitialize()
{
	CancelPendingAnalysisRequest();
	OnAnalysisCompleted.Clear();
	Super::Deinitialize();
}

bool UPlatformRunAnalysisSubsystem::RequestAnalysisForEpisodeResult(const FString& episodeResultPath)
{
	if (PendingHttpRequest.IsValid())
	{
		BroadcastFailure(0, TEXT("An AI analysis request is already pending."));
		return false;
	}

	if (AnalysisEndpointUrl.TrimStartAndEnd().IsEmpty())
	{
		BroadcastFailure(0, TEXT("AI analysis endpoint URL is empty."));
		return false;
	}

	FString RequestJson;
	TArray<FString> Diagnostics;
	if (!BuildAnalysisRequestJsonFromEpisodeResult(episodeResultPath, bFallbackOnly, RequestJson, Diagnostics))
	{
		BroadcastFailure(0, JoinLines(Diagnostics));
		return false;
	}

	PendingRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(AnalysisEndpointUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestJson);
	Request->SetTimeout(RequestTimeoutSeconds);

	TWeakObjectPtr<UPlatformRunAnalysisSubsystem> WeakThis = this;
	const FString CapturedRequestId = PendingRequestId;
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, CapturedRequestId](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandleAnalysisResponse(CapturedRequestId, HttpRequest, HttpResponse, bWasSuccessful);
		});

	PendingHttpRequest = Request;
	if (!Request->ProcessRequest())
	{
		PendingHttpRequest.Reset();
		PendingRequestId.Reset();
		BroadcastFailure(0, TEXT("Failed to start AI analysis request."));
		return false;
	}

	UE_LOG(
		LogPlatformRunAnalysis,
		Log,
		TEXT("AI run analysis request started | Url: %s, EpisodeResult: %s"),
		*AnalysisEndpointUrl,
		*episodeResultPath);

	return true;
}

void UPlatformRunAnalysisSubsystem::CancelPendingAnalysisRequest()
{
	if (!PendingHttpRequest.IsValid())
	{
		return;
	}

	PendingHttpRequest->OnProcessRequestComplete().Unbind();
	PendingHttpRequest->CancelRequest();
	PendingHttpRequest.Reset();
	PendingRequestId.Reset();
}

bool UPlatformRunAnalysisSubsystem::BuildAnalysisRequestJsonFromEpisodeResult(
	const FString& episodeResultPath,
	bool bFallbackOnly,
	FString& outRequestJson,
	TArray<FString>& outDiagnostics)
{
	outRequestJson.Reset();
	outDiagnostics.Reset();

	const FString ResolvedResultPath = ResolveAnalysisPath(episodeResultPath);
	RequireExistingFile(TEXT("episode_result_path"), ResolvedResultPath, outDiagnostics);
	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	const FString RunDirectory = GetRunDirectoryFromEpisodeResult(ResolvedResultPath);
	const FString ResolvedSummaryPath = NormalizeResolvedPath(FPaths::Combine(RunDirectory, TEXT("summary.json")));
	const FString ResolvedEventsPath = NormalizeResolvedPath(FPaths::Combine(FPaths::GetPath(ResolvedResultPath), TEXT("events.jsonl")));

	RequireExistingFile(TEXT("run_summary_path"), ResolvedSummaryPath, outDiagnostics);
	RequireExistingFile(TEXT("episode_events_path"), ResolvedEventsPath, outDiagnostics);
	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> SummaryObject;
	TSharedPtr<FJsonObject> ResultObject;
	TArray<TSharedPtr<FJsonValue>> EventValues;
	LoadSchemaObjectFile(ResolvedSummaryPath, TEXT("run_summary"), TEXT("run_summary"), SummaryObject, outDiagnostics);
	LoadSchemaObjectFile(ResolvedResultPath, TEXT("episode_result"), TEXT("episode_result"), ResultObject, outDiagnostics);
	LoadEventJsonLines(ResolvedEventsPath, EventValues, outDiagnostics);
	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	TSharedRef<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("schema"), TEXT("run_analysis_request"));
	RequestObject->SetNumberField(TEXT("version"), 1);
	RequestObject->SetStringField(TEXT("run_summary_path"), ResolvedSummaryPath);
	RequestObject->SetStringField(TEXT("episode_result_path"), ResolvedResultPath);
	RequestObject->SetStringField(TEXT("episode_events_path"), ResolvedEventsPath);
	RequestObject->SetBoolField(TEXT("fallback_only"), bFallbackOnly);
	RequestObject->SetObjectField(TEXT("run_summary"), SummaryObject.ToSharedRef());
	RequestObject->SetObjectField(TEXT("episode_result"), ResultObject.ToSharedRef());
	RequestObject->SetArrayField(TEXT("episode_events"), EventValues);

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outRequestJson);
	if (!FJsonSerializer::Serialize(RequestObject, Writer))
	{
		outDiagnostics.Add(TEXT("AI run analysis request JSON serialization failed."));
		return false;
	}

	return true;
}

FString UPlatformRunAnalysisSubsystem::BuildDisplayTextFromAnalysisResponse(
	const FString& responseBody,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(responseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		outDiagnostics.Add(TEXT("AI run analysis response JSON parse failed."));
		return TruncateText(responseBody, RawResponsePreviewCharacterLimit);
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("Run Analysis"));
	AppendStringFieldLineAny(RootObject, TEXT("Analysis Id"), TEXT("analysis_id"), TEXT("analysisId"), Lines);
	AppendStringFieldLineAny(RootObject, TEXT("Generation"), TEXT("generation_method"), TEXT("generationMethod"), Lines);

	FString Summary;
	if (RootObject->TryGetStringField(TEXT("summary"), Summary) && !Summary.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Summary"));
		Lines.Add(Summary);
	}

	TSharedPtr<FJsonObject> EpisodeStatistics;
	if (TryGetObjectField(*RootObject, TEXT("episode_statistics"), EpisodeStatistics))
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Episode Result"));
		AppendStringFieldLine(EpisodeStatistics, TEXT("Outcome"), TEXT("outcome"), Lines);
		AppendStringFieldLine(EpisodeStatistics, TEXT("Terminal"), TEXT("terminal_reason"), Lines);
		AppendNumberFieldLine(EpisodeStatistics, TEXT("Duration(s)"), TEXT("duration_s"), Lines);
	}

	AppendFindings(RootObject, Lines);
	AppendSuggestions(RootObject, TEXT("scenario_template_suggestions"), TEXT("Scenario Template Suggestions"), Lines);
	AppendSuggestions(RootObject, TEXT("profile_suggestions"), TEXT("Profile Suggestions"), Lines);
	AppendSuggestions(RootObject, TEXT("runtime_suggestions"), TEXT("Runtime Suggestions"), Lines);
	AppendWarnings(RootObject, Lines);

	return JoinLines(Lines);
}

void UPlatformRunAnalysisSubsystem::HandleAnalysisResponse(
	const FString& requestId,
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> httpRequest,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> httpResponse,
	bool bWasSuccessful)
{
	(void)httpRequest;

	if (!requestId.Equals(PendingRequestId, ESearchCase::CaseSensitive))
	{
		return;
	}

	PendingHttpRequest.Reset();
	PendingRequestId.Reset();

	const int32 ResponseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
	const FString ResponseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString();
	if (!bWasSuccessful)
	{
		BroadcastFailure(ResponseCode, TEXT("AI run analysis HTTP request failed."), ResponseBody);
		return;
	}

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		const FString Message = FString::Printf(TEXT("AI run analysis HTTP error: %d"), ResponseCode);
		BroadcastFailure(ResponseCode, Message, ResponseBody);
		return;
	}

	TArray<FString> Diagnostics;
	const FString DisplayText = BuildDisplayTextFromAnalysisResponse(ResponseBody, Diagnostics);
	if (!Diagnostics.IsEmpty())
	{
		UE_LOG(LogPlatformRunAnalysis, Warning, TEXT("AI run analysis response diagnostic | %s"), *JoinLines(Diagnostics));
	}

	OnAnalysisCompleted.Broadcast(MakeResponse(true, ResponseCode, DisplayText, FString(), ResponseBody));
}

void UPlatformRunAnalysisSubsystem::BroadcastFailure(
	int32 responseCode,
	const FString& message,
	const FString& responseBody)
{
	UE_LOG(LogPlatformRunAnalysis, Warning, TEXT("AI run analysis failed | Code: %d, Message: %s"), responseCode, *message);
	OnAnalysisCompleted.Broadcast(MakeResponse(false, responseCode, FString(), message, responseBody));
}
