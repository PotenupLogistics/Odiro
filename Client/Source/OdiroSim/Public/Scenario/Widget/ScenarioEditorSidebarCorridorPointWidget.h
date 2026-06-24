#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "ScenarioEditorSidebarCorridorPointWidget.generated.h"

class UScenarioEditorSidebarBlockWidget;
class UScenarioTemplateFieldRowViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidgetTextStyleCatalog;

// Broadcasts a committed Corridor axis point coordinate edit with point index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarCorridorPointValueCommitted,
	int32,
	PointIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts a Corridor axis point structural edit request with point index context.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarCorridorPointActionRequested,
	int32,
	PointIndex);

// Detail block for one Corridor axis point in root.corridor.axis.points_m[].
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarCorridorPointWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds point field row delegates after UMG construction.
	virtual void NativeConstruct() override;

	// Releases point field row delegates before teardown.
	virtual void NativeDestruct() override;

	// Index of this point inside root.corridor.axis.points_m[].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 PointIndex = INDEX_NONE;

	// Shared typography catalog passed down to this point block and rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional block wrapping this point detail row group.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarBlockWidget> PointBlockWidget;

	// Optional editable row for the point x coordinate in meters.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> XFieldRow;

	// Optional editable row for the point y coordinate in meters.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> YFieldRow;

	// Emits committed x coordinate text with point index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorPointValueCommitted OnXCommitted;

	// Emits committed y coordinate text with point index context.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorPointValueCommitted OnYCommitted;

	// Emits when this point requests inserting another point after itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorPointActionRequested OnAddPointRequested;

	// Emits when this point requests removing itself.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarCorridorPointActionRequested OnRemovePointRequested;

	// Updates index context and refreshes the point block metadata.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetPointIndex(int32 inPointIndex);

	// Updates the shared typography catalog used by this point widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Refreshes this point widget from one template-local XY point in meters.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromPoint(const FVector2D& pointMeters);

private:
	// Handles x coordinate commits from the x row.
	UFUNCTION()
	void HandleXCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles y coordinate commits from the y row.
	UFUNCTION()
	void HandleYCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles add-point requests from the x row controls.
	UFUNCTION()
	void HandleAddPointRequested();

	// Handles remove-point requests from the x row controls.
	UFUNCTION()
	void HandleRemovePointRequested();

	// Last point used to refresh this widget across UMG construction timing.
	UPROPERTY(Transient)
	FVector2D CachedPointMeters = FVector2D::ZeroVector;

	// True when CachedPointMeters contains valid point data from RefreshFromPoint.
	UPROPERTY(Transient)
	bool bHasCachedPoint = false;

	// Field row ViewModels generated from the cached point state.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> CachedFieldItems;

	// Binds child field row delegates owned by this point widget.
	void BindFieldRows();
	// Releases child field row delegates owned by this point widget.
	void UnbindFieldRows();
	// Applies static labels, editability, and block metadata.
	void ConfigureFieldRows();
	// Regenerates field row ViewModels from the cached point state.
	void RefreshFieldItemsFromViewModel();
	// Applies cached point values to bound field rows.
	void ApplyCachedPointToRows();
	// Applies shared typography to this point block and rows.
	void ApplyTextStyles();
	// Resolves the Scenario Template sidebar ViewModel that creates item ViewModels.
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const;
	// Finds one cached field row ViewModel by id.
	UScenarioTemplateFieldRowViewModel* FindCachedFieldItem(const FString& fieldId) const;
};
