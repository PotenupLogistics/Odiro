#pragma once

#include "CoreMinimal.h"
#include "Components/CheckBox.h"
#include "BaseFormElementTypes.generated.h"

class UBaseTextInputWidget;
class UTexture2D;
class UWidget;

// Text field behavior variants exposed by the base text input component.
UENUM(BlueprintType)
enum class EBaseTextInputMode : uint8
{
	Text,
	Number,
	NumberRange,
	Multiline
};

// Toggle presentation variants sharing one checked-state contract.
UENUM(BlueprintType)
enum class EBaseToggleButtonStyle : uint8
{
	Button,
	Switch
};

// Thumbnail card media inset policy.
UENUM(BlueprintType)
enum class EBaseThumbnailMediaPaddingMode : uint8
{
	FullBleed,
	Inset
};

// Switcher segment data used by the generated segment button rows.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseSwitcherItem
{
	GENERATED_BODY()

	// Stable item id used for selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher")
	FName Id;

	// Visible segment label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher")
	FText Label;

	// Optional segment icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Whether the segment can be selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher")
	bool bDisabled = false;
};

// Dropdown option data used by closed and open dropdown states.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseDropdownItem
{
	GENERATED_BODY()

	// Stable item id used for selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Dropdown")
	FName Id;

	// Visible option label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Dropdown")
	FText Label;

	// Optional option icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Dropdown")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Whether the option can be selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Dropdown")
	bool bDisabled = false;
};

// Checkbox tree item data used by the base checkbox group.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseCheckBoxGroupItem
{
	GENERATED_BODY()

	// Stable item id used for updates and events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	FName Id;

	// Optional parent id used to derive parent indeterminate state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	FName ParentId;

	// Visible checkbox label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	FText Label;

	// Current check state owned by the group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	// Whether this item can be toggled by the user.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Check Box")
	bool bDisabled = false;
};

// Flattened row data consumed by the base tree view and row widget.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseTreeRowItem
{
	GENERATED_BODY()

	// Stable row id used for selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	FName Id;

	// Indentation depth in the already-flattened row list.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree", meta = (ClampMin = "0"))
	int32 Depth = 0;

	// Primary row label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	FText Label;

	// Muted inline label shown after the primary row label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	FText SubLabel;

	// Optional label aligned to the row's right edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	FText RightLabel;

	// Optional row icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Whether this row has children and should show an expander slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	bool bHasChildren = false;

	// Whether the expander should show the expanded state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	bool bExpanded = false;

	// Whether this row should render selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	bool bSelected = false;

	// Whether this row is inactive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tree")
	bool bDisabled = false;
};

// Context menu item data used by the standard item row.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseContextMenuItem
{
	GENERATED_BODY()

	// Stable command id emitted when the item is selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	FName Id;

	// Visible item label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	FText Label;

	// Optional shortcut label aligned to the right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	FText Shortcut;

	// Optional item icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Whether this item is a visual separator instead of a command.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	bool bSeparator = false;

	// Whether this command is destructive and should use danger text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	bool bDanger = false;

	// Whether this command can be selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	bool bDisabled = false;

	// Whether this row should show a submenu caret.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Context Menu")
	bool bHasSubMenu = false;
};

// Broadcasts text input text commits.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseTextInputTextEvent, UBaseTextInputWidget*, Widget, const FText&, Text);

// Broadcasts text input text edits before commit.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseTextInputTextChangedEvent, UBaseTextInputWidget*, Widget, const FText&, Text);

// Broadcasts text input numeric commits.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseTextInputNumberEvent, UBaseTextInputWidget*, Widget, float, Value);

// Broadcasts text input range commits.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBaseTextInputRangeEvent, UBaseTextInputWidget*, Widget, float, LowerValue, float, UpperValue);

// Broadcasts single-value slider changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseSliderValueEvent, UWidget*, Widget, float, Value);

// Broadcasts range slider changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBaseSliderRangeEvent, UWidget*, Widget, float, LowerValue, float, UpperValue);

// Broadcasts id-based selection changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseSelectionChangedEvent, UWidget*, Widget, FName, SelectedId);

// Broadcasts tree row pointer actions without owning the caller's hierarchy policy.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseTreeRowEvent, UWidget*, Widget, FName, RowId);

// Broadcasts toggle and checkbox checked-state changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseCheckStateChangedEvent, UWidget*, Widget, ECheckBoxState, CheckState);

// Broadcasts checkbox group item changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBaseCheckBoxGroupItemChangedEvent, UWidget*, Widget, FName, ItemId, ECheckBoxState, CheckState);

// Broadcasts context menu command selections.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBaseContextMenuItemSelectedEvent, UWidget*, Widget, FName, ItemId);
