#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseCheckBoxWidget.generated.h"

class UBaseCheckBoxWidget;
class UBorder;
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
	UFUNCTION(BlueprintCallable, Category = "UI|Contents")
	void SetLabel(FText inLabel);

	// Returns the checkbox label.
	UFUNCTION(BlueprintPure, Category = "UI|Contents")
	FText GetLabel() const { return Label; }

	// Updates the checked state.
	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetCheckState(ECheckBoxState inCheckState);

	// Returns the checked state.
	UFUNCTION(BlueprintPure, Category = "UI|State")
	ECheckBoxState GetCheckState() const { return CheckState; }

	// Updates whether the checkbox is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetDisabled(bool bInDisabled);

	// Returns whether the checkbox is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|State")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after checked state changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseCheckStateChangedEvent OnCheckStateChanged;

protected:
	// Feeds rounded checkbox material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Makes the decorative checkbox row receive pointer input directly.
	virtual void NativeConstruct() override;

	// Toggles the checked state when the whole decorative row is clicked.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Checkbox label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Current checked state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCheckState", Setter = "SetCheckState", BlueprintGetter = "GetCheckState", BlueprintSetter = "SetCheckState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	// Disabled checkbox state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

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
	// Creates designer-readable sample rows for newly created group widgets.
	UBaseCheckBoxGroupWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Applies group item state to bound generated checkbox rows.
	virtual void SynchronizeBaseProperties() override;

	// Replaces the group item data.
	UFUNCTION(BlueprintCallable, Category = "UI|Contents")
	void SetItems(const TArray<FBaseCheckBoxGroupItem>& inItems);

	// Returns the group item data.
	UFUNCTION(BlueprintPure, Category = "UI|Contents")
	const TArray<FBaseCheckBoxGroupItem>& GetItems() const { return Items; }

	// Updates one item and recalculates parent-child states.
	UFUNCTION(BlueprintCallable, Category = "UI|State")
	bool SetItemCheckState(FName itemId, ECheckBoxState inCheckState);

	// Returns one item's checked state or unchecked when missing.
	UFUNCTION(BlueprintPure, Category = "UI|State")
	ECheckBoxState GetItemCheckState(FName itemId) const;

	// Updates vertical placement of the generated checkbox list inside spare height.
	UFUNCTION(BlueprintCallable, Category = "UI|Layout")
	void SetContentVAlign(EBaseVerticalContentAlign inContentVAlign);

	// Returns vertical placement of the generated checkbox list inside spare height.
	UFUNCTION(BlueprintPure, Category = "UI|Layout")
	EBaseVerticalContentAlign GetContentVAlign() const { return ContentVAlign; }

	// Broadcasts after an item changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseCheckBoxGroupItemChangedEvent OnItemCheckStateChanged;

protected:
	// Removes generated item bindings before destruction.
	virtual void NativeDestruct() override;

	// Rebuilds generated checkbox widgets when a container is present.
	void RebuildItems();

	// Updates generated checkbox widgets without replacing the widget tree.
	void RefreshItems();

	// Removes check-state bindings from generated item widgets before tree replacement or destruction.
	void UnbindGeneratedItems();

	// Recalculates ancestor state from direct child states.
	void RefreshParentStates();

	// Handles generated checkbox state changes.
	UFUNCTION()
	void HandleItemWidgetCheckStateChanged(UWidget* widget, ECheckBoxState inCheckState);

	// Group item data including parent-child relationships.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseCheckBoxGroupItem> Items;

	// Vertical placement for the generated checkbox list when the widget has spare height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetContentVAlign", Setter = "SetContentVAlign", BlueprintGetter = "GetContentVAlign", BlueprintSetter = "SetContentVAlign", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseVerticalContentAlign ContentVAlign = EBaseVerticalContentAlign::Top;

	// Widget class used for generated checkbox rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseCheckBoxWidget> ItemWidgetClass;

	// Panel that receives generated checkbox rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemContainer;

	// Stable ids aligned with generated item child indices.
	TArray<FName> ItemIdsByChildIndex;
};
