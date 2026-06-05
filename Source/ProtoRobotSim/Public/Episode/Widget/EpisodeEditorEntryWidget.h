#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EpisodeEditorEntryWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UEpisodeAssetPaletteWidget;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Entry")
	TSubclassOf<UEpisodeAssetPaletteWidget> AssetPaletteWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Entry")
	int32 AssetPaletteViewportZOrder = 1;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UButton> NewEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UButton> LoadEpisodeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UEditableTextBox> EpisodeSetupJsonPathTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Entry")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void StartNewEpisode();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	bool LoadEpisodeFromPathTextBox();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void SetDiagnosticsFromLines(const TArray<FString>& diagnostics);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void SetDiagnosticsText(const FString& diagnostics);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	UEpisodeAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Entry")
	void RemoveAssetPaletteWidget();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Entry")
	UEpisodeAssetPaletteWidget* GetAssetPaletteWidget() const { return AssetPaletteWidget; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Episode|Editor|Entry")
	void OnEpisodeEditorSessionStarted(bool bLoadedExistingEpisode);

protected:
	UFUNCTION()
	void HandleNewEpisodeButtonClicked();

	UFUNCTION()
	void HandleLoadEpisodeButtonClicked();

private:
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool FinishSuccessfulStart(bool bLoadedExistingEpisode);
	void HideAfterSuccessfulStartIfNeeded();

	UPROPERTY(Transient)
	TObjectPtr<UEpisodeAssetPaletteWidget> AssetPaletteWidget;
};
