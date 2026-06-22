#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarMainPanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarFieldRow;
class UScenarioEditorWidgetClassCatalog;
class UWidgetTextStyleCatalog;

// Robot anchor target edited by the Main sidebar panel.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarRobotAnchorTarget : uint8
{
	Start,
	Goal
};

// Robot anchor field edited by the Main sidebar panel.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarRobotAnchorField : uint8
{
	Type,
	Segment,
	Along,
	Offset,
	Lane,
	Heading
};

// Main project scenario sidebar panel for scenario metadata and robot anchors.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarMainPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds Main panel rows after UMG construction.
	virtual void NativeConstruct() override;
	// Releases Main panel row bindings before teardown.
	virtual void NativeDestruct() override;

	// Optional editable row for scenario.scenario_id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> ScenarioIdFieldRow;

	// Optional read-only row for scenario.version.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> VersionFieldRow;

	// Optional editable row for scenario.intent.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> IntentFieldRow;

	// Optional read-only row summarizing the robot start anchor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartFieldRow;

	// Optional read-only row summarizing the robot goal anchor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalFieldRow;

	// Optional diagnostics text for rejected metadata edits.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Shared typography catalog passed down to field rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// WBP class catalog passed down to dynamic child widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	// Optional root project scenario block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> RootBlockWidget;

	// Optional robot template block nested under the root block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> RobotBlockWidget;

	// Optional required robot start block nested under the robot block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> RobotStartBlockWidget;

	// Optional required robot goal block nested under the robot block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> RobotGoalBlockWidget;

	// Optional read-only row for scenario.schema.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SchemaFieldRow;

	// Updates the shared typography catalog used by this panel and its field rows.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates the WBP class catalog used by this panel and child widgets.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetWidgetClassCatalog(TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog);

	// Pulls the current draft project scenario and refreshes this panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Refreshes this panel from the provided draft scenario document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioDocument& scenarioTemplate);

private:
	// Handles scenario_id edits committed by the field row.
	UFUNCTION()
	void HandleScenarioIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles intent edits committed by the field row.
	UFUNCTION()
	void HandleIntentCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles robot.start.type edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartTypeCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.start.segment edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartSegmentCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.start.along_m fixed-value edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartAlongCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.start.along_m range edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartAlongRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles robot.start.offset_m fixed-value edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartOffsetCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.start.offset_m range edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartOffsetRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles robot.start.lane edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartLaneCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.start.heading edits committed by the field row.
	UFUNCTION()
	void HandleRobotStartHeadingCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.type edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalTypeCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.segment edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalSegmentCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.along_m fixed-value edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalAlongCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.along_m range edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalAlongRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles robot.goal.offset_m fixed-value edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalOffsetCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.offset_m range edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalOffsetRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles robot.goal.lane edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalLaneCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles robot.goal.heading edits committed by the field row.
	UFUNCTION()
	void HandleRobotGoalHeadingCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Optional editable row for robot.start.type.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartTypeFieldRow;

	// Optional editable row for robot.start.segment.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartSegmentFieldRow;

	// Optional editable row for robot.start.along_m.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartAlongFieldRow;

	// Optional editable row for robot.start.offset_m.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartOffsetFieldRow;

	// Optional editable row for robot.start.lane.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartLaneFieldRow;

	// Optional editable row for robot.start.heading.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartHeadingFieldRow;

	// Optional editable row for robot.goal.type.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalTypeFieldRow;

	// Optional editable row for robot.goal.segment.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalSegmentFieldRow;

	// Optional editable row for robot.goal.along_m.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalAlongFieldRow;

	// Optional editable row for robot.goal.offset_m.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalOffsetFieldRow;

	// Optional editable row for robot.goal.lane.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalLaneFieldRow;

	// Optional editable row for robot.goal.heading.
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalHeadingFieldRow;

	// Binds child field row delegates owned by this panel.
	void BindFieldRows();
	// Releases child field row delegates owned by this panel.
	void UnbindFieldRows();
	// Applies static labels and editability to field rows.
	void ConfigureFieldRows();
	// Applies shared typography to diagnostic text and child rows.
	void ApplyTextStyles();
	// Applies static labels and editability to one robot anchor detail row group.
	void ConfigureRobotAnchorRows(
		UScenarioEditorSidebarFieldRow* typeRow,
		UScenarioEditorSidebarFieldRow* segmentRow,
		UScenarioEditorSidebarFieldRow* alongRow,
		UScenarioEditorSidebarFieldRow* offsetRow,
		UScenarioEditorSidebarFieldRow* laneRow,
		UScenarioEditorSidebarFieldRow* headingRow);
	// Refreshes one robot anchor detail row group from template data.
	void RefreshRobotAnchorRows(
		const FScenarioTemplateRobotAnchor& anchor,
		UScenarioEditorSidebarFieldRow* typeRow,
		UScenarioEditorSidebarFieldRow* segmentRow,
		UScenarioEditorSidebarFieldRow* alongRow,
		UScenarioEditorSidebarFieldRow* offsetRow,
		UScenarioEditorSidebarFieldRow* laneRow,
		UScenarioEditorSidebarFieldRow* headingRow) const;
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Commits a scenario_id edit to the draft scenario.
	void CommitScenarioIdText(const FText& text);
	// Commits an intent edit to the draft template.
	void CommitIntentText(const FText& text);
	// Commits one robot anchor text field edit to the draft template.
	void CommitRobotAnchorText(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& text);
	// Commits one robot anchor range field edit to the draft template.
	void CommitRobotAnchorRange(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& minText,
		const FText& maxText);
	// Commits one full robot anchor object through the authoring subsystem.
	void CommitRobotAnchorValue(
		EScenarioEditorSidebarRobotAnchorTarget target,
		const FScenarioTemplateRobotAnchor& anchor);
	// Handles robot anchor text commit boilerplate shared by start and goal rows.
	void HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& text,
		ETextCommit::Type commitMethod);
	// Handles robot anchor range commit boilerplate shared by start and goal rows.
	void HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
	// Applies one authored number value to a field row.
	static void SetNumberRowValue(UScenarioEditorSidebarFieldRow* fieldRow, const FScenarioTemplateNumberValue& value);
	// Parses one optional number value from field row text.
	static bool TryParseOptionalNumber(const FText& text, FScenarioTemplateNumberValue& outValue);
	// Parses one optional number range from field row text.
	static bool TryParseOptionalNumberRange(
		const FText& minText,
		const FText& maxText,
		FScenarioTemplateNumberValue& outValue);
	// Parses one robot anchor type from editor text.
	static bool TryParseRobotAnchorType(const FText& text, EScenarioTemplateRobotAnchorType& outType);
	// Parses one robot heading hint from editor text.
	static bool TryParseRobotHeading(const FText& text, EScenarioTemplateRobotHeading& outHeading);
	// Returns a stable label for a robot anchor type.
	static FString RobotAnchorTypeToString(EScenarioTemplateRobotAnchorType type);
	// Returns a stable label for a robot heading hint.
	static FString RobotHeadingToString(EScenarioTemplateRobotHeading heading);
	// Formats one authored numeric value for compact display.
	static FString FormatNumberValue(const FScenarioTemplateNumberValue& value, const FString& suffix = FString());
	// Formats a robot anchor in template-space terms.
	static FString FormatRobotAnchor(const FScenarioTemplateRobotAnchor& anchor);
};
