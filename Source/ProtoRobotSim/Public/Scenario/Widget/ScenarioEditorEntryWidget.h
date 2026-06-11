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
class PROTOROBOTSIM_API UScenarioEditorEntryWidget : public UUserWidget
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
	TObjectPtr<UButton> NewEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Entry")
	TObjectPtr<UButton> LoadEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Entry")
	TObjectPtr<UEditableTextBox> EpisodeSetupJsonPathTextBox;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	void StartNewEpisode();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	bool LoadEpisodeFromPathTextBox();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	UScenarioAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	void RemoveAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Entry")
	bool CompleteExternallyStartedEpisode(bool bLoadedExistingEpisode);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Entry")
	UScenarioAssetPaletteWidget* GetAssetPaletteWidget() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scenario|Editor|Entry")
	void OnEpisodeEditorSessionStarted(bool bLoadedExistingEpisode);

protected:
	UFUNCTION()
	void HandleNewEpisodeButtonClicked();

	UFUNCTION()
	void HandleLoadEpisodeButtonClicked();

private:
	void BindScenarioEditorLaunchSubsystem();
	void UnbindScenarioEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingEpisode);
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool FinishSuccessfulStart(bool bLoadedExistingEpisode);
	void HideAfterSuccessfulStartIfNeeded();
	UScenarioEditorRootWidget* GetEditorRootWidget() const;

	FDelegateHandle AutoStartCompletedHandle;
	bool bExternalStartCompleted = false;
};
