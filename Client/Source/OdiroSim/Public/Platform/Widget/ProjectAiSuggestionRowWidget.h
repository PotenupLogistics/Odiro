#pragma once

#include "CoreMinimal.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "CommonUserWidget.h"
#include "ProjectAiSuggestionRowWidget.generated.h"

class UExperimentResultSuggestionViewModel;
class UPanelWidget;
class UProjectAiSuggestionSectionWidget;
class UWidget;

// AI suggestion row adapter used by Platform run detail screens.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectAiSuggestionRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Applies suggestion ViewModel fields to WBP-owned labels and severity indicators.
	void InitializeFromSuggestionViewModel(const UExperimentResultSuggestionViewModel* suggestionItem);

protected:
	// WBP template used for each parsed reason/recommendation section.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|List")
	TSubclassOf<UProjectAiSuggestionSectionWidget> SuggestionSectionWidgetClass;

private:
	// Updates the severity indicator visibility from the current severity.
	void RefreshSeverityVisibility(EProjectRunAiSuggestionSeverity severity) const;

	// Applies runtime row text and collapses optional empty fields.
	static void SetRuntimeText(UWidget* textWidget, const FString& text, bool bAutoWrap = false);

	// Applies a runtime row text color to BaseText or native TextBlock widgets.
	static void SetRuntimeTextColor(UWidget* textWidget, const FLinearColor& color);

	// Rebuilds optional WBP-authored structured reason/recommendation sections.
	bool RebuildStructuredSections(
		const UExperimentResultSuggestionViewModel* suggestionItem,
		bool& bOutReasonRendered,
		bool& bOutRecommendationRendered);

	// Clears optional structured sections when fallback text rendering is used.
	void ClearStructuredSections() const;

	// Returns the shared severity color for the given suggestion severity.
	static FLinearColor ResolveSeverityTextColor(EProjectRunAiSuggestionSeverity severity);

	// Shows or hides one optional WBP indicator.
	static void SetIndicatorVisible(UWidget* widget, bool bVisible);

	// Suggestion severity label display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SeverityText;

	// Suggestion title display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TitleText;

	// Suggestion summary/message display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MessageText;

	// Suggestion reason display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ReasonText;

	// Suggested action display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecommendationText;

	// Parameter name display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ParameterText;

	// Current value display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CurrentValueText;

	// Suggested value display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SuggestedValueText;

	// Optional value pill row authored in WBP.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ValueRow;

	// Optional WBP-owned host for parsed reason/recommendation list sections.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StructuredSectionListBox;

	// Optional current value pill authored in WBP.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CurrentValuePill;

	// Optional suggested value pill authored in WBP.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SuggestedValuePill;

	// High severity visual marker.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HighSeverityIndicator;

	// Medium severity visual marker.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MediumSeverityIndicator;

	// Low severity visual marker.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LowSeverityIndicator;

	// Info severity visual marker.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> InfoSeverityIndicator;
};
