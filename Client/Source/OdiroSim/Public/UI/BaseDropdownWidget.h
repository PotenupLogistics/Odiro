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
class USizeBox;
class UTextBlock;
class UWidget;

// Dropdown control with closed selected state and optional generated option rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseDropdownWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Creates a dropdown with WBP-owned layout defaults and runtime dropdown state.
	UBaseDropdownWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

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

	// Updates the selected id without emitting user-selection events.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetSelectedId(FName itemId);

	// Clears the selected item so placeholder text is shown.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void ClearSelection();

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

	// Updates the closed control height; 0 preserves the WBP-authored root height.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Dropdown")
	void SetControlHeightOverride(float inControlHeightOverride);

	// Returns the default closed control height override.
	UFUNCTION(BlueprintPure, Category = "UI|Base Dropdown")
	float GetControlHeightOverride() const { return ControlHeightOverride; }

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

	// Returns the option row class configured by the Widget Blueprint or caller.
	TSubclassOf<UBaseButtonWidget> ResolveOptionWidgetClass() const;

	// Returns the popup menu class configured by the Widget Blueprint or caller.
	TSubclassOf<UBaseDropdownWidget> ResolveMenuWidgetClass() const;

	// Applies selection state, optionally requiring a selectable item and emitting the selection event.
	bool ApplySelectedId(FName itemId, bool bRequireEnabledItem, bool bBroadcastSelectionChanged);

	// Captures the WBP-authored root height before runtime size synchronization writes over it.
	void CaptureAuthoredRootMinHeight();

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedId", Setter = "SetSelectedId", BlueprintGetter = "GetSelectedId", BlueprintSetter = "SetSelectedId", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
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

	// Closed control height override; 0 keeps RootSizeBox's authored MinDesiredHeight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetControlHeightOverride", Setter = "SetControlHeightOverride", BlueprintGetter = "GetControlHeightOverride", BlueprintSetter = "SetControlHeightOverride", Category = "UI|Layout", meta = (DisplayName = "Control Height Override (px)", ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float ControlHeightOverride = 0.0f;

	// Widget class used for generated option buttons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseButtonWidget> OptionWidgetClass;

	// Widget class used for the viewport option-list popup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseDropdownWidget> MenuWidgetClass;

	// Viewport layer used for spawned dropdown option lists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	int32 MenuZOrder = 0;

	// Whether the option-list panel fill color is owned by this WBP instead of the color catalog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option List Style", meta = (ExposeOnSpawn = "true"))
	bool bUseOptionListSurfaceFillColorOverride = false;

	// WBP-authored option-list panel fill color used when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option List Style", meta = (EditCondition = "bUseOptionListSurfaceFillColorOverride", ExposeOnSpawn = "true"))
	FLinearColor OptionListSurfaceFillColorOverride = FLinearColor::Transparent;

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

	// Root size widget whose authored height was captured for this rebuilt widget tree.
	TWeakObjectPtr<USizeBox> CapturedRootMinHeightSource;

	// Initial RootSizeBox MinDesiredHeight value authored in the Widget Blueprint.
	float AuthoredRootMinHeight = 0.0f;

	// Whether RootSizeBox has been inspected for an authored MinDesiredHeight override.
	bool bHasCapturedAuthoredRootMinHeight = false;

	// Whether the captured RootSizeBox authored state had a MinDesiredHeight override.
	bool bAuthoredRootMinHeightOverride = false;

	// Spawned option-list widget owned while this dropdown is open.
	UPROPERTY(Transient)
	TObjectPtr<UBaseDropdownWidget> ActiveMenuWidget;

	// Marks instances spawned only to render a viewport option list.
	UPROPERTY(Transient)
	bool bPopupInstance = false;
};
