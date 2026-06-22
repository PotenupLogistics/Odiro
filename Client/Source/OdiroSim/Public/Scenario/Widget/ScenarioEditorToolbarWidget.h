#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorToolbarWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

// Notifies root widgets when the toolbar-selected template sidebar panel changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioTemplateSidebarPanelChanged,
	EScenarioTemplateSidebarPanel,
	ActivePanel);

// ScenarioEditorMap에서 저장과 Startup menu 복귀를 제공하는 최소 toolbar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString DefaultSavePath = TEXT("Saved/UserProjects/ScenarioEditor/scenario.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString StartupMapId = TEXT("StartupMap");

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

	// Returns the active Scenario Template sidebar panel.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	EScenarioTemplateSidebarPanel GetActiveSidebarPanel() const { return ActiveSidebarPanel; }

	// Applies the active-panel visual state to optional toolbar panel buttons.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void RefreshSidebarPanelButtons();

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

private:
	// Binds optional toolbar buttons exposed by the UMG tree.
	void BindControls();
	// Requests editor widget input mode while toolbar fields may receive focus.
	void RequestEditorWidgetInputMode();
	// Releases toolbar input-mode ownership.
	void ReleaseEditorWidgetInputMode();
	// Updates the toolbar status text when the bound label exists.
	void SetStatusText(const FString& message);
	// Resolves the widget that should receive input focus while the toolbar is active.
	UWidget* ResolveInputModeFocusWidget() const;
	// Resolves the save destination, preserving loaded source paths when available.
	FString ResolveSavePath() const;
	// Applies active/inactive visual state to one optional sidebar tab button.
	void ApplySidebarPanelButtonState(UButton* button, EScenarioTemplateSidebarPanel panel) const;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ToolbarInputModeFocus;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(Transient, meta = (BindWidget))
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

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
