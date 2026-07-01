#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "RobotConfigScreenWidget.generated.h"

class URobotConfigEditorWidget;

// Platform screen wrapper around the robot profile editor.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URobotConfigScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Reloads profile.json and applies it through the wrapped editor controls.
	UFUNCTION(BlueprintCallable, Category = "Platform|Robot")
	bool ReloadProfile();

	// Restores field values through the wrapped editor controls.
	UFUNCTION(BlueprintCallable, Category = "Platform|Robot")
	bool ResetProfileInputs();

	// Saves profile.json through the wrapped editor controls.
	UFUNCTION(BlueprintCallable, Category = "Platform|Robot")
	bool SaveProfile();

	// Returns the wrapped robot profile editor.
	UFUNCTION(BlueprintPure, Category = "Platform|Robot")
	URobotConfigEditorWidget* GetRobotConfigEditor() const { return RobotConfigEditor.Get(); }

private:
	// Robot profile editor owned by the screen Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<URobotConfigEditorWidget> RobotConfigEditor;
};
