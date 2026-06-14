#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioEditorRootWidget.generated.h"

enum class EScenarioEditorViewMode : uint8;

class UButton;
class USizeBox;
class UTextBlock;
class UScenarioAssetPaletteWidget;
class UScenarioEditorToolbarWidget;
class UScenarioLlmPromptWidget;
class UScenarioPlaceableComponent;
class UScenarioPlaceableContextMenuWidget;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bShowAssetPaletteOnEditorSessionStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bAutoRevealLlmPanelOnRightEdge = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelRevealRightEdgePixels = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelHideRightEdgePixels = 96.0f;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorToolbarWidget> ToolbarWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> TopDownOrthoModeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> PerspectiveModeButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> SnapPlacementToGridButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UTextBlock> SnapPlacementToGridButtonText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> PlaceableContextMenuPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioPlaceableContextMenuWidget> PlaceableContextMenuWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> AssetPalettePanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioAssetPaletteWidget> AssetPaletteWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> LlmPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioLlmPromptWidget> ScenarioEditorLlmWidget;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioAssetPaletteWidget* ShowAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HideAssetPaletteWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioPlaceableContextMenuWidget* ShowPlaceableContextMenu(UScenarioPlaceableComponent* selectedPlaceable);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HidePlaceableContextMenu();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void SetLlmPanelVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HandleEditorSessionStarted(bool bLoadedExistingScenario);

	// 현재 view mode에 맞춰 두 모드 전환 버튼의 노출 상태를 갱신함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshViewModeButtons();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshPlacementSnapButton();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioAssetPaletteWidget* GetAssetPaletteWidget() const { return AssetPaletteWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioEditorToolbarWidget* GetToolbarWidget() const { return ToolbarWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioPlaceableContextMenuWidget* GetPlaceableContextMenuWidget() const { return PlaceableContextMenuWidget.Get(); }

private:
	UFUNCTION()
	void HandleTopDownOrthoModeButtonClicked();

	UFUNCTION()
	void HandlePerspectiveModeButtonClicked();

	UFUNCTION()
	void HandleSnapPlacementToGridButtonClicked();

	void BindEditorModeButtons();
	void UnbindEditorModeButtons();
	class AScenarioEditorController* GetEditorController() const;

	void BindEditorLaunchSubsystem();
	void UnbindEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingScenario);
	UWidget* ResolvePlaceableContextMenuVisibilityTarget() const;
	UWidget* ResolveAssetPaletteVisibilityTarget() const;
	UWidget* ResolveLlmPanelVisibilityTarget() const;
	void SetPanelVisibility(UWidget* targetWidget, bool bVisible) const;
	bool ShouldRevealLlmPanelFromMouseEdge() const;
	bool IsMouseOverWidget(const UWidget* targetWidget) const;

	FDelegateHandle AutoStartCompletedHandle;

	// keyboard toggle 등 외부 변경과 버튼 표시를 동기화하기 위한 최근 view mode 캐시.
	EScenarioEditorViewMode LastSeenViewMode = static_cast<EScenarioEditorViewMode>(0);
	bool bHasCachedViewMode = false;
	bool bLastSeenPlacementSnapToGrid = false;
	bool bHasCachedPlacementSnapToGrid = false;
};
