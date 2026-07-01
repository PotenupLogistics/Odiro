#include "Platform/PlatformAnalysisAiSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/SimulationSetupTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlatformAnalysisAi, Log, All);

namespace
{
	const int32 RawResponsePreviewCharacterLimit = 6000;

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

	void AppendStringArraySection(
		const TSharedPtr<FJsonObject>& RootObject,
		const FString& FieldName,
		const FString& Title,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> ArrayValue = RootObject->TryGetField(FieldName);
		if (!ArrayValue.IsValid() || ArrayValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Values = ArrayValue->AsArray();
		if (Values.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(Title);
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				Lines.Add(FString::Printf(TEXT("- %s"), *Value->AsString()));
			}
		}
	}

	void AppendAnalysisV2Metrics(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		TSharedPtr<FJsonObject> MetricsObject;
		if (!RootObject.IsValid() || !TryGetObjectField(*RootObject, TEXT("metrics"), MetricsObject))
		{
			return;
		}

		const int32 StartLineCount = Lines.Num();
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Metrics"));
		AppendNumberFieldLine(MetricsObject, TEXT("Success"), TEXT("success_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Failure"), TEXT("failure_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Collision"), TEXT("collision_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Static Obstacle Collision"), TEXT("static_obstacle_collision_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Pedestrian Collision"), TEXT("pedestrian_collision_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Near Miss"), TEXT("near_miss_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Repath"), TEXT("repath_count"), Lines);
		AppendNumberFieldLine(MetricsObject, TEXT("Tip Over"), TEXT("robot_tip_over_count"), Lines);

		if (Lines.Num() == StartLineCount + 2)
		{
			Lines.SetNum(StartLineCount);
		}
	}

