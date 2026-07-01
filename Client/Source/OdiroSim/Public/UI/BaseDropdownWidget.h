#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseDropdownWidget.generated.h"

class UBaseButtonWidget;
class UBorder;
class UImage;
class UPanelWidget;
class UTextBlock;
class UWidget;

// Dropdown control with closed selected state and optional generated option rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseDropdownWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies selection, open state, and generated options to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Replaces available dropdown items.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetItems(const TArray<FBaseDropdownItem>& inItems);

	// Returns available dropdown items.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	const TArray<FBaseDropdownItem>& GetItems() const { return Items; }

	// Selects an enabled item by stable id.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	bool SelectItemById(FName itemId);

	// Returns the selected item id.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	FName GetSelectedId() const { return SelectedId; }

	// Updates text shown when no enabled item is selected.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetPlaceholderText(FText inPlaceholderText);

	// Returns text shown when no enabled item is selected.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	FText GetPlaceholderText() const { return PlaceholderText; }

	// Updates text shown inside an empty embedded option list.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetEmptyOptionsText(FText inEmptyOptionsText);

	// Returns text shown inside an empty embedded option list.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	FText GetEmptyOptionsText() const { return EmptyOptionsText; }

	// Updates whether the option list is visible.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetOpen(bool bInOpen);

	// Returns whether the option list is visible.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	bool IsOpen() const { return bOpen; }

	// Updates whether the dropdown is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetDisabled(bool bInDisabled);

	// Returns whether the dropdown is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after selection changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSelectionChangedEvent OnSelectionChanged;

protected:
	// Feeds rounded dropdown material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Makes the closed header hit-testable so clicks can toggle the list open.
	virtual void NativeConstruct() override;

	// Removes any spawned option list owned by this dropdown.
	virtual void NativeDestruct() override;

	// Toggles the option list when the header is clicked (option rows handle
	// their own clicks, so only header clicks reach here).
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Returns whether this instance should draw options inside its own WBP tree.
	bool UsesEmbeddedOptionList() const;

	// Opens the WBP-authored option list as a viewport popup below the anchor.
	void OpenOptionListAt(const FGeometry& anchorGeometry);

	// Closes the viewport popup option list owned by this dropdown.
	void CloseOptionList();

	// Rebuilds generated option buttons when an option container is present.
	void RebuildOptions();

	// Updates generated option button state without replacing the widget tree.
	void RefreshOptions();

	// Removes click bindings from generated option buttons before tree replacement or destruction.
	void UnbindGeneratedOptions();

	// Handles generated option button clicks.
	UFUNCTION()
	void HandleOptionClicked(UBaseButtonWidget* button);

	// Handles selections emitted by the spawned viewport option list.
	UFUNCTION()
	void HandlePopupSelectionChanged(UWidget* widget, FName selectedId);

	// Available dropdown items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseDropdownItem> Items;

	// Stable id for the selected item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedId", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	FName SelectedId;

	// Placeholder text shown when no valid selected item exists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetPlaceholderText", Setter = "SetPlaceholderText", BlueprintGetter = "GetPlaceholderText", BlueprintSetter = "SetPlaceholderText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText PlaceholderText = NSLOCTEXT("BaseDropdownWidget", "PlaceholderText", "Select...");

	// Placeholder row text shown when the embedded option list has no items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetEmptyOptionsText", Setter = "SetEmptyOptionsText", BlueprintGetter = "GetEmptyOptionsText", BlueprintSetter = "SetEmptyOptionsText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText EmptyOptionsText = NSLOCTEXT("BaseDropdownWidget", "EmptyOptionsText", "No options");

	// Whether the option list is visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsOpen", Setter = "SetOpen", BlueprintGetter = "IsOpen", BlueprintSetter = "SetOpen", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bOpen = false;

	// Disabled dropdown state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Widget class used for generated option buttons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseButtonWidget> OptionWidgetClass;

	// Widget class used for the viewport option-list popup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseDropdownWidget> MenuWidgetClass;

	// Viewport layer used for spawned dropdown option lists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	int32 MenuZOrder = 0;

	// Rounded closed dropdown surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Closed-state selected label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedTextBlock;

	// Closed-state selected icon owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedIconImage;

	// Closed-state caret icon owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CaretImage;

	// Closed-state content row owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ClosedContent;

	// Rounded wrapper drawn behind the option list (toggled with the open state).
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> OptionListSurface;

	// Panel that receives generated option buttons.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> OptionContainer;

	// Optional empty-list text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyOptionsTextBlock;

	// Stable ids aligned with generated option child indices.
	TArray<FName> OptionIdsByChildIndex;

	// Spawned option-list widget owned while this dropdown is open.
	UPROPERTY(Transient)
	TObjectPtr<UBaseDropdownWidget> ActiveMenuWidget;

	// Marks instances spawned only to render a viewport option list.
	UPROPERTY(Transient)
	bool bPopupInstance = false;
};
