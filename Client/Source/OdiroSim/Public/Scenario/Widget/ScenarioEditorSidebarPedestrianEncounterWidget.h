#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarPedestrianEncounterWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UWidgetTextStyleCatalog;

// Editable field ids exposed by one root.pedestrians.encounters[] widget.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarPedestrianEncounterField : uint8
{
	EncounterId,
	Type,
	AtSegment,
	Persona,
	MeetOffset,
	Cooperation,
	Evasiveness,
	PersonalSpace,
	AwarenessHorizon,
	MaxYieldWait,
	SidestepDistance
};

// Broadcasts a committed text edit for one pedestrian encounter field.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FScenarioEditorSidebarPedestrianEncounterTextCommitted,
	int32,
	EncounterIndex,
	EScenarioEditorSidebarPedestrianEncounterField,
	Field,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a committed min/max edit for one pedestrian encounter numeric field.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FScenarioEditorSidebarPedestrianEncounterRangeCommitted,
	int32,
	EncounterIndex,
	EScenarioEditorSidebarPedestrianEncounterField,
	Field,
	const FText&,
	MinText,
	const FText&,
	MaxText,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a structural edit request for one encounter index.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarPedestrianEncounterActionRequested,
	int32,
	EncounterIndex);

// Detail block for one root.pedestrians.encounters[] entry.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarPedestrianEncounterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds encounter field row delegates after UMG construction.
	virtual void NativeConstruct() override;

	// Releases encounter field row delegates before teardown.
	virtual void NativeDestruct() override;

	// Index of this encounter inside root.pedestrians.encounters[].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 EncounterIndex = INDEX_NONE;

	// Shared typography catalog passed down to this encounter block and rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional block wrapping this encounter detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> EncounterBlockWidget;

	// Optional editable row for root.pedestrians.encounters[].id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> EncounterIdFieldRow;

	// Optional editable row for root.pedestrians.encounters[].type.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> TypeFieldRow;

	// Optional editable row for root.pedestrians.encounters[].at.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AtSegmentFieldRow;

	// Optional editable row for root.pedestrians.encounters[].persona.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PersonaFieldRow;

	// Optional editable row for root.pedestrians.encounters[].meet_offset_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> MeetOffsetFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.cooperation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> CooperationFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.evasiveness.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> EvasivenessFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.personal_space_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PersonalSpaceFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.awareness_horizon_s.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AwarenessHorizonFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.max_yield_wait_s.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> MaxYieldWaitFieldRow;

	// Optional editable row for root.pedestrians.encounters[].overrides.sidestep_distance_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SidestepDistanceFieldRow;

	// Emits committed text for string or fixed numeric encounter fields.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarPedestrianEncounterTextCommitted OnFieldTextCommitted;

	// Emits committed range text for numeric encounter fields.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarPedestrianEncounterRangeCommitted OnFieldRangeCommitted;

	// Emits an add request using this encounter index as insertion context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarPedestrianEncounterActionRequested OnAddRequested;

	// Emits a remove request for this encounter index.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarPedestrianEncounterActionRequested OnRemoveRequested;

	// Updates index context and refreshes the encounter block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetEncounterIndex(int32 inEncounterIndex);

	// Updates the shared typography catalog used by this encounter widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Refreshes this encounter widget from one template encounter entry.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromEncounter(const FScenarioTemplatePedestrianEncounter& encounter);

private:
	// Handles id row commits.
	UFUNCTION()
	void HandleEncounterIdCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles type row commits.
	UFUNCTION()
	void HandleTypeCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles at row commits.
	UFUNCTION()
	void HandleAtSegmentCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles persona row commits.
	UFUNCTION()
	void HandlePersonaCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles meet offset fixed-value commits.
	UFUNCTION()
	void HandleMeetOffsetCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles meet offset range commits.
	UFUNCTION()
	void HandleMeetOffsetRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles cooperation fixed-value commits.
	UFUNCTION()
	void HandleCooperationCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles cooperation range commits.
	UFUNCTION()
	void HandleCooperationRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles evasiveness fixed-value commits.
	UFUNCTION()
	void HandleEvasivenessCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles evasiveness range commits.
	UFUNCTION()
	void HandleEvasivenessRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles personal space fixed-value commits.
	UFUNCTION()
	void HandlePersonalSpaceCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles personal space range commits.
	UFUNCTION()
	void HandlePersonalSpaceRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles awareness horizon fixed-value commits.
	UFUNCTION()
	void HandleAwarenessHorizonCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles awareness horizon range commits.
	UFUNCTION()
	void HandleAwarenessHorizonRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles max yield wait fixed-value commits.
	UFUNCTION()
	void HandleMaxYieldWaitCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles max yield wait range commits.
	UFUNCTION()
	void HandleMaxYieldWaitRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles sidestep distance fixed-value commits.
	UFUNCTION()
	void HandleSidestepDistanceCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles sidestep distance range commits.
	UFUNCTION()
	void HandleSidestepDistanceRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles encounter add button requests.
	UFUNCTION()
	void HandleAddRequested();
	// Handles encounter remove button requests.
	UFUNCTION()
	void HandleRemoveRequested();

	// Last encounter used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FScenarioTemplatePedestrianEncounter CachedEncounter;

	// True when CachedEncounter contains valid data from RefreshFromEncounter.
	UPROPERTY(Transient)
	bool bHasCachedEncounter = false;

	// Binds child field row delegates owned by this encounter widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this encounter widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies cached encounter values to bound field rows.
	void ApplyCachedEncounterToRows();
	// Applies shared typography to this encounter block and rows.
	void ApplyTextStyles();
	// Broadcasts a fixed-value text commit for one field.
	void BroadcastText(EScenarioEditorSidebarPedestrianEncounterField field, const FText& text, ETextCommit::Type commitMethod);
	// Broadcasts a range commit for one field.
	void BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField field, const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Applies one authored number value to a numeric field row.
	static void SetNumberRowValue(UScenarioEditorSidebarFieldRow* fieldRow, const FScenarioTemplateNumberValue& value);
	// Returns a stable label for a pedestrian encounter type.
	static FString EncounterTypeToString(EScenarioTemplateEncounterType type);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
};
