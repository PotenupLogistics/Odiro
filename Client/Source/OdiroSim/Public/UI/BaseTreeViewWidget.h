#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseTreeViewWidget.generated.h"

class UBaseTreeRowWidget;
class UBorder;
class UImage;
class UPanelWidget;
class UTextBlock;

// One visual row for flattened tree views.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTreeRowWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies row item data to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the row item data.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tree")
	void SetItem(const FBaseTreeRowItem& inItem);

	// Returns the row item data.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tree")
	FBaseTreeRowItem GetItem() const { return Item; }

	// Broadcasts after the row is clicked.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Tree|Events")
	FBaseTreeRowEvent OnRowClicked;

	// Broadcasts after a row with children requests expansion handling.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Tree|Events")
	FBaseTreeRowEvent OnExpansionRequested;

protected:
	// Feeds rounded row material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Emits row delegates for caller-owned selection and expansion policies.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Row data rendered by this widget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetItem", Setter = "SetItem", BlueprintGetter = "GetItem", BlueprintSetter = "SetItem", Category = "UI|Base Tree", meta = (ExposeOnSpawn = "true"))
	FBaseTreeRowItem Item;

	// Per-depth indentation width; zero uses the active base spacing token.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree", meta = (ClampMin = "0.0"))
	float IndentWidth = 0.0f;

	// Rounded row surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Accent strip owned by the Widget Blueprint for selected rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectionBar;

	// Optional indentation spacer owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IndentBox;

	// Optional expander caret image owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ExpanderImage;

	// Optional row icon image owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// Primary label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Inline muted label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubLabelTextBlock;

	// Right-aligned label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RightLabelTextBlock;
};

// Flattened tree view surface that owns row widget generation only.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTreeViewWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies flattened item data to generated row widgets.
	virtual void SynchronizeBaseProperties() override;

	// Replaces the flattened tree row data.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tree")
	void SetItems(const TArray<FBaseTreeRowItem>& inItems);

	// Returns the flattened tree row data.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tree")
	const TArray<FBaseTreeRowItem>& GetItems() const { return Items; }

	// Selects one row by stable id.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tree")
	bool SelectItemById(FName itemId);

	// Returns the selected row id.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tree")
	FName GetSelectedId() const { return SelectedId; }

	// Broadcasts after selected row changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Tree|Events")
	FBaseSelectionChangedEvent OnSelectionChanged;

	// Broadcasts after a generated row is clicked.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Tree|Events")
	FBaseTreeRowEvent OnRowClicked;

	// Broadcasts after a generated row with children requests expansion handling.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Tree|Events")
	FBaseTreeRowEvent OnExpansionRequested;

protected:
	// Rebuilds generated tree rows when a row container is present.
	void RebuildRows();

	// Updates generated tree rows without replacing the widget tree.
	void RefreshRows();

	// Handles click events from generated rows.
	UFUNCTION()
	void HandleGeneratedRowClicked(UWidget* widget, FName rowId);

	// Handles expansion requests from generated rows.
	UFUNCTION()
	void HandleGeneratedExpansionRequested(UWidget* widget, FName rowId);

	// Flattened row data owned by the caller.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseTreeRowItem> Items;

	// Stable id for the selected row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedId", Category = "UI|Base Tree", meta = (ExposeOnSpawn = "true"))
	FName SelectedId;

	// Widget class used for generated tree rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	TSubclassOf<UBaseTreeRowWidget> RowWidgetClass;

	// Panel that receives generated row widgets.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RowContainer;
};