	FString ReadStringField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& DefaultValue = FString())
	{
		FString Value;
		return Object.TryGetStringField(FieldName, Value) ? Value.TrimStartAndEnd() : DefaultValue;
	}

	FString ReadDisplayString(
		const FJsonObject& Object,
		const FString& FieldName)
	{
		TSharedPtr<FJsonObject> DisplayObject;
		return TryGetObjectField(Object, TEXT("display"), DisplayObject)
			? ReadStringField(*DisplayObject, FieldName)
			: FString();
	}

	void AppendDisplayFieldLine(
		const FJsonObject& Object,
		const FString& Label,
		const FString& FieldName,
		TArray<FString>& Lines)
	{
		const FString DisplayText = ReadDisplayString(Object, FieldName);
		if (!DisplayText.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("%s: %s"), *Label, *DisplayText));
		}
	}

	void AppendAnalysisV2RunOverview(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		TSharedPtr<FJsonObject> RunOverviewObject;
		if (!RootObject.IsValid() || !TryGetObjectField(*RootObject, TEXT("run_overview"), RunOverviewObject))
		{
			return;
		}

		const int32 StartLineCount = Lines.Num();
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Run Overview"));
		AppendDisplayFieldLine(*RunOverviewObject, TEXT("Total Play Time"), TEXT("total_play_time"), Lines);
		AppendDisplayFieldLine(*RunOverviewObject, TEXT("Success Rate"), TEXT("success_rate"), Lines);
		AppendDisplayFieldLine(*RunOverviewObject, TEXT("Collision Count"), TEXT("collision_count"), Lines);

		if (Lines.Num() == StartLineCount + 2)
		{
			Lines.SetNum(StartLineCount);
			AppendAnalysisV2Metrics(RootObject, Lines);
		}
	}

	void AppendAnalysisV2Insights(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> InsightsValue = RootObject->TryGetField(TEXT("insights"));
		if (!InsightsValue.IsValid() || InsightsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Insights = InsightsValue->AsArray();
		if (Insights.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Insights"));
		for (const TSharedPtr<FJsonValue>& InsightValue : Insights)
		{
			if (!InsightValue.IsValid() || InsightValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> InsightObject = InsightValue->AsObject();
			if (!InsightObject.IsValid())
			{
				continue;
			}

			const FString Severity = ReadStringField(*InsightObject, TEXT("severity"));
			const FString Title = ReadStringField(*InsightObject, TEXT("title"));
			const FString Description = ReadStringField(*InsightObject, TEXT("description"));
			const FString TagText = Severity.IsEmpty() ? FString() : FString::Printf(TEXT("[%s] "), *Severity);
			if (!Title.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("- %s%s"), *TagText, *Title));
			}
			if (!Description.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  %s"), *Description));
			}
		}
	}

	void AppendAnalysisV2Episodes(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> EpisodesValue = RootObject->TryGetField(TEXT("episodes"));
		if (!EpisodesValue.IsValid() || EpisodesValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Episodes = EpisodesValue->AsArray();
		if (Episodes.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Episodes"));
		for (const TSharedPtr<FJsonValue>& EpisodeValue : Episodes)
		{
			if (!EpisodeValue.IsValid() || EpisodeValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> EpisodeObject = EpisodeValue->AsObject();
			if (!EpisodeObject.IsValid())
			{
				continue;
			}

			TArray<FString> Parts;
			const FString EpisodeId = ReadStringField(*EpisodeObject, TEXT("episode_id"));
			const FString Outcome = ReadDisplayString(*EpisodeObject, TEXT("outcome"));
			const FString Duration = ReadDisplayString(*EpisodeObject, TEXT("duration"));
			if (!EpisodeId.IsEmpty())
			{
				Parts.Add(EpisodeId);
			}
			if (!Outcome.IsEmpty())
			{
				Parts.Add(Outcome);
			}
			if (!Duration.IsEmpty())
			{
				Parts.Add(Duration);
			}
			if (!Parts.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("- %s"), *FString::Join(Parts, TEXT(" | "))));
			}
		}
	}

	void AppendAnalysisV2Recommendations(
		const TSharedPtr<FJsonObject>& RootObject,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> RecommendationsValue = RootObject->TryGetField(TEXT("recommendations"));
		if (!RecommendationsValue.IsValid() || RecommendationsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Recommendations = RecommendationsValue->AsArray();
		if (Recommendations.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Recommendations"));
		for (const TSharedPtr<FJsonValue>& RecommendationValue : Recommendations)
		{
			if (!RecommendationValue.IsValid() || RecommendationValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Recommendation = RecommendationValue->AsObject();
			if (!Recommendation.IsValid())
			{
				continue;
			}

			FString Title;
			FString Target;
			FString Priority;
			FString Reason;
			FString RecommendationText;
			Recommendation->TryGetStringField(TEXT("title"), Title);
			Recommendation->TryGetStringField(TEXT("target"), Target);
			Recommendation->TryGetStringField(TEXT("priority"), Priority);
			Recommendation->TryGetStringField(TEXT("reason"), Reason);
			Recommendation->TryGetStringField(TEXT("recommendation"), RecommendationText);

			TArray<FString> Tags;
			if (!Priority.IsEmpty())
			{
				Tags.Add(Priority);
			}
			if (!Target.IsEmpty())
			{
				Tags.Add(Target);
			}

			const FString TagText = Tags.IsEmpty() ? FString() : FString::Printf(TEXT("[%s] "), *FString::Join(Tags, TEXT(", ")));
			const FString DisplayTitle = Title.IsEmpty() ? FString(TEXT("Recommendation")) : Title;
			Lines.Add(FString::Printf(TEXT("- %s%s"), *TagText, *DisplayTitle));
			if (!Reason.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  Reason: %s"), *Reason));
			}
			if (!RecommendationText.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  Recommendation: %s"), *RecommendationText));
			}
		}
	}

	bool IsAnalysisV2Response(const TSharedPtr<FJsonObject>& RootObject)
	{
		if (!RootObject.IsValid())
		{
			return false;
		}

		TSharedPtr<FJsonObject> SummaryObject;
		return RootObject->HasField(TEXT("status"))
			|| RootObject->HasField(TEXT("review_id"))
			|| RootObject->HasField(TEXT("recommendation_type"))
			|| RootObject->HasField(TEXT("run_overview"))
			|| RootObject->HasField(TEXT("metrics"))
			|| RootObject->HasField(TEXT("insights"))
			|| RootObject->HasField(TEXT("warnings"))
			|| TryGetObjectField(*RootObject, TEXT("summary"), SummaryObject);
	}

	FString BuildAnalysisV2DisplayText(const TSharedPtr<FJsonObject>& RootObject)
	{
		TArray<FString> Lines;

		const FString Status = ReadStringField(*RootObject, TEXT("status"));
		if (Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase))
		{
			Lines.Add(TEXT("AI Analysis Failed"));
			TSharedPtr<FJsonObject> ErrorObject;
			if (TryGetObjectField(*RootObject, TEXT("error"), ErrorObject))
			{
				AppendStringFieldLine(ErrorObject, TEXT("Message"), TEXT("message"), Lines);
			}
			AppendStringArraySection(RootObject, TEXT("warnings"), TEXT("Warnings"), Lines);
			return JoinLines(Lines);
		}

		Lines.Add(TEXT("AI Analysis"));
		AppendStringFieldLine(RootObject, TEXT("Review Id"), TEXT("review_id"), Lines);
		AppendStringFieldLine(RootObject, TEXT("Run Id"), TEXT("run_id"), Lines);

		TSharedPtr<FJsonObject> SummaryObject;
		if (TryGetObjectField(*RootObject, TEXT("summary"), SummaryObject))
		{
			Lines.Add(TEXT(""));
			Lines.Add(TEXT("Summary"));
			AppendStringFieldLine(SummaryObject, TEXT("Message"), TEXT("message"), Lines);
		}

		AppendAnalysisV2RunOverview(RootObject, Lines);
		AppendAnalysisV2Insights(RootObject, Lines);
		AppendAnalysisV2Episodes(RootObject, Lines);
		AppendAnalysisV2Recommendations(RootObject, Lines);
		AppendStringArraySection(RootObject, TEXT("warnings"), TEXT("Warnings"), Lines);
		return JoinLines(Lines);
	}

	void AppendRecommendations(
		const TSharedPtr<FJsonObject>& RootObject,
		const FString& FieldName,
		const FString& Title,
		TArray<FString>& Lines)
	{
		if (!RootObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> RecommendationsValue = RootObject->TryGetField(FieldName);
		if (!RecommendationsValue.IsValid() || RecommendationsValue->Type != EJson::Array)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>& Recommendations = RecommendationsValue->AsArray();
		if (Recommendations.IsEmpty())
		{
			return;
		}

		Lines.Add(TEXT(""));
		Lines.Add(Title);

		for (const TSharedPtr<FJsonValue>& RecommendationValue : Recommendations)
		{
			if (!RecommendationValue.IsValid() || RecommendationValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Recommendation = RecommendationValue->AsObject();
			if (!Recommendation.IsValid())
			{
				continue;
			}

			FString Param;
			FString Reason;
			Recommendation->TryGetStringField(TEXT("param"), Param);
			Recommendation->TryGetStringField(TEXT("reason"), Reason);

			const FString Current = JsonValueToDisplayString(Recommendation->TryGetField(TEXT("current")));
			const FString Suggested = JsonValueToDisplayString(Recommendation->TryGetField(TEXT("suggested")));
			Lines.Add(FString::Printf(TEXT("- %s: %s -> %s"), *Param, *Current, *Suggested));
			if (!Reason.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  %s"), *Reason));
			}
		}
	}

	void AppendWarnings(const TSharedPtr<FJsonObject>& RootObject, TArray<FString>& Lines)
	{
		AppendStringArraySection(RootObject, TEXT("llmWarnings"), TEXT("Warnings"), Lines);
	}

	FPlatformAnalysisAiResponse MakeResponse(
		bool bSuccess,
		int32 ResponseCode,
		const FString& DisplayText,
		const FString& ErrorMessage,
		const FString& ResponseBody,
		const FString& ProjectPath = FString(),
		const FString& RunId = FString(),
		const FString& RunDirectory = FString(),
		const FString& ReviewOutputPath = FString())
	{
		FPlatformAnalysisAiResponse Response;
		Response.bSuccess = bSuccess;
		Response.ResponseCode = ResponseCode;
		Response.DisplayText = DisplayText;
		Response.ErrorMessage = ErrorMessage;
		Response.ResponseBody = ResponseBody;
		Response.ProjectPath = ProjectPath;
		Response.RunId = RunId;
		Response.RunDirectory = RunDirectory;
		Response.ReviewOutputPath = ReviewOutputPath;
		return Response;
	}
}

void UPlatformAnalysisAiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString EndpointUrl;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProjectRunAnalysisEndpointUrl="), EndpointUrl))
	{
		EndpointUrl = EndpointUrl.TrimStartAndEnd();
		if (!EndpointUrl.IsEmpty())
		{
			ProjectRunAnalysisEndpointUrl = EndpointUrl;
		}
	}
}

void UPlatformAnalysisAiSubsystem::Deinitialize()
{
	CancelPendingAnalysisRequest();
	OnAnalysisCompleted.Clear();
	Super::Deinitialize();
}

bool UPlatformAnalysisAiSubsystem::RequestAnalysisForProjectRun(
	const FString& projectPath,
	const FString& runId)
{
	if (PendingHttpRequest.IsValid())
	{
		BroadcastFailure(0, TEXT("An AI analysis request is already pending."));
		return false;
	}

	if (ProjectRunAnalysisEndpointUrl.TrimStartAndEnd().IsEmpty())
	{
		BroadcastFailure(0, TEXT("Project run AI analysis endpoint URL is empty."));
		return false;
	}

	FString RequestJson;
	TArray<FString> Diagnostics;
	if (!BuildAnalysisRequestJsonForProjectRun(projectPath, runId, RequestJson, Diagnostics))
	{
		BroadcastFailure(0, JoinLines(Diagnostics));
		return false;
	}

	const FUserProjectRunSnapshotPaths Paths = FUserProjectRunSnapshot::BuildPaths(projectPath, runId);
	PendingRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	PendingProjectPath = Paths.ProjectPath;
	PendingRunId = Paths.RunId;
	PendingRunDirectory = Paths.RunPath;
	PendingReviewOutputPath = FPaths::Combine(Paths.ReviewPath, TEXT("analysis_run_response_v2.json"));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ProjectRunAnalysisEndpointUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestJson);
	Request->SetTimeout(RequestTimeoutSeconds);

	TWeakObjectPtr<UPlatformAnalysisAiSubsystem> WeakThis = this;
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
		const FString ProjectPathContext = PendingProjectPath;
		const FString RunIdContext = PendingRunId;
		const FString RunDirectoryContext = PendingRunDirectory;
		const FString ReviewOutputPathContext = PendingReviewOutputPath;
		PendingHttpRequest.Reset();
		PendingRequestId.Reset();
		PendingProjectPath.Reset();
		PendingRunId.Reset();
		PendingRunDirectory.Reset();
		PendingReviewOutputPath.Reset();
		BroadcastFailure(
			0,
			TEXT("Failed to start project run AI analysis request."),
			FString(),
			ProjectPathContext,
			RunIdContext,
			RunDirectoryContext,
			ReviewOutputPathContext);
		return false;
	}

	UE_LOG(
		LogPlatformAnalysisAi,
		Log,
		TEXT("Project run AI analysis request started | Url: %s, Project: %s, RunId: %s"),
		*ProjectRunAnalysisEndpointUrl,
		*projectPath,
		*runId);

	return true;
}

void UPlatformAnalysisAiSubsystem::CancelPendingAnalysisRequest()
{
	if (!PendingHttpRequest.IsValid())
	{
		return;
	}

	PendingHttpRequest->OnProcessRequestComplete().Unbind();
	PendingHttpRequest->CancelRequest();
	PendingHttpRequest.Reset();
	PendingRequestId.Reset();
	PendingProjectPath.Reset();
	PendingRunId.Reset();
	PendingRunDirectory.Reset();
	PendingReviewOutputPath.Reset();
}

bool UPlatformAnalysisAiSubsystem::BuildAnalysisRequestJsonForProjectRun(
	const FString& projectPath,
	const FString& runId,
	FString& outRequestJson,
	TArray<FString>& outDiagnostics)
{
	outRequestJson.Reset();
	outDiagnostics.Reset();

	const FUserProjectRunSnapshotParseResult SnapshotResult = FUserProjectRunSnapshot::Parse(projectPath, runId);
	if (!SnapshotResult.bSuccess)
	{
		for (const FScenarioCompileDiagnostic& Diagnostic : SnapshotResult.Diagnostics)
		{
			outDiagnostics.Add(Diagnostic.Message);
		}
		return false;
	}

	if (!FPaths::FileExists(SnapshotResult.Paths.SummaryPath))
	{
		outDiagnostics.Add(FString::Printf(TEXT("summary.json file not found: %s"), *SnapshotResult.Paths.SummaryPath));
		return false;
	}

	TSharedRef<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("project_path"), SnapshotResult.Paths.ProjectPath);
	RequestObject->SetStringField(TEXT("run_id"), SnapshotResult.Paths.RunId);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&outRequestJson);
	if (!FJsonSerializer::Serialize(RequestObject, Writer))
	{
		outDiagnostics.Add(TEXT("Project run AI analysis request JSON serialization failed."));
		return false;
	}

	return true;
}

