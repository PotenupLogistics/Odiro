#include "Platform/Widget/ProjectAiSuggestionRowWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "Platform/Widget/ProjectAiSuggestionSectionWidget.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseTextWidget.h"

namespace
{
	// Parsed display section from API strings such as "이유\n- ...".
	struct FSuggestionTextSection
	{
		// Two-character UI header shown at the left of the bullet list.
		FString Header;

		// Bullet list items displayed to the right of Header.
		TArray<FString> Items;
	};

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

	// Returns the compact row header for known API section titles.
	FString NormalizeSuggestionSectionHeader(const FString& rawHeader)
	{
		const FString header = rawHeader.TrimStartAndEnd();
		if (header == TEXT("확인 항목"))
		{
			return TEXT("확인");
		}
		return header;
	}

	// Returns true for section headings produced by the Result Analysis v2 API.
	bool IsKnownSuggestionSectionHeader(const FString& line)
	{
		const FString header = line.TrimStartAndEnd();
		return header == TEXT("이유")
			|| header == TEXT("확인 항목")
			|| header == TEXT("확인")
			|| header == TEXT("관찰")
			|| header == TEXT("해석");
	}

	// Removes the API bullet marker while preserving the displayed text.
	FString StripSuggestionBulletPrefix(const FString& line)
	{
		FString item = line.TrimStartAndEnd();
		if (item.StartsWith(TEXT("-")))
		{
			item.RightChopInline(1);
		}
		return item.TrimStartAndEnd();
	}

	// Parses API-authored heading plus dash-list text into display sections.
	bool ParseSuggestionTextSections(const FString& text, TArray<FSuggestionTextSection>& outSections)
	{
		outSections.Reset();

		TArray<FString> lines;
		text.ParseIntoArrayLines(lines, false);

		FSuggestionTextSection* activeSection = nullptr;
		for (const FString& rawLine : lines)
		{
			const FString line = rawLine.TrimStartAndEnd();
			if (line.IsEmpty())
			{
				continue;
			}

			if (IsKnownSuggestionSectionHeader(line))
			{
				FSuggestionTextSection& newSection = outSections.AddDefaulted_GetRef();
				newSection.Header = NormalizeSuggestionSectionHeader(line);
				activeSection = &newSection;
				continue;
			}

			if (line.StartsWith(TEXT("-")) && activeSection)
			{
				const FString item = StripSuggestionBulletPrefix(line);
				if (!item.IsEmpty())
				{
					activeSection->Items.Add(item);
				}
				continue;
			}

			if (activeSection && activeSection->Items.IsEmpty())
			{
				activeSection->Items.Add(line);
			}
			else
			{
				outSections.Reset();
				return false;
			}
		}

		outSections.RemoveAll(
			[](const FSuggestionTextSection& section)
			{
				return section.Header.IsEmpty() || section.Items.IsEmpty();
			});
		return !outSections.IsEmpty();
	}

	// Appends parsed sections from one API field into a display section list.
	bool AppendParsedSuggestionSections(const FString& text, TArray<FSuggestionTextSection>& outSections)
	{
		TArray<FSuggestionTextSection> parsedSections;
		if (!ParseSuggestionTextSections(text, parsedSections))
		{
			return false;
		}

		outSections.Append(MoveTemp(parsedSections));
		return true;
	}

	// Formats parsed sections as a compact header/list row for the existing WBP text slots.
	FString FormatSuggestionTextSections(const TArray<FSuggestionTextSection>& sections)
	{
		FString output;
		for (const FSuggestionTextSection& section : sections)
		{
			if (section.Header.IsEmpty() || section.Items.IsEmpty())
			{
				continue;
			}

			if (!output.IsEmpty())
			{
				output += TEXT("\n");
			}

			const FString firstLinePrefix = FString::Printf(TEXT("%s │ "), *section.Header);
			const FString continuationPrefix = TEXT("     │ ");
			bool bFirstItem = true;
			for (const FString& item : section.Items)
			{
				const FString trimmedItem = item.TrimStartAndEnd();
				if (trimmedItem.IsEmpty())
				{
					continue;
				}

				if (!bFirstItem)
				{
					output += TEXT("\n");
				}

				output += bFirstItem ? firstLinePrefix : continuationPrefix;
				output += TEXT("- ");
				output += trimmedItem;
				bFirstItem = false;
			}
		}
		return output;
	}

