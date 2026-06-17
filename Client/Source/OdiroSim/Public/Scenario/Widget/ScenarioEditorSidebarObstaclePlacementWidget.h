#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarObstaclePlacementWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UWidgetTextStyleCatalog;
class SWidget;

// Broadcasts a committed static obstacle placement text edit with placement index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarObstaclePlacementTextCommitted,
	int32,
	PlacementIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Detail block for one root.obstacles.placements[] entry.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarObstaclePlacementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native placement detail block when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Binds placement field row delegates after UMG construction.
	virtual void NativeConstruct() override;

	// Releases placement field row delegates before teardown.
	virtual void NativeDestruct() override;

	// Index of this placement inside root.obstacles.placements[].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 PlacementIndex = INDEX_NONE;

	// Shared typography catalog passed down to this placement block and rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional block wrapping this placement detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> PlacementBlockWidget;

	// Optional editable row for root.obstacles.placements[].placement_id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PlacementIdFieldRow;

	// Optional read-only row for root.obstacles.placements[].kind.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> KindFieldRow;

	// Optional editable row for root.obstacles.placements[].prop.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PropFieldRow;

	// Optional editable row for root.obstacles.placements[].at.segment.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SegmentFieldRow;

	// Optional editable row for root.obstacles.placements[].at.along_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AlongFieldRow;

	// Optional editable row for root.obstacles.placements[].at.offset_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> OffsetFieldRow;

	// Optional editable row for root.obstacles.placements[].allow_blocking.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AllowBlockingFieldRow;

	// Emits committed placement_id text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnPlacementIdCommitted;

	// Emits committed prop text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnPropCommitted;

	// Emits committed at.segment text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnSegmentCommitted;

	// Emits committed at.along_m text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnAlongCommitted;

	// Emits committed at.offset_m text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnOffsetCommitted;

	// Emits committed allow_blocking text with placement index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementTextCommitted OnAllowBlockingCommitted;

	// Updates index context and refreshes the placement block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetPlacementIndex(int32 inPlacementIndex);

	// Updates the shared typography catalog used by this placement widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Refreshes this placement widget from one template placement entry.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromPlacement(const FScenarioTemplateObstaclePlacement& placement);

private:
	// Handles placement_id commits from the id row.
	UFUNCTION()
	void HandlePlacementIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles prop commits from the prop row.
	UFUNCTION()
	void HandlePropCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles at.segment commits from the segment row.
	UFUNCTION()
	void HandleSegmentCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles at.along_m commits from the along row.
	UFUNCTION()
	void HandleAlongCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles at.offset_m commits from the offset row.
	UFUNCTION()
	void HandleOffsetCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles allow_blocking commits from the boolean row.
	UFUNCTION()
	void HandleAllowBlockingCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Last placement used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FScenarioTemplateObstaclePlacement CachedPlacement;

	// True when CachedPlacement contains valid placement data from RefreshFromPlacement.
	UPROPERTY(Transient)
	bool bHasCachedPlacement = false;

	// Builds the native fallback placement tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Binds child field row delegates owned by this placement widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this placement widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies cached placement values to bound field rows.
	void ApplyCachedPlacementToRows();
	// Applies shared typography to this placement block and rows.
	void ApplyTextStyles();
	// Returns whether the cached placement kind should expose fixed-placement edits.
	bool IsFixedPlacement() const;
	// Returns a stable label for an obstacle placement kind.
	static FString PlacementKindToString(EScenarioTemplateObstaclePlacementKind kind);
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(const FScenarioTemplateNumberValue& value);
};
