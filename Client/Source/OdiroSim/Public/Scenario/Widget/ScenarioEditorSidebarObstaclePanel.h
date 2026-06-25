#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarObstaclePanel.generated.h"

class UTextBlock;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorWidgetClassCatalog;
class UScenarioTemplateFieldRowViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidgetTextStyleCatalog;

// Obstacle Scenario Template sidebar panel for minimum clearance and placement rules.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarObstaclePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds Obstacle field rows after UMG construction.
	virtual void NativeConstruct() override;

	// Releases Obstacle field row bindings before teardown.
	virtual void NativeDestruct() override;

	// Shared typography catalog passed down to blocks and field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// WBP class catalog passed down to dynamic child widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	// Optional root obstacle template block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> ObstacleBlockWidget;

	// Optional min_clear_width_m property block under root.obstacles.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> MinClearWidthBlockWidget;

	// Optional placements property block under root.obstacles.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> PlacementsBlockWidget;

	// Optional editable row for root.obstacles.min_clear_width_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> MinClearWidthFieldRow;

	// Optional diagnostics text for rejected Obstacle edits.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Updates the shared typography catalog used by this panel and its children.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates the WBP class catalog used by this panel and child widgets.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetWidgetClassCatalog(TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog);

	// Pulls the current draft Scenario Template and refreshes this panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Refreshes this panel from the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Applies the shell-selected block path to this panel and repeated placement blocks.
	void ApplySelectedBlockPath();

	// Appends every block widget currently owned by this panel.
	void CollectBlockWidgets(TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const;

	// Returns the block widget that owns the requested stable block path.
	UScenarioEditorSidebarBlockWidget* FindBlockWidgetByPath(const FString& blockPath) const;

private:
	// Handles fixed min_clear_width_m edits committed by the field row.
	UFUNCTION()
	void HandleMinClearWidthCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles min/max min_clear_width_m edits committed by the field row.
	UFUNCTION()
	void HandleMinClearWidthRangeCommitted(
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles add requests from the placements count row.
	UFUNCTION()
	void HandlePlacementsCountAddRequested();

	// Handles remove requests from the placements count row.
	UFUNCTION()
	void HandlePlacementsCountRemoveRequested();

	// Handles text commits from dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementFieldTextCommitted(
		int32 placementIndex,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FText& text,
		ETextCommit::Type commitMethod);

	// Handles range commits from dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementFieldRangeCommitted(
		int32 placementIndex,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles add requests from dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementAddRequested(int32 placementIndex);

	// Handles remove requests from dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementRemoveRequested(int32 placementIndex);

	// Dynamic placement widgets owned by root.obstacles.placements[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarObstaclePlacementWidget>> PlacementWidgets;

	// Dynamic count row for root.obstacles.placements[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> PlacementsCountFieldRow;

	// Binds child field row delegates owned by this panel.
	void BindFieldRows();
	// Releases child field row delegates owned by this panel.
	void UnbindFieldRows();
	// Applies static labels and editability to field rows and blocks.
	void ConfigureFieldRows();
	// Applies shared typography to diagnostic text and child rows.
	void ApplyTextStyles();
	// Applies current Obstacle field row ViewModels to bound row widgets.
	void ApplyObstacleFieldItems();
	// Rebuilds editable placement widgets for obstacle placement rules.
	void RefreshPlacementRows(const TArray<FScenarioTemplateObstaclePlacement>& placements);
	// Adds one field row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		UScenarioTemplateFieldRowViewModel* fieldItemViewModel) const;
	// Adds an editable placement widget to the placements block.
	UScenarioEditorSidebarObstaclePlacementWidget* AddPlacementWidget(
		int32 placementIndex,
		const FScenarioTemplateObstaclePlacement& placement,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the ViewModel that forwards draft template commands.
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const;
	// Runs a ViewModel command, refreshes the panel, and mirrors command status text.
	void ExecuteTemplateCommand(TFunctionRef<bool(UScenarioTemplateSidebarViewModel*, FString&)> command);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
};
