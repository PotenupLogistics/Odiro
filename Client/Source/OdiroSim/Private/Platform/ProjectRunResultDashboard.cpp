#include "Platform/ProjectRunResultDashboard.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* MainReviewDirectoryName = TEXT("review");
	const TCHAR* MainAnalysisResponseFileName = TEXT("analysis_run_response_v2.json");
	const TCHAR* MainRecommendationsFileName = TEXT("recommendations.json");
	constexpr double MainInstantGoalDurationToleranceSeconds = 0.1;
	constexpr double MainGoalDistanceToleranceMeters = 0.001;

	struct FDashboardRobotAnchor
	{
		FString Segment;
		double AlongMeters = 0.0;
		double OffsetMeters = 0.0;
	};

	FString NormalizeDashboardPath(FString Path)
	{
		Path = Path.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			return FString();
		}

		Path = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Path);
		return Path;
	}

	bool TryParseJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	bool TryReadJsonObjectFile(const FString& JsonFilePath, TSharedPtr<FJsonObject>& OutObject)
	{
		FString JsonString;
		return FFileHelper::LoadFileToString(JsonString, *JsonFilePath)
			&& TryParseJsonObject(JsonString, OutObject);
	}

	FString ReadStringOrDefault(const FJsonObject& Object, const FString& FieldName, const FString& DefaultValue = FString())
	{
		FString Value;
		return Object.TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	double ReadNumberOrDefault(const FJsonObject& Object, const FString& FieldName, const double DefaultValue = 0.0)
	{
		double Value = 0.0;
		return Object.TryGetNumberField(FieldName, Value) ? Value : DefaultValue;
	}

	int32 ReadIntegerMetric(const FJsonObject& MetricsObject, const FString& FieldName)
	{
		return FMath::RoundToInt(ReadNumberOrDefault(MetricsObject, FieldName, 0.0));
	}

	bool TryGetObjectField(const FJsonObject& Object, const FString& FieldName, TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			return false;
		}

		OutObject = Value->AsObject();
		return OutObject.IsValid();
	}

	bool TryGetArrayField(const FJsonObject& Object, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray)
	{
		OutArray.Reset();
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid() || Value->Type != EJson::Array)
		{
			return false;
		}

		OutArray = Value->AsArray();
		return true;
	}

	FString JsonValueToCompactString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return FString();
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			return FString::SanitizeFloat(Value->AsNumber());
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		default:
			break;
		}

		return FString();
	}

	int32 CountPrimaryCollisions(const FJsonObject& MetricsObject)
	{
		return ReadIntegerMetric(MetricsObject, TEXT("blocked_region_collision_count"))
			+ ReadIntegerMetric(MetricsObject, TEXT("pedestrian_collision_count"))
			+ ReadIntegerMetric(MetricsObject, TEXT("static_obstacle_collision_count"));
	}

	bool TryReadRobotAnchor(
		const FJsonObject& RowObject,
		const FString& AnchorName,
		FDashboardRobotAnchor& OutAnchor)
	{
		TSharedPtr<FJsonObject> ScenarioSemanticObject;
		TSharedPtr<FJsonObject> RobotObject;
		TSharedPtr<FJsonObject> AnchorObject;
		if (!TryGetObjectField(RowObject, TEXT("scenario_semantic"), ScenarioSemanticObject)
			|| !TryGetObjectField(*ScenarioSemanticObject, TEXT("robot"), RobotObject)
			|| !TryGetObjectField(*RobotObject, AnchorName, AnchorObject))
		{
			return false;
		}

		OutAnchor.Segment = ReadStringOrDefault(*AnchorObject, TEXT("segment"));
		OutAnchor.AlongMeters = ReadNumberOrDefault(*AnchorObject, TEXT("along_m"));
		OutAnchor.OffsetMeters = ReadNumberOrDefault(*AnchorObject, TEXT("offset_m"));
		return true;
	}

	bool IsStartInsideGoalRadius(const FJsonObject& RowObject, const FJsonObject* MetricsObject)
	{
		if (!MetricsObject)
		{
			return false;
		}

		const double GoalThresholdMeters = ReadNumberOrDefault(*MetricsObject, TEXT("goal_threshold_m"), -1.0);
		if (GoalThresholdMeters <= 0.0)
		{
			return false;
		}

		FDashboardRobotAnchor StartAnchor;
		FDashboardRobotAnchor GoalAnchor;
		if (!TryReadRobotAnchor(RowObject, TEXT("start"), StartAnchor)
			|| !TryReadRobotAnchor(RowObject, TEXT("goal"), GoalAnchor)
			|| StartAnchor.Segment.IsEmpty()
			|| !StartAnchor.Segment.Equals(GoalAnchor.Segment, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const double DeltaAlongMeters = StartAnchor.AlongMeters - GoalAnchor.AlongMeters;
		const double DeltaOffsetMeters = StartAnchor.OffsetMeters - GoalAnchor.OffsetMeters;
		const double StartGoalDistanceMeters = FMath::Sqrt(
			DeltaAlongMeters * DeltaAlongMeters + DeltaOffsetMeters * DeltaOffsetMeters);
		return StartGoalDistanceMeters <= GoalThresholdMeters + MainGoalDistanceToleranceMeters;
	}

	bool IsImmediateGoalReachedRow(
		const FJsonObject& RowObject,
		const FJsonObject* MetricsObject,
		const FString& TerminalReason,
		const double DurationSeconds)
	{
		if (!TerminalReason.Equals(TEXT("GoalReached"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		return IsStartInsideGoalRadius(RowObject, MetricsObject)
			|| DurationSeconds <= MainInstantGoalDurationToleranceSeconds;
	}

	bool IsSuccessRow(
		const FJsonObject& RowObject,
		const FJsonObject* MetricsObject,
		const FString& TerminalReason,
		const double DurationSeconds)
	{
		if (IsImmediateGoalReachedRow(RowObject, MetricsObject, TerminalReason, DurationSeconds))
		{
			return false;
		}

		const FString Outcome = ReadStringOrDefault(RowObject, TEXT("outcome"));
		if (Outcome.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		return MetricsObject && ReadNumberOrDefault(*MetricsObject, TEXT("goal_reached"), 0.0) > 0.0;
	}

	FString MakePreviewImagePath(const FString& RunDirectory, const FString& EpisodeId)
	{
		if (RunDirectory.IsEmpty() || EpisodeId.IsEmpty())
		{
			return FString();
		}

		const FString PreviewPath = NormalizeDashboardPath(FPaths::Combine(
			RunDirectory,
			TEXT("episodes"),
			EpisodeId,
			TEXT("preview.png")));
		return FPaths::FileExists(PreviewPath) ? PreviewPath : FString();
	}

	FProjectRunAiSuggestionDashboardItem MakeSuggestion(
		const EProjectRunAiSuggestionSeverity Severity,
		FString Message)
	{
		FProjectRunAiSuggestionDashboardItem Item;
		Item.Severity = Severity;
		switch (Severity)
		{
		case EProjectRunAiSuggestionSeverity::High:
			Item.SeverityLabel = TEXT("높음");
			break;
		case EProjectRunAiSuggestionSeverity::Medium:
			Item.SeverityLabel = TEXT("중간");
			break;
		case EProjectRunAiSuggestionSeverity::Low:
			Item.SeverityLabel = TEXT("낮음");
			break;
		default:
			Item.SeverityLabel = TEXT("정보");
			break;
		}
		Item.Message = MoveTemp(Message);
		return Item;
	}

	EProjectRunAiSuggestionSeverity ParseSuggestionSeverity(const FJsonObject& Object)
	{
		const FString Severity = ReadStringOrDefault(Object, TEXT("severity")).TrimStartAndEnd().ToLower();
		if (Severity == TEXT("high") || Severity == TEXT("critical") || Severity == TEXT("error") || Severity == TEXT("높음"))
		{
			return EProjectRunAiSuggestionSeverity::High;
		}
		if (Severity == TEXT("medium") || Severity == TEXT("warning") || Severity == TEXT("중간"))
		{
			return EProjectRunAiSuggestionSeverity::Medium;
		}
		if (Severity == TEXT("low") || Severity == TEXT("낮음"))
		{
			return EProjectRunAiSuggestionSeverity::Low;
		}

		const double Priority = ReadNumberOrDefault(Object, TEXT("priority"), 0.0);
		if (Priority >= 3.0)
		{
			return EProjectRunAiSuggestionSeverity::High;
		}
		if (Priority >= 2.0)
		{
			return EProjectRunAiSuggestionSeverity::Medium;
		}
		if (Priority >= 1.0)
		{
			return EProjectRunAiSuggestionSeverity::Low;
		}
		return EProjectRunAiSuggestionSeverity::Info;
	}

	FString BuildSuggestionMessage(const FJsonObject& Object)
	{
		FString Message = ReadStringOrDefault(Object, TEXT("message")).TrimStartAndEnd();
		if (!Message.IsEmpty())
		{
			return Message;
		}

		Message = ReadStringOrDefault(Object, TEXT("reason")).TrimStartAndEnd();
		const FString Param = ReadStringOrDefault(Object, TEXT("param")).TrimStartAndEnd();
		const FString Current = JsonValueToCompactString(Object.TryGetField(TEXT("current"))).TrimStartAndEnd();
		const FString Suggested = JsonValueToCompactString(Object.TryGetField(TEXT("suggested"))).TrimStartAndEnd();

		TArray<FString> Parts;
		if (!Param.IsEmpty() && (!Current.IsEmpty() || !Suggested.IsEmpty()))
		{
			Parts.Add(FString::Printf(TEXT("%s: %s -> %s"), *Param, *Current, *Suggested));
		}
		if (!Message.IsEmpty())
		{
			Parts.Add(Message);
		}
		return FString::Join(Parts, TEXT(" "));
	}

	bool AppendRecommendationsArray(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> RecommendationValues;
		if (!TryGetArrayField(RootObject, TEXT("recommendations"), RecommendationValues))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& RecommendationValue : RecommendationValues)
		{
			if (!RecommendationValue.IsValid() || RecommendationValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> RecommendationObject = RecommendationValue->AsObject();
			if (!RecommendationObject.IsValid())
			{
				continue;
			}

			FString Message = BuildSuggestionMessage(*RecommendationObject);
			if (Message.IsEmpty())
			{
				continue;
			}

			OutDashboardData.Suggestions.Add(MakeSuggestion(ParseSuggestionSeverity(*RecommendationObject), MoveTemp(Message)));
		}
		return true;
	}

	bool AppendLatestRecommendationsFile(
		const FString& ReviewDirectory,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<FString> ReviewDirectoryNames;
		IFileManager::Get().FindFiles(
			ReviewDirectoryNames,
			*FPaths::Combine(ReviewDirectory, TEXT("*")),
			false,
			true);
		ReviewDirectoryNames.Sort();

		for (int32 Index = ReviewDirectoryNames.Num() - 1; Index >= 0; --Index)
		{
			const FString RecommendationsPath = NormalizeDashboardPath(FPaths::Combine(
				ReviewDirectory,
				ReviewDirectoryNames[Index],
				MainRecommendationsFileName));
			FString RecommendationsJson;
			if (!FFileHelper::LoadFileToString(RecommendationsJson, *RecommendationsPath))
			{
				continue;
			}

			return FProjectRunResultDashboardJson::AppendAiFromRecommendationsJsonString(
				RecommendationsJson,
				OutDashboardData);
		}
		return false;
	}
}

bool FProjectRunResultDashboardJson::BuildFromRunDirectory(
	const FString& runDirectory,
	FProjectRunResultDashboardData& outDashboardData)
{
	outDashboardData = FProjectRunResultDashboardData();
	const FString RunDirectory = NormalizeDashboardPath(runDirectory);
	const FString SummaryPath = NormalizeDashboardPath(FPaths::Combine(RunDirectory, TEXT("summary.json")));

	FString SummaryJson;
	if (!FFileHelper::LoadFileToString(SummaryJson, *SummaryPath))
	{
		outDashboardData.RunId = FPaths::GetCleanFilename(RunDirectory);
		outDashboardData.Diagnostics.Add(FString::Printf(TEXT("summary.json 읽기 실패: %s"), *SummaryPath));
		AppendAiFromRunDirectory(RunDirectory, outDashboardData);
		return false;
	}

	const bool bSummaryLoaded = BuildFromSummaryJsonString(SummaryJson, RunDirectory, outDashboardData);
	AppendAiFromRunDirectory(RunDirectory, outDashboardData);
	return bSummaryLoaded;
}

bool FProjectRunResultDashboardJson::BuildFromSummaryJsonString(
	const FString& summaryJson,
	const FString& runDirectory,
	FProjectRunResultDashboardData& outDashboardData)
{
	outDashboardData = FProjectRunResultDashboardData();
	const FString RunDirectory = NormalizeDashboardPath(runDirectory);
	outDashboardData.RunId = FPaths::GetCleanFilename(RunDirectory);

	TSharedPtr<FJsonObject> RootObject;
	if (!TryParseJsonObject(summaryJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("summary.json JSON 파싱 실패"));
		return false;
	}

	const FString Schema = ReadStringOrDefault(*RootObject, TEXT("schema"));
	if (!Schema.Equals(TEXT("run_summary"), ESearchCase::CaseSensitive))
	{
		outDashboardData.Diagnostics.Add(FString::Printf(TEXT("지원하지 않는 summary schema: %s"), *Schema));
		return false;
	}

	TSharedPtr<FJsonObject> RunObject;
	if (TryGetObjectField(*RootObject, TEXT("run"), RunObject))
	{
		outDashboardData.RunId = ReadStringOrDefault(*RunObject, TEXT("run_id"), outDashboardData.RunId);
	}

	TArray<TSharedPtr<FJsonValue>> RowValues;
	if (!TryGetArrayField(*RootObject, TEXT("rows"), RowValues))
	{
		outDashboardData.Diagnostics.Add(TEXT("summary.json rows 배열 없음"));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& RowValue : RowValues)
	{
		if (!RowValue.IsValid() || RowValue->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> RowObject = RowValue->AsObject();
		if (!RowObject.IsValid())
		{
			continue;
		}

		TSharedPtr<FJsonObject> MetricsObject;
		const FJsonObject* MetricsPtr = TryGetObjectField(*RowObject, TEXT("metrics"), MetricsObject)
			? MetricsObject.Get()
			: nullptr;

		FProjectRunEpisodeDashboardItem Episode;
		Episode.EpisodeId = ReadStringOrDefault(*RowObject, TEXT("episode_id"));
		Episode.DurationSeconds = ReadNumberOrDefault(*RowObject, TEXT("duration_s"),
			MetricsPtr ? ReadNumberOrDefault(*MetricsPtr, TEXT("duration_s"), 0.0) : 0.0);
		Episode.Outcome = ReadStringOrDefault(*RowObject, TEXT("outcome"));
		Episode.TerminalReason = ReadStringOrDefault(*RowObject, TEXT("terminal_reason"));
		Episode.bSuccess = IsSuccessRow(
			*RowObject,
			MetricsPtr,
			Episode.TerminalReason,
			Episode.DurationSeconds);
		Episode.CollisionCount = MetricsPtr ? CountPrimaryCollisions(*MetricsPtr) : 0;
		Episode.PreviewImagePath = MakePreviewImagePath(RunDirectory, Episode.EpisodeId);

		outDashboardData.TotalDurationSeconds += Episode.DurationSeconds;
		outDashboardData.CollisionCount += Episode.CollisionCount;
		outDashboardData.SuccessCount += Episode.bSuccess ? 1 : 0;
		outDashboardData.Episodes.Add(Episode);
	}

	outDashboardData.EpisodeCount = outDashboardData.Episodes.Num();
	outDashboardData.bSummaryLoaded = true;
	return true;
}

bool FProjectRunResultDashboardJson::AppendAiFromRunDirectory(
	const FString& runDirectory,
	FProjectRunResultDashboardData& outDashboardData)
{
	const FString RunDirectory = NormalizeDashboardPath(runDirectory);
	const FString ReviewDirectory = NormalizeDashboardPath(FPaths::Combine(RunDirectory, MainReviewDirectoryName));
	const int32 InitialSuggestionCount = outDashboardData.Suggestions.Num();

	const FString AnalysisResponsePath = NormalizeDashboardPath(FPaths::Combine(ReviewDirectory, MainAnalysisResponseFileName));
	FString AnalysisResponseJson;
	if (FFileHelper::LoadFileToString(AnalysisResponseJson, *AnalysisResponsePath))
	{
		AppendAiFromAnalysisResponseJsonString(AnalysisResponseJson, outDashboardData);
	}

	if (outDashboardData.Suggestions.Num() == InitialSuggestionCount)
	{
		AppendLatestRecommendationsFile(ReviewDirectory, outDashboardData);
	}

	return outDashboardData.bAiLoaded;
}

bool FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(
	const FString& responseJson,
	FProjectRunResultDashboardData& outDashboardData)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!TryParseJsonObject(responseJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("analysis_run_response_v2.json JSON 파싱 실패"));
		return false;
	}

	const TSharedPtr<FJsonValue> SummaryValue = RootObject->TryGetField(TEXT("summary"));
	if (SummaryValue.IsValid())
	{
		if (SummaryValue->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> SummaryObject = SummaryValue->AsObject();
			if (SummaryObject.IsValid())
			{
				outDashboardData.AiSummary = ReadStringOrDefault(*SummaryObject, TEXT("message"), outDashboardData.AiSummary);
			}
		}
		else if (SummaryValue->Type == EJson::String)
		{
			outDashboardData.AiSummary = SummaryValue->AsString();
		}
	}

	AppendRecommendationsArray(*RootObject, outDashboardData);
	outDashboardData.bAiLoaded = true;
	return true;
}

bool FProjectRunResultDashboardJson::AppendAiFromRecommendationsJsonString(
	const FString& recommendationsJson,
	FProjectRunResultDashboardData& outDashboardData)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!TryParseJsonObject(recommendationsJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("recommendations.json JSON 파싱 실패"));
		return false;
	}

	const FString Reason = ReadStringOrDefault(*RootObject, TEXT("reason")).TrimStartAndEnd();
	if (outDashboardData.AiSummary.IsEmpty() && !Reason.IsEmpty())
	{
		outDashboardData.AiSummary = Reason;
	}

	AppendRecommendationsArray(*RootObject, outDashboardData);
	outDashboardData.bAiLoaded = true;
	return true;
}