	// Converts API-authored structured text to compact row text; leaves free text unchanged.
	FString FormatStructuredSuggestionText(const FString& text)
	{
		TArray<FSuggestionTextSection> sections;
		if (ParseSuggestionTextSections(text, sections))
		{
			const FString formattedText = FormatSuggestionTextSections(sections).TrimStartAndEnd();
			if (!formattedText.IsEmpty())
			{
				return formattedText;
			}
		}
		return text.TrimStartAndEnd();
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
	FString BuildSuggestionDetail(
		const UExperimentResultSuggestionViewModel* suggestionItem,
		const bool bOmitReason)
	{
		FString detail;
		if (!suggestionItem)
		{
			return detail;
		}

		AppendSuggestionDisplayLine(detail, suggestionItem->GetSubtitle());
		if (!bOmitReason)
		{
			AppendSuggestionDisplayLine(detail, FormatStructuredSuggestionText(suggestionItem->GetReason()));
		}
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
	bool bReasonRendered = false;
	bool bRecommendationRendered = false;
	RebuildStructuredSections(suggestionItem, bReasonRendered, bRecommendationRendered);
	const FString detail = BuildSuggestionDetail(suggestionItem, bReasonRendered);
	const FString recommendation = bRecommendationRendered
		? FString()
		: FormatStructuredSuggestionText(suggestionItem->GetRecommendation());
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

bool UProjectAiSuggestionRowWidget::RebuildStructuredSections(
	const UExperimentResultSuggestionViewModel* suggestionItem,
	bool& bOutReasonRendered,
	bool& bOutRecommendationRendered)
{
	bOutReasonRendered = false;
	bOutRecommendationRendered = false;

	const TSubclassOf<UProjectAiSuggestionSectionWidget> sectionWidgetClass = ResolveSuggestionSectionWidgetClass();
	if (!suggestionItem || !StructuredSectionListBox || !sectionWidgetClass)
	{
		ClearStructuredSections();
		return false;
	}

	TArray<FSuggestionTextSection> reasonSections;
	TArray<FSuggestionTextSection> recommendationSections;
	const bool bReasonParsed = AppendParsedSuggestionSections(suggestionItem->GetReason(), reasonSections);
	const bool bRecommendationParsed =
		AppendParsedSuggestionSections(suggestionItem->GetRecommendation(), recommendationSections);

	TArray<FSuggestionTextSection> sections;
	sections.Append(reasonSections);
	sections.Append(recommendationSections);
	if (sections.IsEmpty())
	{
		ClearStructuredSections();
		return false;
	}

	StructuredSectionListBox->ClearChildren();
	bool bRenderedAnySection = false;
	for (const FSuggestionTextSection& section : sections)
	{
		if (section.Header.IsEmpty() || section.Items.IsEmpty())
		{
			continue;
		}

		UProjectAiSuggestionSectionWidget* sectionWidget =
			CreateWidget<UProjectAiSuggestionSectionWidget>(this, sectionWidgetClass);
		if (!sectionWidget)
		{
			continue;
		}

		sectionWidget->InitializeSection(section.Header, section.Items);
		StructuredSectionListBox->AddChild(sectionWidget);
		bRenderedAnySection = true;
	}

	StructuredSectionListBox->SetVisibility(
		bRenderedAnySection ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	bOutReasonRendered = bRenderedAnySection && bReasonParsed;
	bOutRecommendationRendered = bRenderedAnySection && bRecommendationParsed;
	return bRenderedAnySection;
}

TSubclassOf<UProjectAiSuggestionSectionWidget> UProjectAiSuggestionRowWidget::ResolveSuggestionSectionWidgetClass() const
{
	if (SuggestionSectionWidgetClass)
	{
		return SuggestionSectionWidgetClass;
	}

	if (!StructuredSectionListBox)
	{
		return nullptr;
	}

	for (int32 childIndex = 0; childIndex < StructuredSectionListBox->GetChildrenCount(); ++childIndex)
	{
		if (const UProjectAiSuggestionSectionWidget* previewSection =
			Cast<UProjectAiSuggestionSectionWidget>(StructuredSectionListBox->GetChildAt(childIndex)))
		{
			return previewSection->GetClass();
		}
	}

	return nullptr;
}

void UProjectAiSuggestionRowWidget::ClearStructuredSections() const
{
	if (StructuredSectionListBox)
	{
		StructuredSectionListBox->ClearChildren();
		StructuredSectionListBox->SetVisibility(ESlateVisibility::Collapsed);
	}
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
