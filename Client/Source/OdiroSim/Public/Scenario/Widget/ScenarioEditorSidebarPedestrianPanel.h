#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianEncounterWidget.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarPedestrianPanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarBlockWidget;
class UWidgetTextStyleCatalog;
class SWidget;

// Pedestrian Scenario Template sidebar panel that exposes the editable tree shape without committing edits yet.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarPedestrianPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native Pedestrian tree when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Configures Pedestrian rows after UMG construction.
	virtual void NativeConstruct() override;

	// Shared typography catalog passed down to blocks and field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

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

	// Pulls the current draft Scenario Template and refreshes this panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Refreshes this panel from the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate);

private:
	// Dynamic count row for root.pedestrians.encounters[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> EncountersCountFieldRow;

	// Dynamic encounter widgets owned by root.pedestrians.encounters[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarPedestrianEncounterWidget>> EncounterWidgets;

	// Builds the native fallback panel tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies shared typography to diagnostic text and child rows.
	void ApplyTextStyles();
	// Rebuilds structure-only encounter widgets for pedestrian encounter rules.
	void RefreshEncounterRows(const TArray<FScenarioTemplatePedestrianEncounter>& encounters);
	// Adds one structure row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		const FString& label,
		const FString& value,
		EScenarioEditorSidebarFieldInputType inputType,
		bool bEditable,
		bool bArrayControlsEnabled = false) const;
	// Adds an editable encounter widget to the encounters block without committing field changes.
	UScenarioEditorSidebarPedestrianEncounterWidget* AddEncounterWidget(
		int32 encounterIndex,
		const FScenarioTemplatePedestrianEncounter& encounter,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Applies one authored number value to a field row.
	static void SetNumberRowValue(UScenarioEditorSidebarFieldRow* fieldRow, const FScenarioTemplateNumberValue& value);
	// Applies one authored integer value to a field row.
	static void SetIntegerRowValue(UScenarioEditorSidebarFieldRow* fieldRow, const FScenarioTemplateIntegerValue& value);
	// Joins a string list for one editable field row.
	static FString JoinStringList(const TArray<FString>& values);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
	// Formats one authored integer value for editable text controls.
	static FString FormatEditableInteger(int32 value);
};
