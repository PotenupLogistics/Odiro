#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseContextMenuWidget.generated.h"

class UBaseContextMenuItemWidget;
class UBaseContextMenuWidget;
class UBorder;
class UImage;
class UNamedSlot;
class UPanelWidget;
class UTextBlock;

// Standard context menu row supporting icon, shortcut, danger, separator, and submenu states.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseContextMenuItemWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Applies item data to bound row controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the context menu item data.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Context Menu")
	void SetItem(const FBaseContextMenuItem& inItem);

	// Returns the context menu item data.
	UFUNCTION(BlueprintPure, Category = "UI|Base Context Menu")
	FBaseContextMenuItem GetItem() const { return Item; }

	// Broadcasts after this row command is selected.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseContextMenuItemSelectedEvent OnItemSelected;

protected:
	// Emits the item id for command rows before forwarding the CommonUI click event.
	virtual void NativeOnClicked() override;

	// Context menu row data.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetItem", Setter = "SetItem", BlueprintGetter = "GetItem", BlueprintSetter = "SetItem", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FBaseContextMenuItem Item;

	// Shortcut label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShortcutTextBlock;

	// Optional submenu caret image owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SubMenuCaretImage;

	// Optional separator line owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SeparatorLineWidget;

	// Optional command-row content owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ItemContent;

	// Tracks whether the current item data, not caller state, forced the disabled flag.
	UPROPERTY(Transient)
	bool bItemForcesDisabled = false;
};

// Context menu container with a NamedSlot and optional generated standard item rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseContextMenuWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies menu surface and generated item rows to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Replaces menu items.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Context Menu")
	void SetItems(const TArray<FBaseContextMenuItem>& inItems);

	// Returns menu items.
	UFUNCTION(BlueprintPure, Category = "UI|Base Context Menu")
	const TArray<FBaseContextMenuItem>& GetItems() const { return Items; }

	// Updates text shown when no generated items are available.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Context Menu")
	void SetPlaceholderText(FText inPlaceholderText);

	// Returns text shown when no generated items are available.
	UFUNCTION(BlueprintPure, Category = "UI|Base Context Menu")
	FText GetPlaceholderText() const { return PlaceholderText; }

	// Broadcasts after a generated item row is selected.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseContextMenuItemSelectedEvent OnItemSelected;

protected:
	// Feeds rounded menu material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Rebuilds generated menu item rows when a container is present.
	void RebuildItems();

	// Updates generated menu item rows without replacing the widget tree.
	void RefreshItems();

	// Handles generated menu item row selections.
	UFUNCTION()
	void HandleGeneratedItemSelected(UWidget* widget, FName itemId);

	// Menu item data for generated rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseContextMenuItem> Items;

	// Placeholder text for empty generated menus.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetPlaceholderText", Setter = "SetPlaceholderText", BlueprintGetter = "GetPlaceholderText", BlueprintSetter = "SetPlaceholderText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText PlaceholderText = NSLOCTEXT("BaseContextMenuWidget", "PlaceholderText", "No menu items");

	// Widget class used for generated item rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseContextMenuItemWidget> ItemWidgetClass;

	// Rounded menu surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Slot for caller-authored arbitrary menu content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> ContentSlot;

	// Panel that receives generated standard item rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemContainer;

	// Optional empty-menu text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceholderTextBlock;
};

// Right-click anchor that opens a base context menu at the pointer position.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseContextMenuAnchorWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Creates example menu items so a dropped anchor has visible purpose.
	UBaseContextMenuAnchorWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Applies design-time placeholder text.
	virtual void SynchronizeBaseProperties() override;

	// Replaces menu items passed to spawned menu widgets.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Context Menu")
	void SetItems(const TArray<FBaseContextMenuItem>& inItems);

	// Returns menu items passed to spawned menu widgets.
	UFUNCTION(BlueprintPure, Category = "UI|Base Context Menu")
	const TArray<FBaseContextMenuItem>& GetItems() const { return Items; }

	// Broadcasts after a spawned menu item is selected.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseContextMenuItemSelectedEvent OnItemSelected;

protected:
	// Enables focus-loss dismissal for menus spawned by this anchor.
	virtual void NativeConstruct() override;

	// Opens on right click and closes on the next local click.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Closes the active menu when focus moves outside this anchor.
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	// Cleans up the active menu widget.
	virtual void NativeDestruct() override;

	// Opens the menu at a viewport position.
	void OpenMenuAt(const FVector2D& screenPosition);

	// Closes the active menu widget.
	void CloseMenu();

	// Handles selections emitted by the active menu.
	UFUNCTION()
	void HandleMenuItemSelected(UWidget* widget, FName itemId);

	// Menu widget class to spawn on right click.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseContextMenuWidget> MenuWidgetClass;

	// Items passed to spawned menu widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseContextMenuItem> Items;

	// Viewport layer used for spawned context menu widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	int32 MenuZOrder = 0;

	// Designer-visible hint rendered by the WBP when present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText PlaceholderText = NSLOCTEXT("BaseContextMenuAnchorWidget", "PlaceholderText", "Right-click for context menu");

	// Optional anchor hint text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceholderTextBlock;

	// Active menu widget owned by this anchor while visible.
	UPROPERTY(Transient)
	TObjectPtr<UBaseContextMenuWidget> ActiveMenuWidget;
};
