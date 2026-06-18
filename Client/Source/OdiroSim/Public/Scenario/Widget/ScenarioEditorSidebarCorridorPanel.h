#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarCorridorPanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarCorridorLaneWidget;
class UScenarioEditorSidebarCorridorPointWidget;
class UScenarioEditorSidebarCorridorSegmentWidget;
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

	// Optional axis points property block under root.corridor.axis.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> AxisPointsBlockWidget;

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

	// Optional editable count row for root.corridor.axis.points_m[].
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
	void RefreshFromTemplate(const FScenarioDocument& scenarioTemplate);

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

	// Handles axis point x edits committed by dynamic point widgets.
	UFUNCTION()
	void HandleAxisPointXCommitted(int32 pointIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles axis point y edits committed by dynamic point widgets.
	UFUNCTION()
	void HandleAxisPointYCommitted(int32 pointIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles add requests from dynamic point widgets and count row.
	UFUNCTION()
	void HandleAxisPointAddRequested(int32 pointIndex);

	// Handles remove requests from dynamic point widgets and count row.
	UFUNCTION()
	void HandleAxisPointRemoveRequested(int32 pointIndex);

	// Handles add requests from the axis points count row.
	UFUNCTION()
	void HandleAxisPointsCountAddRequested();

	// Handles remove requests from the axis points count row.
	UFUNCTION()
	void HandleAxisPointsCountRemoveRequested();

	// Handles segment id edits committed by dynamic segment widgets.
	UFUNCTION()
	void HandleSegmentIdCommitted(int32 segmentIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles segment type edits committed by dynamic segment widgets.
	UFUNCTION()
	void HandleSegmentTypeCommitted(int32 segmentIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles segment along-range edits committed by dynamic segment widgets.
	UFUNCTION()
	void HandleSegmentAlongRangeCommitted(
		int32 segmentIndex,
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles segment replaced_by edits committed by dynamic segment widgets.
	UFUNCTION()
	void HandleSegmentReplacedByCommitted(int32 segmentIndex, const FText& text, ETextCommit::Type commitMethod);

	// Handles add requests from dynamic segment widgets and count rows.
	UFUNCTION()
	void HandleSegmentAddRequested(int32 segmentIndex);

	// Handles remove requests from dynamic segment widgets and count rows.
	UFUNCTION()
	void HandleSegmentRemoveRequested(int32 segmentIndex);

	// Handles add requests from the segments count row.
	UFUNCTION()
	void HandleSegmentsCountAddRequested();

	// Handles remove requests from the segments count row.
	UFUNCTION()
	void HandleSegmentsCountRemoveRequested();

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

	// Dynamic point widgets owned by root.corridor.axis.points_m[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarCorridorPointWidget>> AxisPointWidgets;

	// Dynamic count row for root.corridor.segments[].
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> SegmentsCountFieldRow;

	// Dynamic segment widgets owned by root.corridor.segments[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarCorridorSegmentWidget>> SegmentWidgets;

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
	// Rebuilds editable point widgets for the Corridor axis polyline.
	void RefreshAxisPointRows(const TArray<FVector2D>& pointsMeters);
	// Rebuilds editable segment widgets for semantic Corridor segments.
	void RefreshSegmentRows(const TArray<FScenarioTemplateSegment>& segments);
	// Adds a read-only field row to a dynamic block body.
	UScenarioEditorSidebarFieldRow* AddReadOnlyFieldRow(
		UScenarioEditorSidebarBlockWidget* parentBlockWidget,
		const FString& label,
		const FString& value,
		EScenarioEditorSidebarFieldInputType inputType) const;
	// Adds an editable lane widget to a side profile block.
	UScenarioEditorSidebarCorridorLaneWidget* AddLaneWidget(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FScenarioTemplateLaneRule& lane,
		const TArray<FString>& surfaceOptions,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Adds an editable axis point widget to the axis points block.
	UScenarioEditorSidebarCorridorPointWidget* AddAxisPointWidget(
		int32 pointIndex,
		const FVector2D& pointMeters,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Adds an editable segment widget to the segment list block.
	UScenarioEditorSidebarCorridorSegmentWidget* AddSegmentWidget(
		int32 segmentIndex,
		const FScenarioTemplateSegment& segment,
		const TArray<FString>& surfaceOptions,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Returns catalog-backed Corridor surface ids for combo-box fields.
	TArray<FString> GetCorridorSurfaceIdOptions() const;
	// Returns the current draft side lane profile by value for mutation.
	TArray<FScenarioTemplateLaneRule> GetDraftLaneProfile(EScenarioEditorCorridorSide side) const;
	// Returns the current draft axis points by value for mutation.
	TArray<FVector2D> GetDraftAxisPoints() const;
	// Returns the current draft segment list by value for mutation.
	TArray<FScenarioTemplateSegment> GetDraftSegments() const;
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
	// Commits one axis point x edit to the draft template.
	void CommitAxisPointXText(int32 pointIndex, const FText& text);
	// Commits one axis point y edit to the draft template.
	void CommitAxisPointYText(int32 pointIndex, const FText& text);
	// Commits a full axis point list through the authoring subsystem.
	void CommitAxisPoints(const TArray<FVector2D>& pointsMeters);
	// Adds an axis point after the provided point index.
	void AddAxisPointAfter(int32 pointIndex);
	// Removes the axis point at the provided point index.
	void RemoveAxisPointAt(int32 pointIndex);
	// Creates a default axis point near another point.
	static FVector2D MakeDefaultAxisPoint(
		const TArray<FVector2D>& existingPoints,
		int32 neighborIndex);
	// Commits one segment id edit to the draft template.
	void CommitSegmentIdText(int32 segmentIndex, const FText& text);
	// Commits one segment type edit to the draft template.
	void CommitSegmentTypeText(int32 segmentIndex, const FText& text);
	// Commits one segment along-range edit to the draft template.
	void CommitSegmentAlongRangeText(int32 segmentIndex, const FText& minText, const FText& maxText);
	// Commits one segment replaced_by edit to the draft template.
	void CommitSegmentReplacedByText(int32 segmentIndex, const FText& text);
	// Commits a full segment list through the authoring subsystem.
	void CommitSegments(const TArray<FScenarioTemplateSegment>& segments);
	// Adds a segment after the provided segment index.
	void AddSegmentAfter(int32 segmentIndex);
	// Removes the segment at the provided segment index.
	void RemoveSegmentAt(int32 segmentIndex);
	// Creates a valid default segment rule near another segment.
	FScenarioTemplateSegment MakeDefaultSegment(
		const TArray<FScenarioTemplateSegment>& existingSegments,
		int32 neighborIndex) const;
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
	// Parses one meter value from field row text.
	static bool TryParseMeters(const FText& text, double& outMeters);
	// Parses one corridor segment type from field row text.
	static bool TryParseSegmentType(const FText& text, EScenarioTemplateSegmentType& outType);
	// Returns a stable label for a corridor axis type.
	static FString AxisTypeToString(EScenarioCorridorAxisType type);
	// Returns a stable label for a corridor segment type.
	static FString SegmentTypeToString(EScenarioTemplateSegmentType type);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
	// Measures the authored corridor polyline in meters.
	static double MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters);
};
