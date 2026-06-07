#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EpisodeEditorEntryWidget.generated.h"

class UButton;
class UEditableTextBox;
class UEpisodeAssetPaletteWidget;
class UEpisodeEditorRootWidget;
class UEpisodeEditorLaunchSubsystem;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UEpisodeEditorEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Entry")
	bool bHideOnSuccessfulStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Entry")
	bool bShowAssetPaletteOnSuccessfulStart = true;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UButton> NewEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UButton> LoadEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UEditableTextBox> EpisodeSetupJsonPathTextBox;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void StartNewEpisode();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	bool LoadEpisodeFromPathTextBox();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	UEpisodeAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void RemoveAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	bool CompleteExternallyStartedEpisode(bool bLoadedExistingEpisode);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Entry")
	UEpisodeAssetPaletteWidget* GetAssetPaletteWidget() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Episode|Editor|Entry")
	void OnEpisodeEditorSessionStarted(bool bLoadedExistingEpisode);

protected:
	UFUNCTION()
	void HandleNewEpisodeButtonClicked();

	UFUNCTION()
	void HandleLoadEpisodeButtonClicked();

private:
	void BindEpisodeEditorLaunchSubsystem();
	void UnbindEpisodeEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingEpisode);
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool FinishSuccessfulStart(bool bLoadedExistingEpisode);
	void HideAfterSuccessfulStartIfNeeded();
	UEpisodeEditorRootWidget* GetEditorRootWidget() const;

	FDelegateHandle AutoStartCompletedHandle;
	bool bExternalStartCompleted = false;
};
