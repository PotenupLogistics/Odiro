#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarWidget.generated.h"

class UScrollBox;
class UScenarioEditorSidebarBlockWidget;
class UTextBlock;
class UScenarioEditorSidebarMainPanel;
class UScenarioEditorWidgetClassCatalog;
class UWidget;
class UWidgetSwitcher;
class UWidgetTextStyleCatalog;

// Scenario Template panel switch host used by the editor side sidebar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Refreshes the read-only sidebar after UMG construction.
	virtual void NativeConstruct() override;

	// Releases block-selection bindings before teardown.
	virtual void NativeDestruct() override;

	// Scenario Template block currently displayed by the sidebar.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	EScenarioTemplateSidebarPanel ActivePanel = EScenarioTemplateSidebarPanel::Main;

	// Shared typography catalog used by the sidebar and child panel widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// WBP class catalog used for generated editor detail panels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	// Optional title text for the active Scenario Template block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> PanelTitleTextBlock;

	// Optional status text for missing draft or sidebar binding diagnostics.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Optional scroll area that owns the active Scenario Template block panel.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScrollBox> SidebarScrollBox;

	// Optional switcher that hosts Main/Corridor/Obstacle/Pedestrian panel widgets.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidgetSwitcher> PanelSwitcher;

	// Optional specialized Main panel widget for template metadata and robot anchors.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarMainPanel> MainPanelWidget;

	// Optional specialized Corridor panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> CorridorPanelWidget;

	// Optional specialized Obstacle panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> ObstaclePanelWidget;

	// Optional specialized Pedestrian panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> PedestrianPanelWidget;

	// Selects the Scenario Template block displayed by the sidebar.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetActivePanel(EScenarioTemplateSidebarPanel panel);

	// Updates the shared typography catalog used by this sidebar and its children.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates the WBP class catalog used by this sidebar and child panel widgets.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetWidgetClassCatalog(TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog);

	// Pulls the current draft Scenario Template from the authoring subsystem and renders it.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Renders a read-only view of the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Applies the selected block visuals without rebuilding the active panel content.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void ApplySelectedBlockFocus(bool bScrollIntoView);

private:
	// Generated Main panel used when no typed Main panel is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedMainPanelWidget;

	// Generated Corridor panel used when no typed Corridor panel is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedCorridorPanelWidget;

	// Generated Obstacle panel used when no typed Obstacle panel is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedObstaclePanelWidget;

	// Generated Pedestrian panel used when no typed Pedestrian panel is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedPedestrianPanelWidget;

	// Legacy summary container kept only so migrated WBP trees can be hidden by C++.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UWidget> FallbackSummaryContainer;

	// Legacy primary summary text kept only for migration-time collapse.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UTextBlock> PrimaryFieldsTextBlock;

	// Legacy secondary summary text kept only for migration-time collapse.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UTextBlock> SecondaryFieldsTextBlock;

	// Legacy repeated-item summary text kept only for migration-time collapse.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UTextBlock> ListSummaryTextBlock;

	// True while a selected-block scroll request has already been deferred to the next tick.
	bool bSelectedBlockScrollPending = false;

	// Remaining deferred attempts used while panel rows and expanded block layouts settle.
	int32 SelectedBlockScrollAttemptsRemaining = 0;

	// Refreshes the active typed panel widget.
	bool RefreshActivePanelContent(const FScenarioDocument& scenarioTemplate);
	// Creates or returns the typed widget for one sidebar panel.
	UWidget* EnsurePanelWidget(EScenarioTemplateSidebarPanel panel);
	// Creates or returns a catalog-backed generated panel widget for one sidebar panel.
	UWidget* EnsureGeneratedPanelWidget(EScenarioTemplateSidebarPanel panel);
	// Returns a catalog-backed generated panel widget without creating it.
	UWidget* ResolveGeneratedPanelWidget(EScenarioTemplateSidebarPanel panel) const;
	// Applies the active panel to the optional UMG widget switcher.
	void RefreshPanelSwitcher();
	// Resolves the switcher child widget for one sidebar panel.
	UWidget* ResolvePanelWidget(EScenarioTemplateSidebarPanel panel) const;
	// Applies shell-level title and diagnostics text.
	void SetSidebarShellText(
		const FString& title,
		const FString& diagnosticsText);
	// Collapses migrated summary text widgets so typed panels are the only detail surface.
	void CollapseLegacySummaryWidgets() const;
	// Applies text to a bound text block when it exists.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;
	// Registers this sidebar as an editor UI region so viewport selection ignores sidebar clicks.
	void RequestEditorWidgetInputMode();
	// Releases this sidebar from editor UI-region tracking before teardown.
	void ReleaseEditorWidgetInputMode();
	// Pushes non-visual dependencies to typed child panel widgets.
	void ConfigureChildPanelDependencies() const;
	// Returns the display title for one template sidebar panel.
	static FString PanelToTitle(EScenarioTemplateSidebarPanel panel);
	// Binds active panel block events to the shell selection command.
	void BindPanelBlockSelection(UWidget* panelWidget);
	// Releases active panel block events from the shell selection command.
	void UnbindPanelBlockSelection(UWidget* panelWidget);
	// Applies the current ViewModel-selected block state to the active panel.
	void ApplyActivePanelSelectionState();
	// Defers selected block scrolling until refreshed panel layout is available.
	void RequestScrollSelectedBlockIntoView();
	// Defers selected block scrolling with a bounded retry count for dynamic panel rebuilds.
	void RequestScrollSelectedBlockIntoView(int32 attemptsRemaining);
	// Scrolls the current ViewModel-selected block into the sidebar viewport.
	void ScrollSelectedBlockIntoView(int32 attemptsRemaining);
	// Finds a block widget in the active panel by stable block path.
	UScenarioEditorSidebarBlockWidget* FindActivePanelBlockWidgetByPath(const FString& blockPath) const;
	// Appends every selectable block widget owned by one panel.
	void CollectPanelBlockWidgets(UWidget* panelWidget, TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const;
	// Resolves a placeable id represented by a sidebar block path.
	bool TryResolvePlaceableIdForBlockPath(const FString& blockPath, FString& outInstanceId) const;
	// Releases block events from every currently resolved panel widget.
	void UnbindAllPanelBlockSelection();
	// Binds one optional block widget to the shell selection command.
	void BindBlockSelection(UScenarioEditorSidebarBlockWidget* blockWidget);
	// Releases one optional block widget from the shell selection command.
	void UnbindBlockSelection(UScenarioEditorSidebarBlockWidget* blockWidget);
	// Routes a selected sidebar block path into the shared shell ViewModel.
	UFUNCTION()
	void HandlePanelBlockSelected(const FString& blockPath);
};
