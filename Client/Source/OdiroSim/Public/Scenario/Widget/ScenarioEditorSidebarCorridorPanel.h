#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarCorridorPanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarCorridorLaneWidget;
class UWidgetTextStyleCatalog;
class SWidget;

// Corridor Scenario Template sidebar panel for axis, width, side lanes, and segments.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarCorridorPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native Corridor tree when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Binds Corridor field rows after UMG construction.
	virtual void NativeConstruct() override;

	// Releases Corridor field row bindings before teardown.
	virtual void NativeDestruct() override;

	// Shared typography catalog passed down to blocks and field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional root Corridor template block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> CorridorBlockWidget;

	// Optional axis property block under root.corridor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> AxisBlockWidget;

	// Optional walkway width property block under root.corridor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> WalkwayWidthBlockWidget;

	// Optional building-side lane profile block under root.corridor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> BuildingSideBlockWidget;

	// Optional curb-side lane profile block under root.corridor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> CurbSideBlockWidget;

	// Optional semantic segments block under root.corridor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> SegmentsBlockWidget;

	// Optional read-only row for root.corridor.axis.type.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AxisTypeFieldRow;

	// Optional read-only row for root.corridor.axis.points_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AxisPointsFieldRow;

	// Optional editable row for root.corridor.walkway_width_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> WalkwayWidthFieldRow;

	// Optional diagnostics text for rejected Corridor edits.
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
	// Handles fixed walkway width edits committed by the field row.
	UFUNCTION()
	void HandleWalkwayWidthCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles min/max walkway width edits committed by the field row.
	UFUNCTION()
	void HandleWalkwayWidthRangeCommitted(
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles lane surface edits committed by dynamic lane widgets.
	UFUNCTION()
	void HandleLaneSurfaceCommitted(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& text,
		ETextCommit::Type commitMethod);

	// Handles fixed lane width edits committed by dynamic lane widgets.
	UFUNCTION()
	void HandleLaneWidthCommitted(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& text,
		ETextCommit::Type commitMethod);

	// Handles min/max lane width edits committed by dynamic lane widgets.
	UFUNCTION()
	void HandleLaneWidthRangeCommitted(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles add requests from dynamic lane widgets and count rows.
	UFUNCTION()
	void HandleLaneAddRequested(EScenarioEditorCorridorSide side, int32 laneIndex);

	// Handles remove requests from dynamic lane widgets and count rows.
	UFUNCTION()
	void HandleLaneRemoveRequested(EScenarioEditorCorridorSide side, int32 laneIndex);

	// Handles add requests from the building_side count row.
	UFUNCTION()
	void HandleBuildingSideCountAddRequested();

	// Handles remove requests from the building_side count row.
	UFUNCTION()
	void HandleBuildingSideCountRemoveRequested();

	// Handles add requests from the curb_side count row.
	UFUNCTION()
	void HandleCurbSideCountAddRequested();

	// Handles remove requests from the curb_side count row.
	UFUNCTION()
	void HandleCurbSideCountRemoveRequested();

	// Dynamic count row for root.corridor.building_side[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> BuildingSideCountFieldRow;

	// Dynamic count row for root.corridor.curb_side[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> CurbSideCountFieldRow;

	// Dynamic lane widgets owned by root.corridor.building_side[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarCorridorLaneWidget>> BuildingSideLaneWidgets;

	// Dynamic lane widgets owned by root.corridor.curb_side[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarCorridorLaneWidget>> CurbSideLaneWidgets;

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
	// Rebuilds editable lane widgets for one Corridor side lane profile.
	void RefreshLaneProfileRows(
		EScenarioEditorCorridorSide side,
		UScenarioEditorSidebarBlockWidget* sideBlockWidget,
		const TArray<FScenarioTemplateLaneRule>& lanes);
	// Rebuilds read-only rows for semantic Corridor segments.
	void RefreshSegmentRows(const TArray<FScenarioTemplateSegment>& segments) const;
	// Adds a read-only field row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddReadOnlyFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		const FString& label,
		const FString& value,
		EScenarioEditorSidebarFieldInputType inputType) const;
	// Adds a nested segment block to the semantic segment list.
	UScenarioEditorSidebarBlockWidget* AddSegmentBlock(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		const FScenarioTemplateSegment& segment) const;
	// Adds an editable lane widget to a side profile block.
	UScenarioEditorSidebarCorridorLaneWidget* AddLaneWidget(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FScenarioTemplateLaneRule& lane,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Returns the current draft side lane profile by value for mutation.
	TArray<FScenarioTemplateLaneRule> GetDraftLaneProfile(EScenarioEditorCorridorSide side) const;
	// Commits a fixed walkway width edit to the draft template.
	void CommitWalkwayWidthText(const FText& text);
	// Commits a min/max walkway width edit to the draft template.
	void CommitWalkwayWidthRangeText(const FText& minText, const FText& maxText);
	// Commits a validated walkway width value through the authoring subsystem.
	void CommitWalkwayWidthValue(const FScenarioTemplateNumberValue& widthMeters);
	// Commits one lane surface id edit to the draft template.
	void CommitLaneSurfaceText(EScenarioEditorCorridorSide side, int32 laneIndex, const FText& text);
	// Commits one fixed lane width edit to the draft template.
	void CommitLaneWidthText(EScenarioEditorCorridorSide side, int32 laneIndex, const FText& text);
	// Commits one min/max lane width edit to the draft template.
	void CommitLaneWidthRangeText(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& minText,
		const FText& maxText);
	// Commits a full side lane profile through the authoring subsystem.
	void CommitLaneProfile(EScenarioEditorCorridorSide side, const TArray<FScenarioTemplateLaneRule>& lanes);
	// Adds a lane after the provided lane index.
	void AddLaneAfter(EScenarioEditorCorridorSide side, int32 laneIndex);
	// Removes the lane at the provided lane index.
	void RemoveLaneAt(EScenarioEditorCorridorSide side, int32 laneIndex);
	// Creates a valid default lane rule for one Corridor side.
	static FScenarioTemplateLaneRule MakeDefaultLaneRule(
		EScenarioEditorCorridorSide side,
		const TArray<FScenarioTemplateLaneRule>& existingLanes,
		int32 neighborIndex);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
	// Parses one meter value from field row text.
	static bool TryParseMeters(const FText& text, double& outMeters);
	// Returns a stable label for a corridor axis type.
	static FString AxisTypeToString(EScenarioCorridorAxisType type);
	// Returns a stable label for a corridor segment type.
	static FString SegmentTypeToString(EScenarioTemplateSegmentType type);
	// Formats one authored numeric value for compact display.
	static FString FormatNumberValue(const FScenarioTemplateNumberValue& value, const FString& suffix = FString());
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
	// Formats one authored string value for compact display.
	static FString FormatStringValue(const FScenarioTemplateStringValue& value);
	// Formats a comma-separated string list.
	static FString FormatStringList(const TArray<FString>& values);
	// Formats one Corridor side lane rule.
	static FString FormatLaneRule(const FScenarioTemplateLaneRule& lane);
	// Formats a compact summary of Corridor axis points.
	static FString FormatAxisPointsSummary(const TArray<FVector2D>& pointsMeters);
	// Measures the authored corridor polyline in meters.
	static double MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters);
};
