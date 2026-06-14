#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioEditorEntryWidget.generated.h"

class UButton;
class UEditableTextBox;
class UScenarioAssetPaletteWidget;
class UScenarioEditorRootWidget;
class UScenarioEditorLaunchSubsystem;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Entry")
	bool bHideOnSuccessfulStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Entry")
	bool bShowAssetPaletteOnSuccessfulStart = true;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Entry")
	TObjectPtr<UButton> NewScenarioButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Entry")
	TObjectPtr<UButton> LoadScenarioButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Entry")
	TObjectPtr<UEditableTextBox> ScenarioSetupJsonPathTextBox;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	void StartNewScenario();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	bool LoadScenarioFromPathTextBox();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	UScenarioAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	void RemoveAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	bool CompleteExternallyStartedScenario(bool bLoadedExistingScenario);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Entry")
	UScenarioAssetPaletteWidget* GetAssetPaletteWidget() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scenario|Editor|Entry")
	void OnScenarioEditorSessionStarted(bool bLoadedExistingScenario);

protected:
	UFUNCTION()
	void HandleNewScenarioButtonClicked();

	UFUNCTION()
	void HandleLoadScenarioButtonClicked();

private:
	void BindScenarioEditorLaunchSubsystem();
	void UnbindScenarioEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingScenario);
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool FinishSuccessfulStart(bool bLoadedExistingScenario);
	void HideAfterSuccessfulStartIfNeeded();
	UScenarioEditorRootWidget* GetEditorRootWidget() const;

	FDelegateHandle AutoStartCompletedHandle;
	bool bExternalStartCompleted = false;
};
