#include "Platform/ViewModel/ExperimentResultViewModel.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"

namespace
{
	FString NormalizeExperimentResultVmPath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

	FString BuildExperimentResultVmRunDirectory(const FString& projectPath, const FString& runId)
	{
		const FString normalizedProjectPath = NormalizeExperimentResultVmPath(projectPath);
		const FString normalizedRunId = runId.TrimStartAndEnd();
		if (normalizedProjectPath.IsEmpty() || normalizedRunId.IsEmpty())
		{
			return FString();
		}

		return NormalizeExperimentResultVmPath(FPaths::Combine(normalizedProjectPath, TEXT("runs"), normalizedRunId));
	}

	template <typename ItemT>
	TArray<ItemT*> CopyExperimentResultItems(const TArray<TObjectPtr<ItemT>>& sourceItems)
	{
		TArray<ItemT*> result;
		result.Reserve(sourceItems.Num());
		for (ItemT* item : sourceItems)
		{
			result.Add(item);
		}
		return result;
	}
}

void UExperimentResultViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	if (UPlatformUiSubsystem* subsystem = ResolvePlatformUiSubsystem())
	{
		subsystem->OnAnalysisCompleted.RemoveAll(this);
		subsystem->OnAnalysisCompleted.AddUObject(this, &UExperimentResultViewModel::HandleAnalysisCompleted);
	}
}

void UExperimentResultViewModel::SetSubsystemOverride(UPlatformUiSubsystem* platformUiSubsystem)
{
	if (UPlatformUiSubsystem* oldSubsystem = ResolvePlatformUiSubsystem())
	{
		oldSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	PlatformUiOverride = platformUiSubsystem;

	if (UPlatformUiSubsystem* newSubsystem = ResolvePlatformUiSubsystem())
	{
		newSubsystem->OnAnalysisCompleted.RemoveAll(this);
		newSubsystem->OnAnalysisCompleted.AddUObject(this, &UExperimentResultViewModel::HandleAnalysisCompleted);
	}
}

bool UExperimentResultViewModel::LoadRunDirectory(const FString& runDirectory)
{
	const FString normalizedRunDirectory = NormalizeExperimentResultVmPath(runDirectory);
	if (normalizedRunDirectory.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Run directory가 없습니다."));
		return false;
	}

	FProjectRunResultDashboardData loadedDashboardData;
	const bool bLoaded = UPlatformUiSubsystem::LoadProjectRunDashboard(
		normalizedRunDirectory,
		loadedDashboardData);

	SetSelectedRunDirectory(normalizedRunDirectory);
	SetRunId(loadedDashboardData.RunId);
	SetDashboardData(loadedDashboardData);
	RefreshDisplayLabels();

	if (!loadedDashboardData.Diagnostics.IsEmpty())
	{
		SetDiagnosticsText(FString::Join(loadedDashboardData.Diagnostics, TEXT("\n")));
	}
	else
	{
		ClearDiagnostics();
	}

	return bLoaded;
}

bool UExperimentResultViewModel::RequestAiAnalysis(const FString& projectPath, const FString& runId)
{
	UPlatformUiSubsystem* subsystem = ResolvePlatformUiSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString normalizedProjectPath = NormalizeExperimentResultVmPath(projectPath);
	const FString normalizedRunId = runId.TrimStartAndEnd();
	PendingAnalysisRunId = normalizedRunId;
	PendingAnalysisRunDirectory = BuildExperimentResultVmRunDirectory(normalizedProjectPath, normalizedRunId);
	FString errorText;
	if (!subsystem->RequestProjectRunAnalysis(normalizedProjectPath, normalizedRunId, errorText))
	{
		PendingAnalysisRunId.Reset();
		PendingAnalysisRunDirectory.Reset();
		SetDiagnosticsText(errorText.IsEmpty() ? TEXT("AI 분석 요청 실패.") : errorText);
		return false;
	}

	SetBusy(true);
	SetDiagnosticsText(FString::Printf(TEXT("AI analysis requested: %s"), *normalizedRunId));
	return true;
}

TArray<UExperimentResultEpisodeViewModel*> UExperimentResultViewModel::GetEpisodeItems() const
{
	return CopyExperimentResultItems(EpisodeItems);
}

TArray<UExperimentResultSuggestionViewModel*> UExperimentResultViewModel::GetSuggestionItems() const
{
	return CopyExperimentResultItems(SuggestionItems);
}

TArray<UExperimentResultInsightViewModel*> UExperimentResultViewModel::GetInsightItems() const
{
	return CopyExperimentResultItems(InsightItems);
}

TArray<UOdiroListItemViewModel*> UExperimentResultViewModel::GetWarningItems() const
{
	return CopyExperimentResultItems(WarningItems);
}

UPlatformUiSubsystem* UExperimentResultViewModel::ResolvePlatformUiSubsystem() const
{
	if (PlatformUiOverride)
	{
		return PlatformUiOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UPlatformUiSubsystem>() : nullptr;
}

void UExperimentResultViewModel::HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response)
{
	const FString responseRunId = response.RunId.TrimStartAndEnd();
	if (!responseRunId.IsEmpty())
	{
		const FString currentRunId = RunId.IsEmpty()
			? FPaths::GetCleanFilename(SelectedRunDirectory)
			: RunId;
		const bool bPendingDifferentRun = !PendingAnalysisRunId.IsEmpty()
			&& !responseRunId.Equals(PendingAnalysisRunId, ESearchCase::IgnoreCase);
		const bool bDisplayedDifferentRun = PendingAnalysisRunId.IsEmpty()
			&& !currentRunId.IsEmpty()
			&& !responseRunId.Equals(currentRunId, ESearchCase::IgnoreCase);
		if (bPendingDifferentRun || bDisplayedDifferentRun)
		{
			return;
		}
	}

	SetBusy(false);
	if (response.bSuccess)
	{
		const FString responseRunDirectory = NormalizeExperimentResultVmPath(response.RunDirectory);
		const FString runDirectory = !responseRunDirectory.IsEmpty()
			? responseRunDirectory
			: !PendingAnalysisRunDirectory.IsEmpty()
			? PendingAnalysisRunDirectory
			: SelectedRunDirectory;
		if (!runDirectory.IsEmpty())
		{
			LoadRunDirectory(runDirectory);
		}
		PendingAnalysisRunId.Reset();
		PendingAnalysisRunDirectory.Reset();
		return;
	}

	const FString runId = !responseRunId.IsEmpty()
		? responseRunId
		: PendingAnalysisRunId.IsEmpty()
		? RunId
		: PendingAnalysisRunId;
	const FString errorMessage = response.ErrorMessage.IsEmpty() ? response.DisplayText : response.ErrorMessage;
	SetDiagnosticsText(runId.IsEmpty()
		? errorMessage
		: FString::Printf(TEXT("AI analysis failed for %s: %s"), *runId, *errorMessage));
	PendingAnalysisRunId.Reset();
	PendingAnalysisRunDirectory.Reset();
}

void UExperimentResultViewModel::SetSelectedRunDirectory(const FString& runDirectory)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedRunDirectory, NormalizeExperimentResultVmPath(runDirectory));
}

