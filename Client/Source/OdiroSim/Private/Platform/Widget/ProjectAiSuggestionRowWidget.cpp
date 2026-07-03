#include "Platform/Widget/ProjectAiSuggestionRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseTextWidget.h"

namespace
{
	// Appends one trimmed display line to a multi-line detail string.
	void AppendSuggestionDisplayLine(FString& output, const FString& line)
	{
		const FString trimmedLine = line.TrimStartAndEnd();
		if (trimmedLine.IsEmpty())
		{
			return;
		}

		if (!output.IsEmpty())
		{
			output += TEXT("\n");
		}
		output += trimmedLine;
	}

	// Chooses the strongest available title for the header row.
	FString BuildSuggestionTitle(const UExperimentResultSuggestionViewModel* suggestionItem)
	{
		if (!suggestionItem)
		{
			return FString();
		}

		FString title = suggestionItem->GetTitle().TrimStartAndEnd();
		if (!title.IsEmpty())
		{
			return title;
		}

		title = suggestionItem->GetParameterName().TrimStartAndEnd();
		if (!title.IsEmpty())
		{
			return title;
		}

		title = suggestionItem->GetRecommendation().TrimStartAndEnd();
		if (!title.IsEmpty())
		{
			return title;
		}

		return suggestionItem->GetReason().TrimStartAndEnd();
	}

	// Combines optional AI response fields into the WBP-authored detail line.
	FString BuildSuggestionDetail(const UExperimentResultSuggestionViewModel* suggestionItem)
	{
		FString detail;
		if (!suggestionItem)
		{
			return detail;
		}

		AppendSuggestionDisplayLine(detail, suggestionItem->GetSubtitle());
		AppendSuggestionDisplayLine(detail, suggestionItem->GetReason());
		return detail;
	}
}

void UProjectAiSuggestionRowWidget::InitializeFromSuggestionViewModel(
	const UExperimentResultSuggestionViewModel* suggestionItem)
{
	if (!suggestionItem)
	{
		return;
	}

	const FString title = BuildSuggestionTitle(suggestionItem);
	const FString detail = BuildSuggestionDetail(suggestionItem);
	const FString recommendation = suggestionItem->GetRecommendation().TrimStartAndEnd();
	const bool bParameterTextActsAsTitle = !TitleText && ParameterText;

	SetRuntimeText(SeverityText.Get(), suggestionItem->GetSeverityLabel());
	SetRuntimeTextColor(SeverityText.Get(), ResolveSeverityTextColor(suggestionItem->GetSeverity()));
	SetRuntimeText(TitleText.Get(), title);
	SetRuntimeText(MessageText.Get(), FString());
	SetRuntimeText(ReasonText.Get(), detail, true);
	SetRuntimeText(RecommendationText.Get(), recommendation, true);
	SetRuntimeText(ParameterText.Get(), bParameterTextActsAsTitle ? title : suggestionItem->GetParameterName());
	SetRuntimeText(CurrentValueText.Get(), FString());
	SetRuntimeText(SuggestedValueText.Get(), FString());
	SetIndicatorVisible(CurrentValuePill.Get(), false);
	SetIndicatorVisible(SuggestedValuePill.Get(), false);
	SetIndicatorVisible(ValueRow.Get(), false);
	RefreshSeverityVisibility(suggestionItem->GetSeverity());
}

void UProjectAiSuggestionRowWidget::RefreshSeverityVisibility(const EProjectRunAiSuggestionSeverity severity) const
{
	SetIndicatorVisible(HighSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::High);
	SetIndicatorVisible(MediumSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Medium);
	SetIndicatorVisible(LowSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Low);
	SetIndicatorVisible(InfoSeverityIndicator.Get(), severity == EProjectRunAiSuggestionSeverity::Info);
}

void UProjectAiSuggestionRowWidget::SetRuntimeText(UWidget* textWidget, const FString& text, const bool bAutoWrap)
{
	if (!textWidget)
	{
		return;
	}

	const FString displayText = text.TrimStartAndEnd();
	if (UBaseTextWidget* baseTextWidget = Cast<UBaseTextWidget>(textWidget))
	{
		baseTextWidget->SetText(FText::FromString(displayText));
		baseTextWidget->SetAutoWrapText(bAutoWrap);
	}
	else if (UTextBlock* textBlock = Cast<UTextBlock>(textWidget))
	{
		textBlock->SetText(FText::FromString(displayText));
		textBlock->SetAutoWrapText(bAutoWrap);
	}

	textWidget->SetVisibility(displayText.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UProjectAiSuggestionRowWidget::SetRuntimeTextColor(UWidget* textWidget, const FLinearColor& color)
{
	if (UBaseTextWidget* baseTextWidget = Cast<UBaseTextWidget>(textWidget))
	{
		baseTextWidget->SetColorAndOpacityOverride(color);
		return;
	}

	if (UTextBlock* textBlock = Cast<UTextBlock>(textWidget))
	{
		textBlock->SetColorAndOpacity(FSlateColor(color));
	}
}

FLinearColor UProjectAiSuggestionRowWidget::ResolveSeverityTextColor(
	const EProjectRunAiSuggestionSeverity severity)
{
	const UBaseWidgetColorCatalog* colors =
		UBaseWidgetColorCatalog::ResolveCatalog(UBaseWidgetColorCatalog::MakeDefaultCatalogReference());
	if (colors)
	{
		switch (severity)
		{
		case EProjectRunAiSuggestionSeverity::High:
			return colors->StatusDangerColor;
		case EProjectRunAiSuggestionSeverity::Medium:
			return colors->StatusWarnColor;
		case EProjectRunAiSuggestionSeverity::Low:
			return colors->StatusSuccessColor;
		case EProjectRunAiSuggestionSeverity::Info:
		default:
			return colors->StatusInfoColor;
		}
	}

	switch (severity)
	{
	case EProjectRunAiSuggestionSeverity::High:
		return FLinearColor(0.898f, 0.325f, 0.294f, 1.0f);
	case EProjectRunAiSuggestionSeverity::Medium:
		return FLinearColor(0.878f, 0.627f, 0.188f, 1.0f);
	case EProjectRunAiSuggestionSeverity::Low:
		return FLinearColor(0.298f, 0.686f, 0.314f, 1.0f);
	case EProjectRunAiSuggestionSeverity::Info:
	default:
		return FLinearColor(0.290f, 0.624f, 0.961f, 1.0f);
	}
}

void UProjectAiSuggestionRowWidget::SetIndicatorVisible(UWidget* widget, const bool bVisible)
{
	if (widget)
	{
		widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
