#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarCorridorSegmentWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UScenarioTemplateFieldRowViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidgetTextStyleCatalog;

// Broadcasts a committed Corridor segment text edit with segment index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarCorridorSegmentTextCommitted,
	int32,
	SegmentIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a committed Corridor segment range edit with segment index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FScenarioEditorSidebarCorridorSegmentRangeCommitted,
	int32,
	SegmentIndex,
	const FText&,
	MinText,
	const FText&,
	MaxText,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a Corridor segment structural edit request with segment index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarCorridorSegmentActionRequested,
	int32,
	SegmentIndex);

// Detail block for one Corridor semantic segment rule.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarCorridorSegmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds segment field row delegates after UMG construction.
	virtual void NativeConstruct() override;

	// Releases segment field row delegates before teardown.
	virtual void NativeDestruct() override;

	// Index of this segment inside root.corridor.segments[].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 SegmentIndex = INDEX_NONE;

	// Shared typography catalog passed down to this segment block and rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional block wrapping this segment detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> SegmentBlockWidget;

	// Optional editable row for root.corridor.segments[].id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> IdFieldRow;

	// Optional editable row for root.corridor.segments[].type.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> TypeFieldRow;

	// Optional editable row for root.corridor.segments[].along_range_m.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> AlongRangeFieldRow;

	// Optional editable row for root.corridor.segments[].replaced_by.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> ReplacedByFieldRow;

	// Emits committed id text with segment index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentTextCommitted OnIdCommitted;

	// Emits committed type text with segment index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentTextCommitted OnTypeCommitted;

	// Emits committed along range text with segment index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentRangeCommitted OnAlongRangeCommitted;

	// Emits committed replaced_by text with segment index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentTextCommitted OnReplacedByCommitted;

	// Emits when this segment requests inserting another segment after itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentActionRequested OnAddSegmentRequested;

	// Emits when this segment requests removing itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorSegmentActionRequested OnRemoveSegmentRequested;

	// Updates index context and refreshes the segment block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetSegmentIndex(int32 inSegmentIndex);

	// Updates the shared typography catalog used by this segment widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates available Corridor surface ids for the replaced_by combo box.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetSurfaceOptions(const TArray<FString>& surfaceIds);

	// Refreshes this segment widget from one Scenario Template segment rule.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromSegment(const FScenarioTemplateSegment& segment);

private:
	// Handles id text commits from the id row.
	UFUNCTION()
	void HandleIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles type text commits from the type row.
	UFUNCTION()
	void HandleTypeCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles along range commits from the range row.
	UFUNCTION()
	void HandleAlongRangeCommitted(
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles replaced_by text commits from the surface row.
	UFUNCTION()
	void HandleReplacedByCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles add-segment requests from the id row controls.
	UFUNCTION()
	void HandleAddSegmentRequested();

	// Handles remove-segment requests from the id row controls.
	UFUNCTION()
	void HandleRemoveSegmentRequested();

	// Last segment rule used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FScenarioTemplateSegment CachedSegment;

	// True when CachedSegment contains valid segment data from RefreshFromSegment.
	UPROPERTY(Transient)
	bool bHasCachedSegment = false;

	// Catalog-backed Corridor surface ids available to the replaced_by row.
	UPROPERTY(Transient)
	TArray<FString> SurfaceOptions;

	// Field row ViewModels generated from the cached segment state.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> CachedFieldItems;

	// Binds child field row delegates owned by this segment widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this segment widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Regenerates field row ViewModels from the cached segment state.
	void RefreshFieldItemsFromViewModel();
	// Applies cached segment values to bound field rows.
	void ApplyCachedSegmentToRows();
	// Applies shared typography to this segment block and rows.
	void ApplyTextStyles();
	// Resolves the Scenario Template sidebar ViewModel that creates item ViewModels.
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const;
	// Finds one cached field row ViewModel by id.
	UScenarioTemplateFieldRowViewModel* FindCachedFieldItem(const FString& fieldId) const;
};
