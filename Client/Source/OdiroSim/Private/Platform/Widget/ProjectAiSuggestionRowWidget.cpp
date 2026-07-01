#include "Platform/Widget/ProjectAiSuggestionRowWidget.h"

#include "Components/Widget.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "UI/BaseTextWidget.h"

void UProjectAiSuggestionRowWidget::InitializeFromSuggestionViewModel(
	const UExperimentResultSuggestionViewModel* suggestionItem)
{
	if (!suggestionItem)
	{
		return;
	}

	if (SeverityText)
	{
		SeverityText->SetText(FText::FromString(suggestionItem->GetSeverityLabel()));
	}
	if (ReasonText)
	{
		ReasonText->SetText(FText::FromString(suggestionItem->GetReason()));
	}
	if (RecommendationText)
	{
		RecommendationText->SetText(FText::FromString(suggestionItem->GetRecommendation()));
	}
	if (ParameterText)
	{
		ParameterText->SetText(FText::FromString(suggestionItem->GetParameterName()));
	}
	if (CurrentValueText)
	{
		CurrentValueText->SetText(FText::FromString(suggestionItem->GetCurrentValue()));
	}
	if (SuggestedValueText)
	{
		SuggestedValueText->SetText(FText::FromString(suggestionItem->GetSuggestedValue()));
	}

	RefreshSeverityVisibility(suggestionItem->GetSeverity());
}

void UProjectAiSuggestionRowWidget::RefreshSeverityVisibility(const EProjectRunAiSuggestionSeverity severity) const
{
	SetIndicatorVisible(HighSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::High);
	SetIndicatorVisible(MediumSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Medium);
	SetIndicatorVisible(LowSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Low);
	SetIndicatorVisible(InfoSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Info);
}

void UProjectAiSuggestionRowWidget::SetIndicatorVisible(UWidget* widget, const bool bVisible)
{
	if (widget)
	{
		widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
