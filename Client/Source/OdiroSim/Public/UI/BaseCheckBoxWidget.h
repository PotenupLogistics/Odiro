#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseCheckBoxWidget.generated.h"

class UBaseCheckBoxWidget;
class UBorder;
class UCheckBox;
class UPanelWidget;
class UTextBlock;

// Base-token styled checkbox with checked, unchecked, indeterminate, and disabled states.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseCheckBoxWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies checked state, label, and disabled state to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the checkbox label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Check Box")
	void SetLabel(FText inLabel);

	// Returns the checkbox label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Check Box")
	FText GetLabel() const { return Label; }

	// Updates the checked state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Check Box")
	void SetCheckState(ECheckBoxState inCheckState);

	// Returns the checked state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Check Box")
	ECheckBoxState GetCheckState() const { return CheckState; }

	// Updates whether the checkbox is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Check Box")
	void SetDisabled(bool bInDisabled);

	// Returns whether the checkbox is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Check Box")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after checked state changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Check Box|Events")
	FBaseCheckStateChangedEvent OnCheckStateChanged;

protected:
	// Feeds rounded checkbox material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Binds optional checkbox child events after WBP construction.
	virtual void NativeConstruct() override;

	// Unbinds optional checkbox child events before destruction.
	virtual void NativeDestruct() override;

	// Toggles the checked state when the whole row is clicked (the visible box
	// and label are decorative, so the native checkbox alone never receives the
	// click). Indeterminate advances to checked.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Handles native UCheckBox state changes.
	UFUNCTION()
	void HandleNativeCheckStateChanged(bool bIsChecked);

	// Checkbox label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Base Check Box", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Current checked state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCheckState", Setter = "SetCheckState", BlueprintGetter = "GetCheckState", BlueprintSetter = "SetCheckState", Category = "UI|Base Check Box", meta = (ExposeOnSpawn = "true"))
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	// Disabled checkbox state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Check Box", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Prevents native checkbox callbacks from echoing synchronization writes.
	UPROPERTY(Transient)
	bool bSynchronizing = false;

	// Optional native checkbox for keyboard/input semantics.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> NativeCheckBox;

	// Rounded checkbox square owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BoxSurfaceBorder;

	// Optional checked glyph/image owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CheckMarkWidget;

	// Optional indeterminate dash owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IndeterminateMarkWidget;

	// Label text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;
};

// Data-driven checkbox group that derives parent indeterminate state from child items.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseCheckBoxGroupWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies group item state to bound generated checkbox rows.
	virtual void SynchronizeBaseProperties() override;

	// Replaces the group item data.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Check Box")
	void SetItems(const TArray<FBaseCheckBoxGroupItem>& inItems);

	// Returns the group item data.
	UFUNCTION(BlueprintPure, Category = "UI|Base Check Box")
	const TArray<FBaseCheckBoxGroupItem>& GetItems() const { return Items; }

	// Updates one item and recalculates parent-child states.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Check Box")
	bool SetItemCheckState(FName itemId, ECheckBoxState inCheckState);

	// Returns one item's checked state or unchecked when missing.
	UFUNCTION(BlueprintPure, Category = "UI|Base Check Box")
	ECheckBoxState GetItemCheckState(FName itemId) const;

	// Broadcasts after an item changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Check Box|Events")
	FBaseCheckBoxGroupItemChangedEvent OnItemCheckStateChanged;

protected:
	// Rebuilds generated checkbox widgets when a container is present.
	void RebuildItems();

	// Updates generated checkbox widgets without replacing the widget tree.
	void RefreshItems();

	// Recalculates ancestor state from direct child states.
	void RefreshParentStates();

	// Handles generated checkbox state changes.
	UFUNCTION()
	void HandleItemWidgetCheckStateChanged(UWidget* widget, ECheckBoxState inCheckState);

	// Group item data including parent-child relationships.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseCheckBoxGroupItem> Items;

	// Widget class used for generated checkbox rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	TSubclassOf<UBaseCheckBoxWidget> ItemWidgetClass;

	// Panel that receives generated checkbox rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemContainer;

	// Stable ids for generated checkbox widgets.
	TMap<TWeakObjectPtr<UBaseCheckBoxWidget>, FName> ItemIdByWidget;
};
