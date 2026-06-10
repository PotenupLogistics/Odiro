#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EpisodeEditorRootWidget.generated.h"

enum class EEpisodeEditorViewMode : uint8;

class UButton;
class USizeBox;
class UEpisodeAssetPaletteWidget;
class UEpisodeEditorToolbarWidget;
class UEpisodeLlmPromptWidget;
class UEpisodePlaceableComponent;
class UEpisodePlaceableContextMenuWidget;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UEpisodeEditorRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Root")
	bool bShowAssetPaletteOnEditorSessionStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Root")
	bool bAutoRevealLlmPanelOnRightEdge = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelRevealRightEdgePixels = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelHideRightEdgePixels = 96.0f;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UEpisodeEditorToolbarWidget> ToolbarWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UButton> TopDownOrthoModeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UButton> PerspectiveModeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UWidget> PlaceableContextMenuPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UEpisodePlaceableContextMenuWidget> PlaceableContextMenuWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UWidget> AssetPalettePanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UEpisodeAssetPaletteWidget> AssetPaletteWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UWidget> LlmPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Root")
	TObjectPtr<UEpisodeLlmPromptWidget> EpisodeEditorLLMWidget;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	UEpisodeAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	void HideAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	UEpisodePlaceableContextMenuWidget* ShowPlaceableContextMenu(UEpisodePlaceableComponent* selectedPlaceable);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	void HidePlaceableContextMenu();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	void SetLlmPanelVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	void HandleEditorSessionStarted(bool bLoadedExistingEpisode);

	// 현재 view mode에 맞춰 두 모드 전환 버튼의 노출 상태를 갱신함.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Root")
	void RefreshViewModeButtons();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Root")
	UEpisodeAssetPaletteWidget* GetAssetPaletteWidget() const { return AssetPaletteWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Root")
	UEpisodeEditorToolbarWidget* GetToolbarWidget() const { return ToolbarWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Root")
	UEpisodePlaceableContextMenuWidget* GetPlaceableContextMenuWidget() const { return PlaceableContextMenuWidget.Get(); }

private:
	UFUNCTION()
	void HandleTopDownOrthoModeButtonClicked();

	UFUNCTION()
	void HandlePerspectiveModeButtonClicked();

	void BindViewModeButtons();
	void UnbindViewModeButtons();
	class AEpisodeEditorController* GetEditorController() const;

	void BindEditorLaunchSubsystem();
	void UnbindEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingEpisode);
	UWidget* ResolvePlaceableContextMenuVisibilityTarget() const;
	UWidget* ResolveAssetPaletteVisibilityTarget() const;
	UWidget* ResolveLlmPanelVisibilityTarget() const;
	void SetPanelVisibility(UWidget* targetWidget, bool bVisible) const;
	bool ShouldRevealLlmPanelFromMouseEdge() const;
	bool IsMouseOverWidget(const UWidget* targetWidget) const;

	FDelegateHandle AutoStartCompletedHandle;

	// keyboard toggle 등 외부 변경과 버튼 표시를 동기화하기 위한 최근 view mode 캐시.
	EEpisodeEditorViewMode LastSeenViewMode = static_cast<EEpisodeEditorViewMode>(0);
	bool bHasCachedViewMode = false;
};
