#include "Platform/ViewModel/ExperimentResultItemViewModels.h"

namespace
{
	// Converts stored numeric/string episode ids into the compact dashboard label.
	FString FormatExperimentResultEpisodeLabel(const FString& episodeId)
	{
		const FString trimmedEpisodeId = episodeId.TrimStartAndEnd();
		if (trimmedEpisodeId.IsEmpty())
		{
			return TEXT("Episode");
		}

		return FString::Printf(TEXT("Episode %s"), *trimmedEpisodeId);
	}

	// Converts raw duration seconds into the dashboard card display string.
	FString FormatExperimentResultEpisodeDuration(const double durationSeconds)
	{
		return FString::Printf(TEXT("%.1f s"), durationSeconds);
	}
}

void UExperimentResultEpisodeViewModel::InitializeFromDashboardItem(
	const FProjectRunEpisodeDashboardItem& episodeItem)
{
	SetEpisodeId(episodeItem.EpisodeId);
	SetDurationLabel(FormatExperimentResultEpisodeDuration(episodeItem.DurationSeconds));
	SetSuccess(episodeItem.bSuccess);
	SetHasPreviewImage(!episodeItem.PreviewImagePath.IsEmpty());
	InitializeItem(
		episodeItem.EpisodeId,
		FormatExperimentResultEpisodeLabel(episodeItem.EpisodeId),
		DurationLabel,
		episodeItem.PreviewImagePath);
}

void UExperimentResultEpisodeViewModel::SetEpisodeId(const FString& episodeId)
{
	UE_MVVM_SET_PROPERTY_VALUE(EpisodeId, episodeId.TrimStartAndEnd());
}

void UExperimentResultEpisodeViewModel::SetDurationLabel(const FString& durationLabel)
{
	UE_MVVM_SET_PROPERTY_VALUE(DurationLabel, durationLabel.TrimStartAndEnd());
}

void UExperimentResultEpisodeViewModel::SetSuccess(const bool bInSuccess)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSuccess, bInSuccess);
}

void UExperimentResultEpisodeViewModel::SetHasPreviewImage(const bool bInHasPreviewImage)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasPreviewImage, bInHasPreviewImage);
}

void UExperimentResultSuggestionViewModel::InitializeFromDashboardItem(
	const FProjectRunAiSuggestionDashboardItem& suggestionItem)
{
	SetSeverity(suggestionItem.Severity);
	SetSeverityLabel(suggestionItem.SeverityLabel);
	SetReason(suggestionItem.Reason);
	SetRecommendation(suggestionItem.Recommendation);
	SetParameterName(suggestionItem.ParameterName);
	SetCurrentValue(suggestionItem.CurrentValue);
	SetSuggestedValue(suggestionItem.SuggestedValue);
	InitializeItem(
		suggestionItem.ParameterName,
		suggestionItem.Title,
		suggestionItem.Message,
		FString());
}

void UExperimentResultSuggestionViewModel::SetSeverity(const EProjectRunAiSuggestionSeverity severity)
{
	UE_MVVM_SET_PROPERTY_VALUE(Severity, severity);
}

void UExperimentResultSuggestionViewModel::SetSeverityLabel(const FString& severityLabel)
{
	UE_MVVM_SET_PROPERTY_VALUE(SeverityLabel, severityLabel.TrimStartAndEnd());
}

void UExperimentResultSuggestionViewModel::SetReason(const FString& reason)
{
	UE_MVVM_SET_PROPERTY_VALUE(Reason, reason.TrimStartAndEnd());
}

void UExperimentResultSuggestionViewModel::SetRecommendation(const FString& recommendation)
{
	UE_MVVM_SET_PROPERTY_VALUE(Recommendation, recommendation.TrimStartAndEnd());
}

void UExperimentResultSuggestionViewModel::SetParameterName(const FString& parameterName)
{
	UE_MVVM_SET_PROPERTY_VALUE(ParameterName, parameterName.TrimStartAndEnd());
}

void UExperimentResultSuggestionViewModel::SetCurrentValue(const FString& currentValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentValue, currentValue.TrimStartAndEnd());
}

void UExperimentResultSuggestionViewModel::SetSuggestedValue(const FString& suggestedValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(SuggestedValue, suggestedValue.TrimStartAndEnd());
}
