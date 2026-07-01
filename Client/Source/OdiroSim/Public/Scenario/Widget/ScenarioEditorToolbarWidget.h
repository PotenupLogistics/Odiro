#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorToolbarWidget.generated.h"

class UButton;
class UScenarioEditorToolbarViewModel;
class UTextBlock;
class UWidget;

// Notifies root widgets when the toolbar-selected template sidebar panel changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioTemplateSidebarPanelChanged,
	EScenarioTemplateSidebarPanel,
	ActivePanel);

// ScenarioEditorMap에서 저장과 root shell startup screen 복귀를 제공하는 최소 toolbar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString DefaultSavePath = TEXT("Saved/UserProjects/ScenarioEditor/scenario.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString StartupMapId = TEXT("ScenarioEditorMap");

	// Currently selected template sidebar panel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	EScenarioTemplateSidebarPanel ActiveSidebarPanel = EScenarioTemplateSidebarPanel::Main;

	// Broadcasts when the user switches Scenario Template sidebar panels.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Toolbar")
	FScenarioTemplateSidebarPanelChanged OnSidebarPanelChanged;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	bool SaveScenario();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void ReturnToMainMenu();

	// Selects the active Scenario Template sidebar panel and refreshes toolbar button state.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SetActiveSidebarPanel(EScenarioTemplateSidebarPanel panel);

	// Selects the root Scenario Template fields panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectMainSidebarPanel();

	// Selects the Corridor fields panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectCorridorSidebarPanel();

	// Selects the static obstacle fields panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectObstacleSidebarPanel();

	// Selects the pedestrian fields panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectPedestrianSidebarPanel();

	// Selects the move transform gizmo tool.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectMoveTransformTool();

	// Selects the rotate transform gizmo tool.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectRotateTransformTool();

	// Selects world-space gizmo coordinates.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectWorldCoordinateMode();

	// Selects local-space gizmo coordinates.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectLocalCoordinateMode();

	// Switches the editor viewport to top-down orthographic mode.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectTopDownOrthoViewMode();

	// Switches the editor viewport to perspective mode.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectPerspectiveViewMode();

	// Returns the active Scenario Template sidebar panel.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	EScenarioTemplateSidebarPanel GetActiveSidebarPanel() const { return ActiveSidebarPanel; }

	// Returns the toolbar ViewModel injected from the world UI subsystem.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	UScenarioEditorToolbarViewModel* GetToolbarViewModel() const { return ToolbarViewModel; }

	// Applies the active-panel visual state to optional toolbar panel buttons.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void RefreshSidebarPanelButtons();

	// Applies the active transform command visual state to optional toolbar buttons.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void RefreshTransformCommandButtons();

	// Applies the active viewport mode visual state to optional toolbar buttons.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void RefreshViewModeButtons();

protected:
	// Handles save button clicks from the toolbar UMG tree.
	UFUNCTION()
	void HandleSaveButtonClicked();

	// Handles return button clicks from the toolbar UMG tree.
	UFUNCTION()
	void HandleReturnButtonClicked();

	// Switches the sidebar to the Main template block.
	UFUNCTION()
	void HandleMainPanelButtonClicked();

	// Switches the sidebar to the Corridor template block.
	UFUNCTION()
	void HandleCorridorPanelButtonClicked();

	// Switches the sidebar to the Obstacle template block.
	UFUNCTION()
	void HandleObstaclePanelButtonClicked();

	// Switches the sidebar to the Pedestrian template block.
	UFUNCTION()
	void HandlePedestrianPanelButtonClicked();

	// Switches the gizmo to move mode.
	UFUNCTION()
	void HandleMoveButtonClicked();

	// Switches the gizmo to rotate mode.
	UFUNCTION()
	void HandleRotateButtonClicked();

	// Switches the gizmo to world-space coordinates.
	UFUNCTION()
	void HandleWorldCoordinateButtonClicked();

	// Switches the gizmo to local-space coordinates.
	UFUNCTION()
	void HandleLocalCoordinateButtonClicked();

	// Switches the editor viewport to top-down orthographic mode.
	UFUNCTION()
	void HandleTopDownOrthoViewButtonClicked();

	// Switches the editor viewport to perspective mode.
	UFUNCTION()
	void HandlePerspectiveViewButtonClicked();

private:
	// Binds optional toolbar buttons exposed by the UMG tree.
	void BindControls();
	// Mirrors ViewModel-driven panel changes back through the widget delegate.
	void SyncSidebarPanelFromViewModel();
	// Requests editor widget input mode while toolbar fields may receive focus.
	void RequestEditorWidgetInputMode();
	// Releases toolbar input-mode ownership.
	void ReleaseEditorWidgetInputMode();
	// Updates the toolbar status text when the bound label exists.
	void SetStatusText(const FString& message);
	// Connects the widget adapter to the subsystem-owned toolbar ViewModel.
	void InitializeViewModel();
	// Resolves the widget that should receive input focus while the toolbar is active.
	UWidget* ResolveInputModeFocusWidget() const;
	// Applies active/inactive visual state to one optional sidebar tab button.
	void ApplySidebarPanelButtonState(UButton* button, EScenarioTemplateSidebarPanel panel) const;
	// Applies active/inactive visual state to one optional transform mode button.
	void ApplyTransformModeButtonState(UButton* button, EScenarioTransformGizmoMode mode) const;
	// Applies active/inactive visual state to one optional coordinate mode button.
	void ApplyCoordinateModeButtonState(UButton* button, EScenarioTransformGizmoOrientationMode orientationMode) const;
	// Applies active/inactive visual state to one optional viewport mode button.
	void ApplyViewModeButtonState(UButton* button, EScenarioEditorViewMode viewMode) const;

	// Subsystem-owned toolbar ViewModel used by MVVM binding and command forwarding.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UScenarioEditorToolbarViewModel> ToolbarViewModel;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToolbarInputModeFocus;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusTextBlock;

	// Optional tab button for the root Scenario Template fields.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainPanelButton;

	// Optional tab button for Corridor fields.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CorridorPanelButton;

	// Optional tab button for static obstacle fields.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ObstaclePanelButton;

	// Optional tab button for pedestrian fields.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PedestrianPanelButton;

	// Optional toolbar button for the move transform tool.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MoveButton;

	// Optional toolbar button for the rotate transform tool.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RotateButton;

	// Optional toolbar button for world-space transform coordinates.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> WorldCoordinateButton;

	// Optional toolbar button for local-space transform coordinates.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LocalCoordinateButton;

	// Compatibility bind name for a moved world-orientation button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> WorldOrientationButton;

	// Compatibility bind name for a moved relative/local-orientation button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RelativeOrientationButton;

	// Optional toolbar button for top-down orthographic view.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> TopDownOrthoViewButton;

	// Optional toolbar button for perspective view.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PerspectiveViewButton;

	// Compatibility bind name for the previous root top-down view button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> TopDownOrthoModeButton;

	// Compatibility bind name for the previous root perspective view button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PerspectiveModeButton;

	// Compatibility bind name for a compact 2D view button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> View2DButton;

	// Compatibility bind name for a compact 3D view button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> View3DButton;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
