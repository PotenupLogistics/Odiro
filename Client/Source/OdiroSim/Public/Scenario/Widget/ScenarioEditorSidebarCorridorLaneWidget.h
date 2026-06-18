#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioEditorSidebarCorridorLaneWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UWidgetTextStyleCatalog;
class SWidget;

// Broadcasts a committed Corridor lane surface edit with side and lane index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FScenarioEditorSidebarCorridorLaneSurfaceCommitted,
	EScenarioEditorCorridorSide,
	Side,
	int32,
	LaneIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a committed Corridor lane fixed width edit with side and lane index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FScenarioEditorSidebarCorridorLaneWidthCommitted,
	EScenarioEditorCorridorSide,
	Side,
	int32,
	LaneIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a committed Corridor lane width range edit with side and lane index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FScenarioEditorSidebarCorridorLaneWidthRangeCommitted,
	EScenarioEditorCorridorSide,
	Side,
	int32,
	LaneIndex,
	const FText&,
	MinText,
	const FText&,
	MaxText,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a Corridor lane structural edit request with side and lane index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FScenarioEditorSidebarCorridorLaneActionRequested,
	EScenarioEditorCorridorSide,
	Side,
	int32,
	LaneIndex);

// Detail block for one Corridor side-lane rule.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarCorridorLaneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native lane detail block when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Binds lane field row delegates after UMG construction.
	virtual void NativeConstruct() override;

	// Releases lane field row delegates before teardown.
	virtual void NativeDestruct() override;

	// Corridor side profile that owns this lane.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	EScenarioEditorCorridorSide Side = EScenarioEditorCorridorSide::Building;

	// Index of this lane inside the side profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 LaneIndex = INDEX_NONE;

	// Shared typography catalog passed down to this lane block and rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional block wrapping this lane detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> LaneBlockWidget;

	// Optional editable surface id row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> SurfaceFieldRow;

	// Optional editable width_m row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> WidthFieldRow;

	// Emits committed surface text with side and lane index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorLaneSurfaceCommitted OnSurfaceCommitted;

	// Emits committed fixed width text with side and lane index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorLaneWidthCommitted OnWidthCommitted;

	// Emits committed width range text with side and lane index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorLaneWidthRangeCommitted OnWidthRangeCommitted;

	// Emits when this lane requests inserting another lane after itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorLaneActionRequested OnAddLaneRequested;

	// Emits when this lane requests removing itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorLaneActionRequested OnRemoveLaneRequested;

	// Updates side/index context and refreshes the lane block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetLaneContext(EScenarioEditorCorridorSide inSide, int32 inLaneIndex);

	// Updates the shared typography catalog used by this lane widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates available Corridor surface ids for the surface combo box.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetSurfaceOptions(const TArray<FString>& surfaceIds);

	// Refreshes this lane widget from one Scenario Template lane rule.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromLane(const FScenarioTemplateLaneRule& lane);

private:
	// Handles surface id text commits from the surface row.
	UFUNCTION()
	void HandleSurfaceCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles fixed width text commits from the width row.
	UFUNCTION()
	void HandleWidthCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles range width text commits from the width row.
	UFUNCTION()
	void HandleWidthRangeCommitted(
		const FText& minText,
		const FText& maxText,
		ETextCommit::Type commitMethod);

	// Handles add-lane requests from the surface row controls.
	UFUNCTION()
	void HandleAddLaneRequested();

	// Handles remove-lane requests from the surface row controls.
	UFUNCTION()
	void HandleRemoveLaneRequested();

	// Last lane rule used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FScenarioTemplateLaneRule CachedLane;

	// True when CachedLane contains valid lane data from RefreshFromLane.
	UPROPERTY(Transient)
	bool bHasCachedLane = false;

	// Catalog-backed Corridor surface ids available to the surface row.
	UPROPERTY(Transient)
	TArray<FString> SurfaceOptions;

	// Builds the native fallback lane tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Binds child field row delegates owned by this lane widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this lane widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Applies cached lane values to bound field rows.
	void ApplyCachedLaneToRows();
	// Applies shared typography to this lane block and rows.
	void ApplyTextStyles();
	// Formats one authored numeric value for editable text controls.
	static FString FormatEditableNumber(double value);
	// Returns the Scenario Template path for this lane's side profile.
	static FString MakeLanePath(EScenarioEditorCorridorSide side);
};