void UExperimentResultViewModel::SetRunId(const FString& runId)
{
	UE_MVVM_SET_PROPERTY_VALUE(RunId, runId.TrimStartAndEnd());
}

void UExperimentResultViewModel::SetDashboardData(const FProjectRunResultDashboardData& dashboardData)
{
	DashboardData = dashboardData;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DashboardData);
	RebuildDashboardItemViewModels();
}

void UExperimentResultViewModel::RefreshDisplayLabels()
{
	const FString totalDurationLabel = DashboardData.TotalPlayTimeLabel.TrimStartAndEnd();
	UE_MVVM_SET_PROPERTY_VALUE(
		TotalDurationLabel,
		totalDurationLabel.IsEmpty()
			? FString::Printf(TEXT("%.1f s"), DashboardData.TotalDurationSeconds)
			: totalDurationLabel);

	const FString averageDuration = DashboardData.EpisodeCount > 0
		? FString::Printf(TEXT("%.1f s"), DashboardData.TotalDurationSeconds / DashboardData.EpisodeCount)
		: FString(TEXT("-"));
	UE_MVVM_SET_PROPERTY_VALUE(AverageDurationLabel, averageDuration);

	const FString successRateOverride = DashboardData.SuccessRateLabel.TrimStartAndEnd();
	const FString successRate = !successRateOverride.IsEmpty()
		? successRateOverride
		: DashboardData.EpisodeCount > 0
		? FString::Printf(TEXT("%.0f%%"), 100.0 * static_cast<double>(DashboardData.SuccessCount) / DashboardData.EpisodeCount)
		: FString(TEXT("-"));
	UE_MVVM_SET_PROPERTY_VALUE(SuccessRateLabel, successRate);

	const FString collisionCountOverride = DashboardData.CollisionCountLabel.TrimStartAndEnd();
	UE_MVVM_SET_PROPERTY_VALUE(
		CollisionCountLabel,
		collisionCountOverride.IsEmpty()
			? FString::Printf(TEXT("%d"), DashboardData.CollisionCount)
			: collisionCountOverride);
	UE_MVVM_SET_PROPERTY_VALUE(
		AiSummaryText,
		DashboardData.AiSummary.IsEmpty() ? FString(TEXT("AI 분석 결과가 없습니다.")) : DashboardData.AiSummary);
}

void UExperimentResultViewModel::RebuildDashboardItemViewModels()
{
	EpisodeItems.Reset();
	EpisodeItems.Reserve(DashboardData.Episodes.Num());
	for (const FProjectRunEpisodeDashboardItem& episodeItem : DashboardData.Episodes)
	{
		UExperimentResultEpisodeViewModel* item = NewObject<UExperimentResultEpisodeViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializeFromDashboardItem(episodeItem);
		EpisodeItems.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EpisodeItems);

	SuggestionItems.Reset();
	SuggestionItems.Reserve(DashboardData.Suggestions.Num());
	for (const FProjectRunAiSuggestionDashboardItem& suggestionItem : DashboardData.Suggestions)
	{
		UExperimentResultSuggestionViewModel* item = NewObject<UExperimentResultSuggestionViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializeFromDashboardItem(suggestionItem);
		SuggestionItems.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SuggestionItems);

	InsightItems.Reset();
	InsightItems.Reserve(DashboardData.Insights.Num());
	for (const FProjectRunAnalysisInsightDashboardItem& insightItem : DashboardData.Insights)
	{
		UExperimentResultInsightViewModel* item = NewObject<UExperimentResultInsightViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializeFromDashboardItem(insightItem);
		InsightItems.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InsightItems);

	WarningItems.Reset();
	WarningItems.Reserve(DashboardData.Warnings.Num());
	for (const FString& warningText : DashboardData.Warnings)
	{
		UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializeItem(warningText, warningText, FString(), FString());
		WarningItems.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(WarningItems);
}
