#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarObstaclePanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarObstaclePlacementWidget;
class UWidgetTextStyleCatalog;
class SWidget;

// Obstacle Scenario Template sidebar panel for minimum clearance and placement rules.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarObstaclePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native Obstacle tree when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Binds Obstacle field rows after UMG construction.
	virtual void NativeConstruct() override;

	// Releases Obstacle field row bindings before teardown.
	virtual void NativeDestruct() override;

	// Shared typography catalog passed down to blocks and field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

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

	// Pulls the current draft Scenario Template and refreshes this panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Refreshes this panel from the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate);

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

	// Handles placement_id edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementIdCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles prop edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementPropCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles at.segment edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementSegmentCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles at.along_m edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementAlongCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles at.offset_m edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementOffsetCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles allow_blocking edits committed by dynamic placement widgets.
	UFUNCTION()
	void HandlePlacementAllowBlockingCommitted(int32 placementIndex, const FText& text, ETextCommit::Type commitMethod);

	// Dynamic placement widgets owned by root.obstacles.placements[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarObstaclePlacementWidget>> PlacementWidgets;

	// Dynamic count row for root.obstacles.placements[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> PlacementsCountFieldRow;

	// Builds the native fallback panel tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Binds child field row delegates owned by this panel.
	void BindFieldRows();
	// Releases child field row delegates owned by this panel.
	void UnbindFieldRows();
	// Applies static labels and editability to field rows and blocks.
	void ConfigureFieldRows();
	// Applies shared typography to diagnostic text and child rows.
	void ApplyTextStyles();
	// Rebuilds editable placement widgets for obstacle placement rules.
	void RefreshPlacementRows(const TArray<FScenarioTemplateObstaclePlacement>& placements);
	// Adds a read-only field row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddReadOnlyFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		const FString& label,
		const FString& value,
		EScenarioEditorSidebarFieldInputType inputType) const;
	// Adds an editable placement widget to the placements block.
	UScenarioEditorSidebarObstaclePlacementWidget* AddPlacementWidget(
		int32 placementIndex,
		const FScenarioTemplateObstaclePlacement& placement,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Returns the current draft placement list by value for mutation.
	TArray<FScenarioTemplateObstaclePlacement> GetDraftPlacements() const;
	// Commits a fixed min_clear_width_m edit to the draft template.
	void CommitMinClearWidthText(const FText& text);
	// Commits a min/max min_clear_width_m edit to the draft template.
	void CommitMinClearWidthRangeText(const FText& minText, const FText& maxText);
	// Commits a validated min_clear_width_m value through the authoring subsystem.
	void CommitMinClearWidthValue(const FScenarioTemplateNumberValue& widthMeters);
	// Commits one placement_id edit to the draft template.
	void CommitPlacementIdText(int32 placementIndex, const FText& text);
	// Commits one prop edit to the draft template.
	void CommitPlacementPropText(int32 placementIndex, const FText& text);
	// Commits one at.segment edit to the draft template.
	void CommitPlacementSegmentText(int32 placementIndex, const FText& text);
	// Commits one fixed at.along_m edit to the draft template.
	void CommitPlacementAlongText(int32 placementIndex, const FText& text);
	// Commits one fixed at.offset_m edit to the draft template.
	void CommitPlacementOffsetText(int32 placementIndex, const FText& text);
	// Commits one allow_blocking edit to the draft template.
	void CommitPlacementAllowBlockingText(int32 placementIndex, const FText& text);
	// Commits a full placement list through the authoring subsystem.
	void CommitPlacements(const TArray<FScenarioTemplateObstaclePlacement>& placements);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
	// Parses one meter value from field row text.
	static bool TryParseMeters(const FText& text, double& outMeters);
	// Parses one boolean value from field row text.
	static bool TryParseBool(const FText& text, bool& outValue);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(const FScenarioTemplateNumberValue& value);
};
