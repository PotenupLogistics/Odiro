#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianEncounterWidget.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarPedestrianPanel.generated.h"

class UTextBlock;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorWidgetClassCatalog;
class UScenarioTemplateFieldRowViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidgetTextStyleCatalog;

// Pedestrian Scenario Template sidebar panel that exposes the editable tree shape without committing edits yet.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarPedestrianPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Configures Pedestrian rows after UMG construction.
	virtual void NativeConstruct() override;

	// Releases Pedestrian block action bindings before teardown.
	virtual void NativeDestruct() override;

	// Shared typography catalog passed down to blocks and field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// WBP class catalog passed down to dynamic child widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	// Optional root pedestrians template block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> PedestriansBlockWidget;

	// Optional background property block under root.pedestrians.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> BackgroundBlockWidget;

	// Optional spawn zone property block under root.pedestrians.background.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> SpawnZoneBlockWidget;

	// Optional encounters property block under root.pedestrians.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> EncountersBlockWidget;

	// Optional editable row for root.pedestrians.background.count.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> BackgroundCountFieldRow;

	// Optional editable row for root.pedestrians.background.speed_mps.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> BackgroundSpeedFieldRow;

	// Optional editable row for root.pedestrians.background.spawn_zone.segments[].
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SpawnSegmentsFieldRow;

	// Optional status text for the current structure-only Pedestrian slice.
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

	// Applies the shell-selected block path to this panel and repeated encounter blocks.
	void ApplySelectedBlockPath();

	// Appends every block widget currently owned by this panel.
	void CollectBlockWidgets(TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const;

	// Returns the block widget that owns the requested stable block path.
	UScenarioEditorSidebarBlockWidget* FindBlockWidgetByPath(const FString& blockPath) const;

private:
	// Handles add requests from the encounters collection block.
	UFUNCTION()
	void HandleEncounterCollectionAddRequested();

	// Handles remove requests from an encounter item block.
	UFUNCTION()
	void HandleEncounterRemoveRequested(int32 encounterIndex);

	// Dynamic count row for root.pedestrians.encounters[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> EncountersCountFieldRow;

	// Dynamic encounter widgets owned by root.pedestrians.encounters[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarPedestrianEncounterWidget>> EncounterWidgets;

	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies shared typography to diagnostic text and child rows.
	void ApplyTextStyles();
	// Applies current Pedestrian field row ViewModels to bound row widgets.
	void ApplyPedestrianFieldItems();
	// Binds child block action delegates owned by this panel.
	void BindControls();
	// Releases child block action delegates owned by this panel.
	void UnbindControls();
	// Rebuilds structure-only encounter widgets for pedestrian encounter rules.
	void RefreshEncounterRows(const TArray<FScenarioTemplatePedestrianEncounter>& encounters);
	// Adds one structure row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		UScenarioTemplateFieldRowViewModel* fieldItemViewModel) const;
	// Adds an editable encounter widget to the encounters block without committing field changes.
	UScenarioEditorSidebarPedestrianEncounterWidget* AddEncounterWidget(
		int32 encounterIndex,
		const FScenarioTemplatePedestrianEncounter& encounter,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the ViewModel that forwards draft template commands.
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const;
	// Runs a ViewModel command, refreshes the panel, and mirrors command status text.
	void ExecuteTemplateCommand(TFunctionRef<bool(UScenarioTemplateSidebarViewModel*, FString&)> command);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
};
