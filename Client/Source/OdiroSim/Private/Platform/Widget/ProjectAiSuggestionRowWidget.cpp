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

	const FString title = suggestionItem->GetTitle().TrimStartAndEnd();
	const FString message = suggestionItem->GetSubtitle().TrimStartAndEnd();
	FString reason = suggestionItem->GetReason();
	if (!TitleText && !title.IsEmpty())
	{
		reason = reason.TrimStartAndEnd().IsEmpty()
			? title
			: FString::Printf(TEXT("%s\n%s"), *title, *reason.TrimStartAndEnd());
	}
	if (!MessageText && !message.IsEmpty())
	{
		reason = reason.TrimStartAndEnd().IsEmpty()
			? message
			: FString::Printf(TEXT("%s\n%s"), *reason.TrimStartAndEnd(), *message);
	}

	SetRuntimeText(SeverityText.Get(), suggestionItem->GetSeverityLabel());
	SetRuntimeText(TitleText.Get(), title);
	SetRuntimeText(MessageText.Get(), message);
	SetRuntimeText(ReasonText.Get(), reason);
	SetRuntimeText(RecommendationText.Get(), suggestionItem->GetRecommendation());
	SetRuntimeText(ParameterText.Get(), suggestionItem->GetParameterName());
	SetRuntimeText(CurrentValueText.Get(), suggestionItem->GetCurrentValue());
	SetRuntimeText(SuggestedValueText.Get(), suggestionItem->GetSuggestedValue());
	RefreshSeverityVisibility(suggestionItem->GetSeverity());
}

void UProjectAiSuggestionRowWidget::RefreshSeverityVisibility(const EProjectRunAiSuggestionSeverity severity) const
{
	SetIndicatorVisible(HighSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::High);
	SetIndicatorVisible(MediumSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Medium);
	SetIndicatorVisible(LowSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Low);
	SetIndicatorVisible(InfoSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Info);
}

void UProjectAiSuggestionRowWidget::SetRuntimeText(UBaseTextWidget* textWidget, const FString& text)
{
	if (!textWidget)
	{
		return;
	}

	const FString displayText = text.TrimStartAndEnd();
	textWidget->SetText(FText::FromString(displayText));
	textWidget->SetVisibility(displayText.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UProjectAiSuggestionRowWidget::SetIndicatorVisible(UWidget* widget, const bool bVisible)
{
	if (widget)
	{
		widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
