#pragma once

#include "CoreMinimal.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "ProjectAiSuggestionRowWidget.generated.h"

class UBaseTextWidget;
class UExperimentResultSuggestionViewModel;
class UWidget;

// AI suggestion row adapter used by Platform run detail screens.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectAiSuggestionRowWidget : public UOdiroCommonUserWidget
{
	GENERATED_BODY()

public:
	// Applies suggestion ViewModel fields to WBP-owned labels and severity indicators.
	void InitializeFromSuggestionViewModel(const UExperimentResultSuggestionViewModel* suggestionItem);

private:
	// Updates the severity indicator visibility from the current severity.
	void RefreshSeverityVisibility(EProjectRunAiSuggestionSeverity severity) const;

	// Shows or hides one optional WBP indicator.
	static void SetIndicatorVisible(UWidget* widget, bool bVisible);

	// Suggestion severity label display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SeverityText;

	// Suggestion reason display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ReasonText;

	// Suggested action display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> RecommendationText;

	// Parameter name display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ParameterText;

	// Current value display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> CurrentValueText;

	// Suggested value display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SuggestedValueText;

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
