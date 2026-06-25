#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarObstaclePlacementWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorWidgetClassCatalog;
class UScenarioTemplateFieldRowViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidgetTextStyleCatalog;

// Broadcasts a committed text edit for one static obstacle placement field.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FScenarioEditorSidebarObstaclePlacementFieldTextCommitted,
	int32,
	PlacementIndex,
	EScenarioEditorSidebarObstaclePlacementField,
	Field,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a committed min/max edit for one static obstacle placement numeric field.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FScenarioEditorSidebarObstaclePlacementFieldRangeCommitted,
	int32,
	PlacementIndex,
	EScenarioEditorSidebarObstaclePlacementField,
	Field,
	const FText&,
	MinText,
	const FText&,
	MaxText,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a structural edit request for one placement index.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarObstaclePlacementActionRequested,
	int32,
	PlacementIndex);

// Broadcasts a committed text edit for one static obstacle placement string-list item.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FScenarioEditorSidebarObstaclePlacementStringListItemTextCommitted,
	int32,
	PlacementIndex,
	EScenarioEditorSidebarObstaclePlacementField,
	Field,
	int32,
	ItemIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a structural edit request for one placement string-list item.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarObstaclePlacementStringListItemActionRequested,
	int32,
	PlacementIndex,
	EScenarioEditorSidebarObstaclePlacementField,
	Field,
	int32,
	ItemIndex);

// Detail block for one root.obstacles.placements[] entry.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarObstaclePlacementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
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

	// WBP class catalog used to create dynamic string-list item rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	// Optional block wrapping this placement detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> PlacementBlockWidget;

	// Optional editable row for root.obstacles.placements[].id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PlacementIdFieldRow;

	// Optional editable row for root.obstacles.placements[].kind.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> KindFieldRow;

	// Optional editable row for root.obstacles.placements[].prop.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PropFieldRow;

	// Optional editable row for root.obstacles.placements[].pattern.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PatternFieldRow;

	// Optional editable row for root.obstacles.placements[].at.segment.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SegmentFieldRow;

	// Optional editable row for root.obstacles.placements[].at.lane.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> LaneFieldRow;

	// Optional editable row for root.obstacles.placements[].at.along_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AlongFieldRow;

	// Optional editable row for root.obstacles.placements[].at.offset_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> OffsetFieldRow;

	// Optional editable row for root.obstacles.placements[].zone.segments[].
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> ZoneSegmentsFieldRow;

	// Optional editable row for root.obstacles.placements[].zone.lanes[].
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> ZoneLanesFieldRow;

	// Optional editable row for root.obstacles.placements[].palette.categories[].
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PaletteCategoriesFieldRow;

	// Optional editable row for root.obstacles.placements[].palette.classes[].
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> PaletteClassesFieldRow;

	// Optional editable row for root.obstacles.placements[].count.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> CountFieldRow;

	// Optional editable row for root.obstacles.placements[].spacing_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SpacingFieldRow;

	// Optional editable row for root.obstacles.placements[].gap_width_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> GapWidthFieldRow;

	// Optional editable row for root.obstacles.placements[].density_per_10m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> DensityFieldRow;

	// Optional editable row for root.obstacles.placements[].yaw_deg.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> YawFieldRow;

	// Optional editable row for root.obstacles.placements[].allow_blocking.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AllowBlockingFieldRow;

	// Emits committed text for string, enum, boolean, or fixed numeric placement fields.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementFieldTextCommitted OnFieldTextCommitted;

	// Emits committed range text for numeric placement fields.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementFieldRangeCommitted OnFieldRangeCommitted;

	// Emits an add request using this placement index as insertion context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementActionRequested OnAddRequested;

	// Emits a remove request for this placement index.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementActionRequested OnRemoveRequested;

	// Emits committed text for one placement string-list item.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementStringListItemTextCommitted OnStringListItemTextCommitted;

	// Emits an add request for one placement string-list item collection.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementStringListItemActionRequested OnStringListItemAddRequested;

	// Emits a remove request for one placement string-list item.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarObstaclePlacementStringListItemActionRequested OnStringListItemRemoveRequested;

	// Updates index context and refreshes the placement block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetPlacementIndex(int32 inPlacementIndex);

	// Updates the shared typography catalog used by this placement widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates the WBP class catalog used by dynamic child rows.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetWidgetClassCatalog(TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog);

	// Refreshes this placement widget from one template placement entry.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromPlacement(const FScenarioTemplateObstaclePlacement& placement);

