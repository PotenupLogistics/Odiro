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
	const TCHAR* MainReviewResponseFileName = TEXT("response.json");
	const TCHAR* MainReplayDirectoryName = TEXT("replay");
	const TCHAR* MainReplayManifestFileName = TEXT("replay.meta.json");
	const TCHAR* MainReplayFrameFileName = TEXT("replay.frames.bin");
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

	bool TryParseDashboardJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	bool TryReadDashboardJsonObjectFile(const FString& JsonFilePath, TSharedPtr<FJsonObject>& OutObject)
	{
		FString JsonString;
		return FFileHelper::LoadFileToString(JsonString, *JsonFilePath)
			&& TryParseDashboardJsonObject(JsonString, OutObject);
	}

	FString ReadDashboardStringOrDefault(const FJsonObject& Object, const FString& FieldName, const FString& DefaultValue = FString())
	{
		FString Value;
		return Object.TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	double ReadDashboardNumberOrDefault(const FJsonObject& Object, const FString& FieldName, const double DefaultValue = 0.0)
	{
		double Value = 0.0;
		return Object.TryGetNumberField(FieldName, Value) ? Value : DefaultValue;
	}

	int32 ReadDashboardIntegerMetric(const FJsonObject& MetricsObject, const FString& FieldName)
	{
		return FMath::RoundToInt(ReadDashboardNumberOrDefault(MetricsObject, FieldName, 0.0));
	}

	bool TryGetDashboardObjectField(const FJsonObject& Object, const FString& FieldName, TSharedPtr<FJsonObject>& OutObject)
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

	FString ReadDashboardDisplayString(
		const FJsonObject& Object,
		const FString& DisplayFieldName,
		const FString& DefaultValue = FString())
	{
		TSharedPtr<FJsonObject> DisplayObject;
		return TryGetDashboardObjectField(Object, TEXT("display"), DisplayObject)
			? ReadDashboardStringOrDefault(*DisplayObject, DisplayFieldName, DefaultValue).TrimStartAndEnd()
			: DefaultValue;
	}

	bool TryGetDashboardArrayField(const FJsonObject& Object, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutArray)
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

	FString DashboardJsonValueToCompactString(const TSharedPtr<FJsonValue>& Value)
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

	int32 CountDashboardPrimaryCollisions(const FJsonObject& MetricsObject)
	{
		return ReadDashboardIntegerMetric(MetricsObject, TEXT("blocked_region_collision_count"))
			+ ReadDashboardIntegerMetric(MetricsObject, TEXT("pedestrian_collision_count"))
			+ ReadDashboardIntegerMetric(MetricsObject, TEXT("static_obstacle_collision_count"));
	}

	bool TryReadDashboardRobotAnchor(
		const FJsonObject& RowObject,
		const FString& AnchorName,
		FDashboardRobotAnchor& OutAnchor)
	{
		TSharedPtr<FJsonObject> ScenarioSemanticObject;
		TSharedPtr<FJsonObject> RobotObject;
		TSharedPtr<FJsonObject> AnchorObject;
		if (!TryGetDashboardObjectField(RowObject, TEXT("scenario_semantic"), ScenarioSemanticObject)
			|| !TryGetDashboardObjectField(*ScenarioSemanticObject, TEXT("robot"), RobotObject)
			|| !TryGetDashboardObjectField(*RobotObject, AnchorName, AnchorObject))
		{
			return false;
		}

		OutAnchor.Segment = ReadDashboardStringOrDefault(*AnchorObject, TEXT("segment"));
		OutAnchor.AlongMeters = ReadDashboardNumberOrDefault(*AnchorObject, TEXT("along_m"));
		OutAnchor.OffsetMeters = ReadDashboardNumberOrDefault(*AnchorObject, TEXT("offset_m"));
		return true;
	}

	bool IsDashboardStartInsideGoalRadius(const FJsonObject& RowObject, const FJsonObject* MetricsObject)
	{
		if (!MetricsObject)
		{
			return false;
		}

		const double GoalThresholdMeters = ReadDashboardNumberOrDefault(*MetricsObject, TEXT("goal_threshold_m"), -1.0);
		if (GoalThresholdMeters <= 0.0)
		{
			return false;
		}

		FDashboardRobotAnchor StartAnchor;
		FDashboardRobotAnchor GoalAnchor;
		if (!TryReadDashboardRobotAnchor(RowObject, TEXT("start"), StartAnchor)
			|| !TryReadDashboardRobotAnchor(RowObject, TEXT("goal"), GoalAnchor)
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

		return IsDashboardStartInsideGoalRadius(RowObject, MetricsObject)
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

		const FString Outcome = ReadDashboardStringOrDefault(RowObject, TEXT("outcome"));
		if (Outcome.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		return MetricsObject && ReadDashboardNumberOrDefault(*MetricsObject, TEXT("goal_reached"), 0.0) > 0.0;
	}

	FString MakeEpisodeDirectory(const FString& RunDirectory, const FString& EpisodeId)
	{
		if (RunDirectory.IsEmpty() || EpisodeId.IsEmpty())
		{
			return FString();
		}

		return NormalizeDashboardPath(FPaths::Combine(
			RunDirectory,
			TEXT("episodes"),
			EpisodeId));
	}

	bool HasReplayArtifactsInDirectory(const FString& ReplayDirectory)
	{
		if (ReplayDirectory.IsEmpty())
		{
			return false;
		}

		return FPaths::FileExists(FPaths::Combine(ReplayDirectory, MainReplayManifestFileName))
			&& FPaths::FileExists(FPaths::Combine(ReplayDirectory, MainReplayFrameFileName));
	}

	bool HasEpisodeReplayArtifacts(const FString& EpisodeDirectory)
	{
		if (EpisodeDirectory.IsEmpty())
		{
			return false;
		}

		return HasReplayArtifactsInDirectory(FPaths::Combine(EpisodeDirectory, MainReplayDirectoryName))
			|| HasReplayArtifactsInDirectory(EpisodeDirectory);
	}

	FString MakePreviewImagePath(const FString& RunDirectory, const FString& EpisodeId)
	{
		const FString EpisodeDirectory = MakeEpisodeDirectory(RunDirectory, EpisodeId);
		if (EpisodeDirectory.IsEmpty())
		{
			return FString();
		}

		const FString PreviewPath = NormalizeDashboardPath(FPaths::Combine(
			EpisodeDirectory,
			TEXT("preview.png")));
		return FPaths::FileExists(PreviewPath) ? PreviewPath : FString();
	}

	FString MakeDashboardSeverityLabel(const EProjectRunAiSuggestionSeverity Severity)
	{
		switch (Severity)
		{
		case EProjectRunAiSuggestionSeverity::High:
			return TEXT("경고");
		case EProjectRunAiSuggestionSeverity::Medium:
			return TEXT("주의");
		case EProjectRunAiSuggestionSeverity::Low:
			return TEXT("알림");
		default:
			return TEXT("알림");
		}
	}

	void ApplySuggestionSeverityLabel(FProjectRunAiSuggestionDashboardItem& Item)
	{
		Item.SeverityLabel = MakeDashboardSeverityLabel(Item.Severity);
	}

	void ApplyInsightSeverityLabel(FProjectRunAnalysisInsightDashboardItem& Item)
	{
		Item.SeverityLabel = MakeDashboardSeverityLabel(Item.Severity);
	}

	EProjectRunAiSuggestionSeverity ParseSuggestionSeverity(const FJsonObject& Object)
	{
		const FString Severity = ReadDashboardStringOrDefault(Object, TEXT("severity")).TrimStartAndEnd().ToLower();
		if (Severity == TEXT("high") || Severity == TEXT("critical") || Severity == TEXT("error")
			|| Severity == TEXT("높음") || Severity == TEXT("경고"))
		{
			return EProjectRunAiSuggestionSeverity::High;
		}
		if (Severity == TEXT("medium") || Severity == TEXT("warning")
			|| Severity == TEXT("중간") || Severity == TEXT("주의"))
		{
			return EProjectRunAiSuggestionSeverity::Medium;
		}
		if (Severity == TEXT("low") || Severity == TEXT("낮음") || Severity == TEXT("알림"))
		{
			return EProjectRunAiSuggestionSeverity::Low;
		}

		const FString PriorityText = ReadDashboardStringOrDefault(Object, TEXT("priority")).TrimStartAndEnd().ToLower();
		if (PriorityText == TEXT("high") || PriorityText == TEXT("critical")
			|| PriorityText == TEXT("높음") || PriorityText == TEXT("경고"))
		{
			return EProjectRunAiSuggestionSeverity::High;
		}
		if (PriorityText == TEXT("medium") || PriorityText == TEXT("warning")
			|| PriorityText == TEXT("중간") || PriorityText == TEXT("주의"))
		{
			return EProjectRunAiSuggestionSeverity::Medium;
		}
		if (PriorityText == TEXT("low") || PriorityText == TEXT("낮음") || PriorityText == TEXT("알림"))
		{
			return EProjectRunAiSuggestionSeverity::Low;
		}

		const double Priority = ReadDashboardNumberOrDefault(Object, TEXT("priority"), 0.0);
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

	FString ReadTrimmedDashboardString(const FJsonObject& Object, const FString& FieldName)
	{
		return ReadDashboardStringOrDefault(Object, FieldName).TrimStartAndEnd();
	}

	bool HasSuggestionDisplayContent(const FProjectRunAiSuggestionDashboardItem& Item)
	{
		return !Item.Title.IsEmpty()
			|| !Item.Message.IsEmpty()
			|| !Item.Reason.IsEmpty()
			|| !Item.Recommendation.IsEmpty()
			|| !Item.ParameterName.IsEmpty()
			|| !Item.CurrentValue.IsEmpty()
			|| !Item.SuggestedValue.IsEmpty();
	}

	bool HasInsightDisplayContent(const FProjectRunAnalysisInsightDashboardItem& Item)
	{
		return !Item.Title.IsEmpty() || !Item.Description.IsEmpty();
	}

	bool ShouldDisplayDashboardAnalysisWarning(const FString& WarningText)
	{
		const FString NormalizedWarning = WarningText.TrimStartAndEnd().ToLower();
		return !NormalizedWarning.StartsWith(TEXT("skipped large file:"))
			&& !NormalizedWarning.StartsWith(TEXT("skipped symlink in policy copy:"));
	}

	FProjectRunAiSuggestionDashboardItem MakeSuggestion(const FJsonObject& Object)
	{
		FProjectRunAiSuggestionDashboardItem Item;
		Item.Severity = ParseSuggestionSeverity(Object);
		ApplySuggestionSeverityLabel(Item);
		Item.Title = ReadTrimmedDashboardString(Object, TEXT("title"));
		Item.Message = ReadTrimmedDashboardString(Object, TEXT("message"));
		Item.Reason = ReadTrimmedDashboardString(Object, TEXT("reason"));
		Item.Recommendation = ReadTrimmedDashboardString(Object, TEXT("recommendation"));
		if (Item.Recommendation.IsEmpty())
		{
			Item.Recommendation = ReadTrimmedDashboardString(Object, TEXT("suggestion"));
		}
		Item.ParameterName = ReadTrimmedDashboardString(Object, TEXT("param"));
		if (Item.ParameterName.IsEmpty())
		{
			Item.ParameterName = ReadTrimmedDashboardString(Object, TEXT("parameter"));
		}
		if (Item.ParameterName.IsEmpty())
		{
			Item.ParameterName = ReadTrimmedDashboardString(Object, TEXT("target"));
		}
		Item.CurrentValue = DashboardJsonValueToCompactString(Object.TryGetField(TEXT("current"))).TrimStartAndEnd();
		Item.SuggestedValue = DashboardJsonValueToCompactString(Object.TryGetField(TEXT("suggested"))).TrimStartAndEnd();
		return Item;
	}

	FProjectRunAnalysisInsightDashboardItem MakeInsight(const FJsonObject& Object)
	{
		FProjectRunAnalysisInsightDashboardItem Item;
		Item.Severity = ParseSuggestionSeverity(Object);
		ApplyInsightSeverityLabel(Item);
		Item.Title = ReadTrimmedDashboardString(Object, TEXT("title"));
		Item.Description = ReadTrimmedDashboardString(Object, TEXT("description"));
		if (Item.Description.IsEmpty())
		{
			Item.Description = ReadTrimmedDashboardString(Object, TEXT("detail"));
		}
		if (Item.Description.IsEmpty())
		{
			Item.Description = ReadTrimmedDashboardString(Object, TEXT("message"));
		}
		return Item;
	}

	void AppendWarningsArray(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> WarningValues;
		if (!TryGetDashboardArrayField(RootObject, TEXT("warnings"), WarningValues))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& WarningValue : WarningValues)
		{
			FString WarningText;
			if (WarningValue.IsValid() && WarningValue->Type == EJson::String)
			{
				WarningText = WarningValue->AsString().TrimStartAndEnd();
			}
			else if (WarningValue.IsValid() && WarningValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> WarningObject = WarningValue->AsObject();
				if (WarningObject.IsValid())
				{
					WarningText = ReadTrimmedDashboardString(*WarningObject, TEXT("message"));
				}
			}

			if (!WarningText.IsEmpty() && ShouldDisplayDashboardAnalysisWarning(WarningText))
			{
				OutDashboardData.Warnings.Add(WarningText);
			}
		}
	}

	void AppendInsightsArray(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> InsightValues;
		if (!TryGetDashboardArrayField(RootObject, TEXT("insights"), InsightValues))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& InsightValue : InsightValues)
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

			FProjectRunAnalysisInsightDashboardItem Insight = MakeInsight(*InsightObject);
			if (HasInsightDisplayContent(Insight))
			{
				OutDashboardData.Insights.Add(MoveTemp(Insight));
			}
		}
	}

	void ApplyRunOverview(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TSharedPtr<FJsonObject> RunOverviewObject;
		if (!TryGetDashboardObjectField(RootObject, TEXT("run_overview"), RunOverviewObject))
		{
			return;
		}

		double TotalPlayTimeSeconds = 0.0;
		if (RunOverviewObject->TryGetNumberField(TEXT("total_play_time_s"), TotalPlayTimeSeconds))
		{
			OutDashboardData.TotalDurationSeconds = TotalPlayTimeSeconds;
		}

		double SuccessRate = 0.0;
		const bool bHasSuccessRate = RunOverviewObject->TryGetNumberField(TEXT("success_rate"), SuccessRate);
		double EpisodeCountValue = 0.0;
		if (RunOverviewObject->TryGetNumberField(TEXT("episode_count"), EpisodeCountValue)
			&& FMath::RoundToInt(EpisodeCountValue) > 0)
		{
			const int32 EpisodeCount = FMath::RoundToInt(EpisodeCountValue);
			OutDashboardData.EpisodeCount = EpisodeCount;
			if (bHasSuccessRate)
			{
				const double SuccessRateRatio = SuccessRate > 1.0 ? SuccessRate / 100.0 : SuccessRate;
				OutDashboardData.SuccessCount = FMath::RoundToInt(SuccessRateRatio * EpisodeCount);
			}
		}

		double CollisionCountValue = 0.0;
		if (RunOverviewObject->TryGetNumberField(TEXT("collision_count"), CollisionCountValue))
		{
			OutDashboardData.CollisionCount = FMath::RoundToInt(CollisionCountValue);
		}
		OutDashboardData.TotalPlayTimeLabel = ReadDashboardDisplayString(
			*RunOverviewObject,
			TEXT("total_play_time"),
			OutDashboardData.TotalPlayTimeLabel);
		OutDashboardData.SuccessRateLabel = ReadDashboardDisplayString(
			*RunOverviewObject,
			TEXT("success_rate"),
			OutDashboardData.SuccessRateLabel);
		OutDashboardData.CollisionCountLabel = ReadDashboardDisplayString(
			*RunOverviewObject,
			TEXT("collision_count"),
			OutDashboardData.CollisionCountLabel);
	}

	void ApplyEpisodeDisplayData(
		FProjectRunEpisodeDashboardItem& Episode,
		const FJsonObject& EpisodeObject,
		const FString& RunDirectory)
	{
		Episode.EpisodeId = ReadDashboardStringOrDefault(EpisodeObject, TEXT("episode_id"), Episode.EpisodeId);
		Episode.DurationSeconds = ReadDashboardNumberOrDefault(EpisodeObject, TEXT("duration_s"), Episode.DurationSeconds);
		Episode.DurationLabel = ReadDashboardDisplayString(EpisodeObject, TEXT("duration"), Episode.DurationLabel);
		Episode.Outcome = ReadDashboardStringOrDefault(EpisodeObject, TEXT("outcome"), Episode.Outcome);
		Episode.OutcomeLabel = ReadDashboardDisplayString(EpisodeObject, TEXT("outcome"), Episode.OutcomeLabel);

		if (!Episode.Outcome.IsEmpty())
		{
			Episode.bSuccess = Episode.Outcome.Equals(TEXT("success"), ESearchCase::IgnoreCase)
				|| Episode.Outcome.Equals(TEXT("Success"), ESearchCase::IgnoreCase);
		}
		if (Episode.EpisodeDirectory.IsEmpty() && !RunDirectory.IsEmpty())
		{
			Episode.EpisodeDirectory = MakeEpisodeDirectory(RunDirectory, Episode.EpisodeId);
			Episode.bReplayAvailable = HasEpisodeReplayArtifacts(Episode.EpisodeDirectory);
			Episode.PreviewImagePath = MakePreviewImagePath(RunDirectory, Episode.EpisodeId);
		}
	}

	void ApplyEpisodesArray(
		const FJsonObject& RootObject,
		const FString& RunDirectory,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> EpisodeValues;
		if (!TryGetDashboardArrayField(RootObject, TEXT("episodes"), EpisodeValues))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& EpisodeValue : EpisodeValues)
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

			const FString EpisodeId = ReadDashboardStringOrDefault(*EpisodeObject, TEXT("episode_id")).TrimStartAndEnd();
			if (EpisodeId.IsEmpty())
			{
				continue;
			}

			FProjectRunEpisodeDashboardItem* ExistingEpisode = OutDashboardData.Episodes.FindByPredicate(
				[&EpisodeId](const FProjectRunEpisodeDashboardItem& Candidate)
				{
					return Candidate.EpisodeId.Equals(EpisodeId, ESearchCase::IgnoreCase);
				});
			if (ExistingEpisode)
			{
				ApplyEpisodeDisplayData(*ExistingEpisode, *EpisodeObject, RunDirectory);
				continue;
			}

			FProjectRunEpisodeDashboardItem Episode;
			ApplyEpisodeDisplayData(Episode, *EpisodeObject, RunDirectory);
			OutDashboardData.Episodes.Add(MoveTemp(Episode));
		}

		if (!EpisodeValues.IsEmpty())
		{
			OutDashboardData.EpisodeCount = OutDashboardData.Episodes.Num();
		}
	}

	bool AppendRecommendationsArray(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> RecommendationValues;
		if (!TryGetDashboardArrayField(RootObject, TEXT("recommendations"), RecommendationValues))
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

			FProjectRunAiSuggestionDashboardItem Suggestion = MakeSuggestion(*RecommendationObject);
			if (!HasSuggestionDisplayContent(Suggestion))
			{
				continue;
			}

			OutDashboardData.Suggestions.Add(MoveTemp(Suggestion));
		}
		return true;
	}

	bool AppendLegacyRecommendationsArray(
		const FJsonObject& RootObject,
		const FString& FieldName,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		TArray<TSharedPtr<FJsonValue>> RecommendationValues;
		if (!TryGetDashboardArrayField(RootObject, FieldName, RecommendationValues))
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

			FProjectRunAiSuggestionDashboardItem Suggestion = MakeSuggestion(*RecommendationObject);
			if (!HasSuggestionDisplayContent(Suggestion))
			{
				continue;
			}

			OutDashboardData.Suggestions.Add(MoveTemp(Suggestion));
		}
		return true;
	}

	bool AppendLegacyRecommendationsArrays(
		const FJsonObject& RootObject,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		bool bFoundRecommendations = false;
		bFoundRecommendations |= AppendLegacyRecommendationsArray(
			RootObject,
			TEXT("botSetupRecommendations"),
			OutDashboardData);
		bFoundRecommendations |= AppendLegacyRecommendationsArray(
			RootObject,
			TEXT("episodeSetupRecommendations"),
			OutDashboardData);
		bFoundRecommendations |= AppendLegacyRecommendationsArray(
			RootObject,
			TEXT("policyServerRecommendations"),
			OutDashboardData);
		return bFoundRecommendations;
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

	bool AppendAiFromAnalysisResponseJsonObject(
		const FJsonObject& RootObject,
		const FString& RunDirectory,
		FProjectRunResultDashboardData& OutDashboardData);

	bool AppendAnalysisResponseJsonFile(
		const FString& ResponsePath,
		const FString& RunDirectory,
		const FString& DiagnosticFileName,
		FProjectRunResultDashboardData& OutDashboardData,
		bool& bOutResponseFailed)
	{
		bOutResponseFailed = false;

		FString ResponseJson;
		if (!FFileHelper::LoadFileToString(ResponseJson, *ResponsePath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		if (!TryParseDashboardJsonObject(ResponseJson, RootObject) || !RootObject.IsValid())
		{
			OutDashboardData.Diagnostics.Add(FString::Printf(TEXT("%s JSON 파싱 실패"), *DiagnosticFileName));
			return false;
		}

		const FString Status = ReadDashboardStringOrDefault(*RootObject, TEXT("status")).TrimStartAndEnd();
		bOutResponseFailed = Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase);
		AppendAiFromAnalysisResponseJsonObject(*RootObject, RunDirectory, OutDashboardData);
		return true;
	}

	bool AppendLatestReviewResponseFile(
		const FString& ReviewDirectory,
		const FString& RunDirectory,
		FProjectRunResultDashboardData& OutDashboardData,
		bool& bOutResponseFailed)
	{
		bOutResponseFailed = false;

		TArray<FString> ReviewDirectoryNames;
		IFileManager::Get().FindFiles(
			ReviewDirectoryNames,
			*FPaths::Combine(ReviewDirectory, TEXT("*")),
			false,
			true);
		ReviewDirectoryNames.Sort();

		for (int32 Index = ReviewDirectoryNames.Num() - 1; Index >= 0; --Index)
		{
			const FString ResponsePath = NormalizeDashboardPath(FPaths::Combine(
				ReviewDirectory,
				ReviewDirectoryNames[Index],
				MainReviewResponseFileName));
			if (AppendAnalysisResponseJsonFile(
				ResponsePath,
				RunDirectory,
				MainReviewResponseFileName,
				OutDashboardData,
				bOutResponseFailed))
			{
				return true;
			}
		}
		return false;
	}

	bool AppendAiFromAnalysisResponseJsonObject(
		const FJsonObject& RootObject,
		const FString& RunDirectory,
		FProjectRunResultDashboardData& OutDashboardData)
	{
		AppendWarningsArray(RootObject, OutDashboardData);

		const FString Status = ReadDashboardStringOrDefault(RootObject, TEXT("status")).TrimStartAndEnd();
		if (Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> ErrorObject;
			if (TryGetDashboardObjectField(RootObject, TEXT("error"), ErrorObject))
			{
				const FString ErrorMessage = ReadTrimmedDashboardString(*ErrorObject, TEXT("message"));
				if (!ErrorMessage.IsEmpty())
				{
					OutDashboardData.AiSummary = ErrorMessage;
					OutDashboardData.Diagnostics.Add(ErrorMessage);
				}
			}
			OutDashboardData.bAiLoaded = true;
			return true;
		}

		if (OutDashboardData.RunId.IsEmpty())
		{
			OutDashboardData.RunId = ReadDashboardStringOrDefault(RootObject, TEXT("run_id"));
		}

		ApplyRunOverview(RootObject, OutDashboardData);
		ApplyEpisodesArray(RootObject, RunDirectory, OutDashboardData);

		const TSharedPtr<FJsonValue> SummaryValue = RootObject.TryGetField(TEXT("summary"));
		if (SummaryValue.IsValid())
		{
			if (SummaryValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> SummaryObject = SummaryValue->AsObject();
				if (SummaryObject.IsValid())
				{
					OutDashboardData.AiSummary = ReadDashboardStringOrDefault(
						*SummaryObject,
						TEXT("message"),
						OutDashboardData.AiSummary).TrimStartAndEnd();
				}
			}
			else if (SummaryValue->Type == EJson::String)
			{
				OutDashboardData.AiSummary = SummaryValue->AsString().TrimStartAndEnd();
			}
		}

		AppendInsightsArray(RootObject, OutDashboardData);
		const int32 InitialSuggestionCount = OutDashboardData.Suggestions.Num();
		AppendRecommendationsArray(RootObject, OutDashboardData);
		if (OutDashboardData.Suggestions.Num() == InitialSuggestionCount)
		{
			AppendLegacyRecommendationsArrays(RootObject, OutDashboardData);
		}
		OutDashboardData.bAiLoaded = true;
		return true;
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
	if (!TryParseDashboardJsonObject(summaryJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("summary.json JSON 파싱 실패"));
		return false;
	}

	const FString Schema = ReadDashboardStringOrDefault(*RootObject, TEXT("schema"));
	if (!Schema.Equals(TEXT("run_summary"), ESearchCase::CaseSensitive))
	{
		outDashboardData.Diagnostics.Add(FString::Printf(TEXT("지원하지 않는 summary schema: %s"), *Schema));
		return false;
	}

	TSharedPtr<FJsonObject> RunObject;
	if (TryGetDashboardObjectField(*RootObject, TEXT("run"), RunObject))
	{
		outDashboardData.RunId = ReadDashboardStringOrDefault(*RunObject, TEXT("run_id"), outDashboardData.RunId);
	}

	TArray<TSharedPtr<FJsonValue>> RowValues;
	if (!TryGetDashboardArrayField(*RootObject, TEXT("rows"), RowValues))
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
		const FJsonObject* MetricsPtr = TryGetDashboardObjectField(*RowObject, TEXT("metrics"), MetricsObject)
			? MetricsObject.Get()
			: nullptr;

		FProjectRunEpisodeDashboardItem Episode;
		Episode.EpisodeId = ReadDashboardStringOrDefault(*RowObject, TEXT("episode_id"));
		Episode.DurationSeconds = ReadDashboardNumberOrDefault(*RowObject, TEXT("duration_s"),
			MetricsPtr ? ReadDashboardNumberOrDefault(*MetricsPtr, TEXT("duration_s"), 0.0) : 0.0);
		Episode.Outcome = ReadDashboardStringOrDefault(*RowObject, TEXT("outcome"));
		Episode.TerminalReason = ReadDashboardStringOrDefault(*RowObject, TEXT("terminal_reason"));
		Episode.bSuccess = IsSuccessRow(
			*RowObject,
			MetricsPtr,
			Episode.TerminalReason,
			Episode.DurationSeconds);
		Episode.CollisionCount = MetricsPtr ? CountDashboardPrimaryCollisions(*MetricsPtr) : 0;
		Episode.EpisodeDirectory = MakeEpisodeDirectory(RunDirectory, Episode.EpisodeId);
		Episode.bReplayAvailable = HasEpisodeReplayArtifacts(Episode.EpisodeDirectory);
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

	bool bAnalysisResponseFailed = false;
	const bool bLoadedReviewResponse = AppendLatestReviewResponseFile(
		ReviewDirectory,
		RunDirectory,
		outDashboardData,
		bAnalysisResponseFailed);
	if (!bLoadedReviewResponse)
	{
		const FString AnalysisResponsePath = NormalizeDashboardPath(FPaths::Combine(
			ReviewDirectory,
			MainAnalysisResponseFileName));
		AppendAnalysisResponseJsonFile(
			AnalysisResponsePath,
			RunDirectory,
			MainAnalysisResponseFileName,
			outDashboardData,
			bAnalysisResponseFailed);
	}

	if (!bAnalysisResponseFailed && outDashboardData.Suggestions.Num() == InitialSuggestionCount)
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
	if (!TryParseDashboardJsonObject(responseJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("analysis_run_response_v2.json JSON 파싱 실패"));
		return false;
	}

	return AppendAiFromAnalysisResponseJsonObject(*RootObject, FString(), outDashboardData);
}

bool FProjectRunResultDashboardJson::AppendAiFromRecommendationsJsonString(
	const FString& recommendationsJson,
	FProjectRunResultDashboardData& outDashboardData)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!TryParseDashboardJsonObject(recommendationsJson, RootObject))
	{
		outDashboardData.Diagnostics.Add(TEXT("recommendations.json JSON 파싱 실패"));
		return false;
	}

	const FString Reason = ReadDashboardStringOrDefault(*RootObject, TEXT("reason")).TrimStartAndEnd();
	if (outDashboardData.AiSummary.IsEmpty() && !Reason.IsEmpty())
	{
		outDashboardData.AiSummary = Reason;
	}

	AppendWarningsArray(*RootObject, outDashboardData);
	AppendRecommendationsArray(*RootObject, outDashboardData);
	outDashboardData.bAiLoaded = true;
	return true;
}
