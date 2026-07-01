#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "ScenarioEditorScreenWidget.generated.h"

class UScenarioEditorRootWidget;

// Platform screen wrapper around ScenarioEditorRootWidget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Returns the wrapped scenario editor root widget.
	UFUNCTION(BlueprintPure, Category = "Platform|Scenario")
	UScenarioEditorRootWidget* GetScenarioEditorRootWidget() const { return ScenarioEditorRootWidget.Get(); }

	// Saves the current scenario through the wrapped editor root.
	UFUNCTION(BlueprintCallable, Category = "Platform|Scenario")
	void SaveCurrentScenario();

private:
	// Scenario editor root owned by the screen Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScenarioEditorRootWidget> ScenarioEditorRootWidget;
};