FString UPlatformAnalysisAiSubsystem::BuildDisplayTextFromAnalysisResponse(
	const FString& responseBody,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(responseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		outDiagnostics.Add(TEXT("AI analysis response JSON parse failed."));
		return TruncateText(responseBody, RawResponsePreviewCharacterLimit);
	}

	if (IsAnalysisV2Response(RootObject))
	{
		return BuildAnalysisV2DisplayText(RootObject);
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("AI Analysis"));
	AppendStringFieldLine(RootObject, TEXT("Analysis Id"), TEXT("analysisId"), Lines);
	AppendStringFieldLine(RootObject, TEXT("Generation"), TEXT("generationMethod"), Lines);

	FString Summary;
	if (RootObject->TryGetStringField(TEXT("summary"), Summary) && !Summary.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Summary"));
		Lines.Add(Summary);
	}

	TSharedPtr<FJsonObject> EpisodeStatistics;
	if (TryGetObjectField(*RootObject, TEXT("episodeStatistics"), EpisodeStatistics))
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Episode Statistics"));
		AppendStringFieldLine(EpisodeStatistics, TEXT("Outcome"), TEXT("outcome"), Lines);
		AppendStringFieldLine(EpisodeStatistics, TEXT("Terminal"), TEXT("terminal_reason"), Lines);
		AppendNumberFieldLine(EpisodeStatistics, TEXT("Duration(s)"), TEXT("duration_s"), Lines);
		AppendNumberFieldLine(EpisodeStatistics, TEXT("Score"), TEXT("score"), Lines);
	}

	AppendRecommendations(RootObject, TEXT("botSetupRecommendations"), TEXT("Bot Setup Recommendations"), Lines);
	AppendRecommendations(RootObject, TEXT("episodeSetupRecommendations"), TEXT("Episode Setup Recommendations"), Lines);
	AppendRecommendations(RootObject, TEXT("policyServerRecommendations"), TEXT("Policy Server Recommendations"), Lines);
	AppendWarnings(RootObject, Lines);

	return JoinLines(Lines);
}

