#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
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
class UScenarioPlaceableDetailsWidget;
class UScenarioEditorSidebarWidget;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	// Controls whether the asset palette is shown immediately when auto reveal is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bShowAssetPaletteOnEditorSessionStart = true;

	// Controls whether the asset palette appears while the cursor is near the bottom edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bAutoRevealAssetPaletteOnBottomEdge = true;

	// Bottom-edge distance that reveals the asset palette.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float AssetPaletteRevealBottomEdgePixels = 24.0f;

	// Bottom-edge distance that keeps the asset palette visible after it has opened.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float AssetPaletteHideBottomEdgePixels = 96.0f;

	// Controls whether the LLM prompt panel appears while the cursor is near the right edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bAutoRevealLlmPanelOnRightEdge = true;

	// Right-edge distance that reveals the LLM prompt panel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelRevealRightEdgePixels = 24.0f;

	// Right-edge distance that keeps the LLM prompt panel visible after it has opened.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelHideRightEdgePixels = 96.0f;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorToolbarWidget> ToolbarWidget;

	// Optional visibility wrapper for the Scenario Template sidebar.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> TemplateSidebarPanel;

	// Optional Scenario Editor sidebar bound by the preferred UMG child name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorSidebarWidget> ScenarioEditorSidebarWidget;

	// Compatibility bind for the current Root WBP sidebar child name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorSidebarWidget> SidebarWidget;

	// Compatibility bind for the previous Scenario Template sidebar child name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorSidebarWidget> ScenarioTemplateSidebarWidget;

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

	// Legacy bind name used as the placeable details widget slot until the UMG tree is renamed.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioPlaceableDetailsWidget> PlaceableContextMenuWidget;

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

	// Shows the selection details panel for a placeable-backed editor item.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioPlaceableDetailsWidget* ShowPlaceableDetails(UScenarioPlaceableComponent* selectedPlaceable);

	// Hides the selection details panel and clears the selected placeable reference.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HidePlaceableDetails();

	// Legacy compatibility wrapper for old context-menu callers.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioPlaceableContextMenuWidget* ShowPlaceableContextMenu(UScenarioPlaceableComponent* selectedPlaceable);

	// Legacy compatibility wrapper for old context-menu callers.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HidePlaceableContextMenu();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void SetLlmPanelVisible(bool bVisible);

	// Selects the Scenario Template sidebar panel shown by the root widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void SetTemplateSidebarPanel(EScenarioTemplateSidebarPanel activePanel);

	// Refreshes the read-only Scenario Template sidebar from the authoring draft.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshTemplateSidebarWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HandleEditorSessionStarted(bool bLoadedExistingScenario);

	// Synchronizes the view-mode toggle buttons with the active editor view mode.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshViewModeButtons();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshPlacementSnapButton();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioAssetPaletteWidget* GetAssetPaletteWidget() const { return AssetPaletteWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioEditorToolbarWidget* GetToolbarWidget() const { return ToolbarWidget.Get(); }

	// Returns the active placeable details widget, including legacy context-menu UMG bindings.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioPlaceableDetailsWidget* GetPlaceableDetailsWidget() const { return PlaceableContextMenuWidget.Get(); }

	// Legacy compatibility wrapper for old context-menu callers.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioPlaceableContextMenuWidget* GetPlaceableContextMenuWidget() const;

private:
	UFUNCTION()
	void HandleTopDownOrthoModeButtonClicked();

	UFUNCTION()
	void HandlePerspectiveModeButtonClicked();

	UFUNCTION()
	void HandleSnapPlacementToGridButtonClicked();

	UFUNCTION()
	void HandleTemplateSidebarPanelChanged(EScenarioTemplateSidebarPanel activePanel);

	void BindEditorModeButtons();
	void UnbindEditorModeButtons();
	// Binds toolbar tab changes to the Scenario Template sidebar.
	void BindTemplateSidebarToolbar();
	// Releases toolbar tab bindings owned by the root widget.
	void UnbindTemplateSidebarToolbar();
	// Applies one template sidebar panel to the sidebar and records the synchronized value.
	void ApplyTemplateSidebarPanel(EScenarioTemplateSidebarPanel activePanel);
	class AScenarioEditorController* GetEditorController() const;

	void BindEditorLaunchSubsystem();
	void UnbindEditorLaunchSubsystem();
	void HandleAutoStartCompleted(bool bLoadedExistingScenario);
	UWidget* ResolvePlaceableDetailsVisibilityTarget() const;
	// Resolves the read-only Scenario Template sidebar across current and migrated UMG child names.
	UScenarioEditorSidebarWidget* ResolveTemplateSidebarWidget() const;
	UWidget* ResolveTemplateSidebarVisibilityTarget() const;
	UWidget* ResolveAssetPaletteVisibilityTarget() const;
	UWidget* ResolveLlmPanelVisibilityTarget() const;
	// Applies asset palette visibility without rebuilding it on every tick.
	void SetAssetPaletteVisible(bool bVisible, bool bRebuildWhenShowing = false);
	void SetPanelVisibility(UWidget* targetWidget, bool bVisible) const;
	// Checks whether the cursor is near enough to the bottom edge to reveal the asset palette.
	bool ShouldRevealAssetPaletteFromMouseEdge() const;
	bool ShouldRevealLlmPanelFromMouseEdge() const;
	bool IsMouseOverWidget(const UWidget* targetWidget) const;

	FDelegateHandle AutoStartCompletedHandle;

	// Cached view-mode state used to keep keyboard toggles and button visibility in sync.
	EScenarioEditorViewMode LastSeenViewMode = static_cast<EScenarioEditorViewMode>(0);
	bool bHasCachedViewMode = false;
	bool bLastSeenPlacementSnapToGrid = false;
	bool bHasCachedPlacementSnapToGrid = false;
};