private:
	// Handles id row commits.
	UFUNCTION()
	void HandlePlacementIdCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles kind row commits.
	UFUNCTION()
	void HandleKindCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles prop row commits.
	UFUNCTION()
	void HandlePropCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles pattern row commits.
	UFUNCTION()
	void HandlePatternCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles at.segment row commits.
	UFUNCTION()
	void HandleSegmentCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles at.lane row commits.
	UFUNCTION()
	void HandleLaneCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles fixed at.along_m commits.
	UFUNCTION()
	void HandleAlongCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max at.along_m commits.
	UFUNCTION()
	void HandleAlongRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles fixed at.offset_m commits.
	UFUNCTION()
	void HandleOffsetCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max at.offset_m commits.
	UFUNCTION()
	void HandleOffsetRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles zone.segments row commits.
	UFUNCTION()
	void HandleZoneSegmentsCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles zone.lanes row commits.
	UFUNCTION()
	void HandleZoneLanesCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles palette.categories row commits.
	UFUNCTION()
	void HandlePaletteCategoriesCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles palette.classes row commits.
	UFUNCTION()
	void HandlePaletteClassesCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles fixed count commits.
	UFUNCTION()
	void HandleCountCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max count commits.
	UFUNCTION()
	void HandleCountRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles fixed spacing_m commits.
	UFUNCTION()
	void HandleSpacingCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max spacing_m commits.
	UFUNCTION()
	void HandleSpacingRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles fixed gap_width_m commits.
	UFUNCTION()
	void HandleGapWidthCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max gap_width_m commits.
	UFUNCTION()
	void HandleGapWidthRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles fixed density_per_10m commits.
	UFUNCTION()
	void HandleDensityCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max density_per_10m commits.
	UFUNCTION()
	void HandleDensityRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles fixed yaw_deg commits.
	UFUNCTION()
	void HandleYawCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles min/max yaw_deg commits.
	UFUNCTION()
	void HandleYawRangeCommitted(const FText& minText, const FText& maxText, ETextCommit::Type commitMethod);
	// Handles allow_blocking commits.
	UFUNCTION()
	void HandleAllowBlockingCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles placement add button requests.
	UFUNCTION()
	void HandleAddRequested();
	// Handles placement remove button requests.
	UFUNCTION()
	void HandleRemoveRequested();

	// Handles add requests for zone.segments[].
	UFUNCTION()
	void HandleZoneSegmentsAddRequested();
	// Handles add requests for zone.lanes[].
	UFUNCTION()
	void HandleZoneLanesAddRequested();
	// Handles add requests for palette.categories[].
	UFUNCTION()
	void HandlePaletteCategoriesAddRequested();
	// Handles add requests for palette.classes[].
	UFUNCTION()
	void HandlePaletteClassesAddRequested();
	// Handles text commits for zone.segments[] items.
	UFUNCTION()
	void HandleZoneSegmentItemCommitted(int32 itemIndex, const FText& text, ETextCommit::Type commitMethod);
	// Handles text commits for zone.lanes[] items.
	UFUNCTION()
	void HandleZoneLaneItemCommitted(int32 itemIndex, const FText& text, ETextCommit::Type commitMethod);
	// Handles text commits for palette.categories[] items.
	UFUNCTION()
	void HandlePaletteCategoryItemCommitted(int32 itemIndex, const FText& text, ETextCommit::Type commitMethod);
	// Handles text commits for palette.classes[] items.
	UFUNCTION()
	void HandlePaletteClassItemCommitted(int32 itemIndex, const FText& text, ETextCommit::Type commitMethod);
	// Handles remove requests for zone.segments[] items.
	UFUNCTION()
	void HandleZoneSegmentItemRemoveRequested(int32 itemIndex);
	// Handles remove requests for zone.lanes[] items.
	UFUNCTION()
	void HandleZoneLaneItemRemoveRequested(int32 itemIndex);
	// Handles remove requests for palette.categories[] items.
	UFUNCTION()
	void HandlePaletteCategoryItemRemoveRequested(int32 itemIndex);
	// Handles remove requests for palette.classes[] items.
	UFUNCTION()
	void HandlePaletteClassItemRemoveRequested(int32 itemIndex);

	// Last placement used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FScenarioTemplateObstaclePlacement CachedPlacement;

	// True when CachedPlacement contains valid placement data from RefreshFromPlacement.
	UPROPERTY(Transient)
	bool bHasCachedPlacement = false;

	// Field row ViewModels generated from CachedPlacement.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> CachedFieldItems;

	// Dynamic item rows for root.obstacles.placements[].zone.segments[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>> ZoneSegmentItemRows;

	// Dynamic item rows for root.obstacles.placements[].zone.lanes[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>> ZoneLaneItemRows;

	// Dynamic item rows for root.obstacles.placements[].palette.categories[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>> PaletteCategoryItemRows;

	// Dynamic item rows for root.obstacles.placements[].palette.classes[].
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>> PaletteClassItemRows;

	// Binds child field row delegates owned by this placement widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this placement widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies cached placement values to bound field rows.
	void ApplyCachedPlacementToRows();
	// Rebuilds cached field row ViewModels from the current placement.
	void RefreshFieldItemsFromViewModel();
	// Applies shared typography to this placement block and rows.
	void ApplyTextStyles();
	// Broadcasts a text commit for one placement field.
	void BroadcastText(EScenarioEditorSidebarObstaclePlacementField field, const FText& text, ETextCommit::Type commitMethod);
	// Broadcasts a range commit for one placement field.
	void BroadcastRange(
		EScenarioEditorSidebarObstaclePlacementField field,
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);
	// Resolves the ViewModel that formats template sidebar field items.
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const;
	// Finds one cached field row ViewModel by id.
	UScenarioTemplateFieldRowViewModel* FindCachedFieldItem(const FString& fieldId) const;
	// Applies collection summary state and rebuilds dynamic item rows for one string-list field.
	void RefreshStringListRows(
		EScenarioEditorSidebarObstaclePlacementField field,
		UScenarioEditorSidebarFieldRow* collectionFieldRow,
		const TArray<FString>& values,
		const TArray<FString>& options,
		const FString& itemLabelPrefix,
		bool bVisible);
	// Adds one string-list item row below the collection field row.
	UScenarioEditorSidebarFieldRow* AddStringListItemRow(
		EScenarioEditorSidebarObstaclePlacementField field,
		UScenarioEditorSidebarFieldRow* collectionFieldRow,
		int32 itemIndex,
		const FString& value,
		const TArray<FString>& options,
		const FString& itemLabelPrefix);
	// Removes all dynamic item rows for one string-list field.
	void ClearStringListRows(EScenarioEditorSidebarObstaclePlacementField field);
	// Resolves the dynamic row storage for one string-list field.
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>>* ResolveStringListRows(
		EScenarioEditorSidebarObstaclePlacementField field);
	// Emits text edits from dynamic string-list item rows.
	void BroadcastStringListItemText(
		EScenarioEditorSidebarObstaclePlacementField field,
		int32 itemIndex,
		const FText& text,
		ETextCommit::Type commitMethod);
	// Emits add/remove requests from string-list collection and item rows.
	void BroadcastStringListItemAction(
		FScenarioEditorSidebarObstaclePlacementStringListItemActionRequested& action,
		EScenarioEditorSidebarObstaclePlacementField field,
		int32 itemIndex);
};
