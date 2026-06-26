#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Styling/SlateTypes.h"
#include "ScenarioEditorRootWidget.generated.h"

enum class EScenarioEditorViewMode : uint8;

class UButton;
class USizeBox;
class UTextBlock;
class UScenarioAssetPaletteWidget;
class UScenarioEditorToolbarWidget;
class UScenarioEditorShellViewModel;
class UScenarioLlmPromptWidget;
class UScenarioEditorOutlinerWidget;
class UScenarioPlaceableComponent;
class UScenarioPlaceableContextMenuWidget;
class UScenarioPlaceableDetailsWidget;
class UScenarioEditorSidebarWidget;
class UTexture2D;
class UWidget;
class UWidgetSwitcher;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	// Controls whether the asset palette stays visible as a persistent editor surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bShowAssetPaletteOnEditorSessionStart = true;

	// Legacy reveal mode used only when the persistent asset palette is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bAutoRevealAssetPaletteOnBottomEdge = false;

	// Bottom-edge distance that reveals the asset palette.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float AssetPaletteRevealBottomEdgePixels = 24.0f;

	// Bottom-edge distance that keeps the asset palette visible after it has opened.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float AssetPaletteHideBottomEdgePixels = 96.0f;

	// Controls whether the LLM prompt panel appears while the cursor is near the right edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root")
	bool bAutoRevealLlmPanelOnRightEdge = false;

	// Right-edge distance that reveals the LLM prompt panel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelRevealRightEdgePixels = 24.0f;

	// Right-edge distance that keeps the LLM prompt panel visible after it has opened.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Root", meta = (ClampMin = "0.0"))
	float LlmPanelHideRightEdgePixels = 96.0f;

	// Texture drawn for the editor-only robot start marker overlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	TObjectPtr<UTexture2D> RobotStartMarkerOverlayTexture;

	// Texture drawn for the editor-only robot goal marker overlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	TObjectPtr<UTexture2D> RobotGoalMarkerOverlayTexture;

	// Fixed screen-space size for route marker overlays, preserving the default 78:120 marker aspect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D RobotRouteMarkerOverlaySize = FVector2D(39.0, 60.0);

	// Normalized overlay anchor point; the marker tip is expected at the bottom center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D RobotRouteMarkerOverlayAnchor = FVector2D(0.5, 1.0);

	// Fallback tint for the robot start marker when no texture is assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FLinearColor RobotStartMarkerOverlayTint = FLinearColor(0.0f, 0.48f, 1.0f, 0.82f);

	// Fallback tint for the robot goal marker when no texture is assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FLinearColor RobotGoalMarkerOverlayTint = FLinearColor(1.0f, 0.03f, 0.03f, 0.82f);

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorToolbarWidget> ToolbarWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UTextBlock> SaveStatusText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UScenarioEditorOutlinerWidget> ScenarioEditorOutlinerWidget;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidgetSwitcher> InspectorSwitcher;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> DetailInspectorPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UWidget> LlmInspectorPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> DetailInspectorTabButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> LlmInspectorTabButton;

	// Optional active-state tab style source; WBP owns the color and padding values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Root")
	TObjectPtr<UButton> InspectorActiveTabButtonStyleSource;

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

	// Legacy entry point that focuses the sidebar block for a placeable-backed editor item.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioPlaceableDetailsWidget* ShowPlaceableDetails(UScenarioPlaceableComponent* selectedPlaceable);

	// Hides the selection details panel and clears the selected placeable reference.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HidePlaceableDetails();

	// Legacy compatibility wrapper that routes old context-menu callers to sidebar focus.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	UScenarioPlaceableContextMenuWidget* ShowPlaceableContextMenu(UScenarioPlaceableComponent* selectedPlaceable);

	// Legacy compatibility wrapper for old context-menu callers.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void HidePlaceableContextMenu();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void SetLlmPanelVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void ShowInspectorTab(EScenarioEditorInspectorTab tab);

	// Applies the Scenario Template sidebar panel shown by the root widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void SetTemplateSidebarPanel(EScenarioTemplateSidebarPanel activePanel, bool bSyncOutlinerSelection = true);

	// Focuses the Scenario Template sidebar block represented by a selected viewport placeable.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	bool FocusSidebarForSelectedPlaceable(UScenarioPlaceableComponent* selectedPlaceable);

	// Refreshes the read-only Scenario Template sidebar from the authoring draft.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshTemplateSidebarWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Root")
	void RefreshScenarioInspector();

	// Refreshes after structural placeable changes that require an outliner registry rescan.
	void RefreshScenarioInspectorWithOutlinerRegistryRebuild();

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

	// Returns the subsystem-owned root shell ViewModel used for MVVM binding and command forwarding.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Root")
	UScenarioEditorShellViewModel* GetShellViewModel() const { return ShellViewModel; }

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
	void HandleSaveButtonClicked();

	UFUNCTION()
	void HandleDetailInspectorTabClicked();

	UFUNCTION()
	void HandleLlmInspectorTabClicked();

	UFUNCTION()
	void HandleOutlinerItemSelected(FScenarioOutlinerItemViewModel item);

	void BindEditorModeButtons();
	void UnbindEditorModeButtons();
	void BindSidebarControls();
	void UnbindSidebarControls();
	// Connects the root adapter to the subsystem-owned shell ViewModel.
	void InitializeViewModel();
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
	UWidget* ResolveDetailInspectorVisibilityTarget() const;
	// Refreshes sidebar/outliner while optionally invalidating the outliner placeable registry.
	void RefreshScenarioInspectorInternal(bool bRebuildOutlinerPlaceableRegistry);
	void SetSaveStatusText(const FString& message) const;
	void SyncOutlinerSelectionToPlaceable(const UScenarioPlaceableComponent* selectedPlaceable) const;
	void HandleControllerSelectedPlaceableChanged(const FString& selectedInstanceId);
	// Applies asset palette visibility without rebuilding it on every tick.
	void SetAssetPaletteVisible(bool bVisible, bool bRebuildWhenShowing = false);
	void SetPanelVisibility(UWidget* targetWidget, bool bVisible) const;
	// Captures WBP-authored inactive inspector tab button styles.
	void CacheInspectorTabButtonStyles();
	// Applies the active inspector tab style without changing the selected tab model.
	void ApplyInspectorTabVisualState();
	// Checks whether the cursor is near enough to the bottom edge to reveal the asset palette.
	bool ShouldRevealAssetPaletteFromMouseEdge() const;
	bool IsMouseOverWidget(const UWidget* targetWidget) const;
	FDelegateHandle AutoStartCompletedHandle;

	// Subsystem-owned shell ViewModel used by MVVM binding and command forwarding.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Scenario|Editor|Root", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UScenarioEditorShellViewModel> ShellViewModel;

	// Cached view-mode state used to keep keyboard toggles and button visibility in sync.
	EScenarioEditorViewMode LastSeenViewMode = static_cast<EScenarioEditorViewMode>(0);
	bool bHasCachedViewMode = false;
	bool bLastSeenPlacementSnapToGrid = false;
	bool bHasCachedPlacementSnapToGrid = false;
	EScenarioEditorInspectorTab ActiveInspectorTab = EScenarioEditorInspectorTab::Detail;
	// WBP-authored Detail tab style before active-state override.
	FButtonStyle DetailInspectorInactiveTabButtonStyle;
	// WBP-authored LLM tab style before active-state override.
	FButtonStyle LlmInspectorInactiveTabButtonStyle;
	// DetailInspectorInactiveTabButtonStyle snapshot 생성 여부.
	bool bHasDetailInspectorInactiveTabButtonStyle = false;
	// LlmInspectorInactiveTabButtonStyle snapshot 생성 여부.
	bool bHasLlmInspectorInactiveTabButtonStyle = false;
};