void UPlatformAnalysisAiSubsystem::HandleAnalysisResponse(
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
	const FString ProjectPathContext = PendingProjectPath;
	const FString RunIdContext = PendingRunId;
	const FString RunDirectoryContext = PendingRunDirectory;
	const FString ReviewOutputPath = PendingReviewOutputPath;
	PendingProjectPath.Reset();
	PendingRunId.Reset();
	PendingRunDirectory.Reset();
	PendingReviewOutputPath.Reset();

	const int32 ResponseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
	const FString ResponseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString();
	if (!bWasSuccessful)
	{
		UE_LOG(
			LogPlatformAnalysisAi,
			Warning,
			TEXT("AI analysis HTTP request failed | Url: %s, Code: %d, Body: %s"),
			httpRequest.IsValid() ? *httpRequest->GetURL() : TEXT("<invalid>"),
			ResponseCode,
			*TruncateText(ResponseBody, RawResponsePreviewCharacterLimit));
		BroadcastFailure(
			ResponseCode,
			TEXT("AI analysis HTTP request failed."),
			ResponseBody,
			ProjectPathContext,
			RunIdContext,
			RunDirectoryContext,
			ReviewOutputPath);
		return;
	}

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		const FString Message = FString::Printf(TEXT("AI analysis HTTP error: %d"), ResponseCode);
		UE_LOG(
			LogPlatformAnalysisAi,
			Warning,
			TEXT("AI analysis HTTP error | Url: %s, Code: %d, Body: %s"),
			httpRequest.IsValid() ? *httpRequest->GetURL() : TEXT("<invalid>"),
			ResponseCode,
			*TruncateText(ResponseBody, RawResponsePreviewCharacterLimit));
		BroadcastFailure(
			ResponseCode,
			Message,
			ResponseBody,
			ProjectPathContext,
			RunIdContext,
			RunDirectoryContext,
			ReviewOutputPath);
		return;
	}

	if (!ReviewOutputPath.IsEmpty())
	{
		const FString ReviewDirectory = FPaths::GetPath(ReviewOutputPath);
		if (!IFileManager::Get().MakeDirectory(*ReviewDirectory, true)
			|| !FFileHelper::SaveStringToFile(
				ResponseBody,
				*ReviewOutputPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogPlatformAnalysisAi, Warning, TEXT("AI analysis review snapshot write failed: %s"), *ReviewOutputPath);
		}
	}

	TArray<FString> Diagnostics;
	const FString DisplayText = BuildDisplayTextFromAnalysisResponse(ResponseBody, Diagnostics);
	if (!Diagnostics.IsEmpty())
	{
		UE_LOG(LogPlatformAnalysisAi, Warning, TEXT("AI analysis response diagnostic | %s"), *JoinLines(Diagnostics));
	}

	OnAnalysisCompleted.Broadcast(MakeResponse(
		true,
		ResponseCode,
		DisplayText,
		FString(),
		ResponseBody,
		ProjectPathContext,
		RunIdContext,
		RunDirectoryContext,
		ReviewOutputPath));
}

void UPlatformAnalysisAiSubsystem::BroadcastFailure(
	int32 responseCode,
	const FString& message,
	const FString& responseBody,
	const FString& projectPath,
	const FString& runId,
	const FString& runDirectory,
	const FString& reviewOutputPath)
{
	UE_LOG(LogPlatformAnalysisAi, Warning, TEXT("AI analysis failed | Code: %d, Message: %s"), responseCode, *message);
	OnAnalysisCompleted.Broadcast(MakeResponse(
		false,
		responseCode,
		FString(),
		message,
		responseBody,
		projectPath,
		runId,
		runDirectory,
		reviewOutputPath));
}
