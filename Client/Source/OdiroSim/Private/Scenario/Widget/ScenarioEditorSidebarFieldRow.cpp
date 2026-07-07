#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseDropdownWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseSliderWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	// Wide numeric bounds used by BaseTextInput range mode when a field has no semantic slider clamp.
	constexpr float UnboundedTextInputRangeLimit = 100000000.0f;

	// Internal dropdown id used for an explicit unset option.
	const FName UnsetDropdownItemId(TEXT("__ScenarioEditorUnset"));

	// Texture path for editing a numeric field as min/max range.
	const TCHAR* SidebarFieldRangeIconPath = TEXT("/Game/Widgets/Icon/icon_range.icon_range");

	// Texture path for editing a numeric field as a single fixed value.
	const TCHAR* SidebarFieldFixedIconPath = TEXT("/Game/Widgets/Icon/icon_fixed.icon_fixed");

	// Texture path for adding one row-owned array item.
	const TCHAR* SidebarFieldAddActionIconPath = TEXT("/Game/Widgets/Icon/T_icon_add-circle.T_icon_add-circle");

	// Texture path for removing one row-owned array item.
	const TCHAR* SidebarFieldRemoveActionIconPath = TEXT("/Game/Widgets/Icon/T_icon_trash.T_icon_trash");

	// Loads a field-row icon from a Blueprint-configurable soft reference.
	UTexture2D* LoadSidebarFieldIconTexture(const TSoftObjectPtr<UTexture2D>& textureReference)
	{
		return textureReference.IsNull() ? nullptr : textureReference.LoadSynchronous();
	}

	// Parses Scenario Template scalar text while tolerating common display suffixes.
	bool TryParseSliderScalar(const FString& text, float& outValue)
	{
		FString scalarText = text.TrimStartAndEnd();
		scalarText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
		scalarText.RemoveFromEnd(TEXT("deg"), ESearchCase::IgnoreCase);
		scalarText.TrimStartAndEndInline();
		return LexTryParseString(outValue, *scalarText) && FMath::IsFinite(outValue);
	}

	// Counts authored decimal places so BaseTextInput range values keep the field's existing precision.
	int32 CountDecimalPlaces(const FString& text)
	{
		FString scalarText = text.TrimStartAndEnd();
		scalarText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
		scalarText.RemoveFromEnd(TEXT("deg"), ESearchCase::IgnoreCase);
		scalarText.TrimStartAndEndInline();

		int32 decimalIndex = INDEX_NONE;
		if (!scalarText.FindChar(TEXT('.'), decimalIndex))
		{
			return -1;
		}

		return FMath::Clamp(scalarText.Len() - decimalIndex - 1, 0, 6);
	}

	// Formats committed range values using the active BaseTextInput display precision.
	FString FormatRangeValueText(const float value, const int32 displayDecimals)
	{
		if (displayDecimals >= 0)
		{
			return FString::Printf(TEXT("%.*f"), FMath::Clamp(displayDecimals, 0, 6), value);
		}

		FString text = FString::SanitizeFloat(value);
		text.RemoveFromEnd(TEXT(".0"));
		return text;
	}
}

UScenarioEditorSidebarFieldRow::UScenarioEditorSidebarFieldRow(
	const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	RangeInputIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarFieldRangeIconPath));
	FixedInputIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarFieldFixedIconPath));
	AddItemIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarFieldAddActionIconPath));
	RemoveItemIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarFieldRemoveActionIconPath));
}

void UScenarioEditorSidebarFieldRow::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarFieldRow::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const bool bWasSynchronizing = bSynchronizing;
	bSynchronizing = true;

	SetTextBlockText(LabelTextBlock.Get(), FieldLabel);
	SetTextBlockText(SeparatorTextBlock.Get(), TEXT(":"));

	const bool bUsesMultilineInput = UsesMultilineInput();
	const bool bUsesComboInput = UsesComboInput();
	const bool bUsesRangeInput = UsesRangeInput();
	const bool bShowDropdown = bEditable && bUsesComboInput;
	const bool bShowRangeInput = bUsesRangeInput;
	const bool bShowTextInput = !bShowDropdown && !bShowRangeInput;

	if (bShowRangeInput)
	{
		SetRangeTextInputState(ValueEditableTextBox.Get(), true);
	}
	else
	{
		SetTextInputState(ValueEditableTextBox.Get(), ValueText, bShowTextInput, bUsesMultilineInput);
	}

	if (ValueComboBox)
	{
		ValueComboBox->SetColorsOverride(ColorsOverride);
		ValueComboBox->SetSizesOverride(SizesOverride);
		ValueComboBox->SetDisabled(!bEditable);
		ValueComboBox->SetPlaceholderText(FText::FromString(ComboUnsetDisplayText));
		ValueComboBox->SetVisibility(bShowDropdown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshDropdownOptions();

	SetActionButtonState(
		RangeToggleButton.Get(),
		bEditable && IsRangeCapable(),
		LoadSidebarFieldIconTexture(bRangeInputEnabled ? RangeInputIconTexture : FixedInputIconTexture),
		FText::FromString(bRangeInputEnabled ? TEXT("1") : TEXT("2")));
	SetActionButtonState(
		AddItemButton.Get(),
		bAddItemControlVisible,
		LoadSidebarFieldIconTexture(AddItemIconTexture),
		FText::FromString(TEXT("+")));
	SetActionButtonState(
		RemoveItemButton.Get(),
		bRemoveItemControlVisible,
		LoadSidebarFieldIconTexture(RemoveItemIconTexture),
		FText::FromString(TEXT("-")));

	RefreshSlider();

	bSynchronizing = bWasSynchronizing;
}

void UScenarioEditorSidebarFieldRow::SetFieldLabel(const FString& label)
{
	FieldLabel = label;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetValueText(const FString& text)
{
	ValueText = text;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetRangeValueText(const FString& minText, const FString& maxText)
{
	MinValueText = minText;
	MaxValueText = maxText;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetComboOptions(const TArray<FString>& options)
{
	ComboOptions = options;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetComboOptionSummaries(
	const TMap<FString, FText>& optionDisplayTexts,
	const TMap<FString, TSoftObjectPtr<UTexture2D>>& optionThumbnailTextures)
{
	ComboOptionDisplayTextByValue = optionDisplayTexts;
	ComboOptionThumbnailByValue = optionThumbnailTextures;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetInputType(const EScenarioEditorSidebarFieldInputType inInputType)
{
	InputType = inInputType;
	bMultilineValue = inInputType == EScenarioEditorSidebarFieldInputType::MultilineText;
	if (!IsRangeCapable())
	{
		bRangeInputEnabled = false;
	}
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetEditable(const bool bInEditable)
{
	bEditable = bInEditable;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetMultilineValue(const bool bInMultilineValue)
{
	bMultilineValue = bInMultilineValue;
	InputType = bInMultilineValue
		? EScenarioEditorSidebarFieldInputType::MultilineText
		: EScenarioEditorSidebarFieldInputType::Text;
	bRangeInputEnabled = false;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetRangeInputEnabled(const bool bInRangeInputEnabled)
{
	bRangeInputEnabled = bInRangeInputEnabled && IsRangeCapable();
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetArrayControlsEnabled(const bool bInArrayControlsEnabled)
{
	bArrayControlsEnabled = bInArrayControlsEnabled;
	bAddItemControlVisible = bInArrayControlsEnabled;
	bRemoveItemControlVisible = bInArrayControlsEnabled;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetAddItemControlVisible(const bool bInAddItemControlVisible)
{
	bAddItemControlVisible = bInAddItemControlVisible;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetRemoveItemControlVisible(const bool bInRemoveItemControlVisible)
{
	bRemoveItemControlVisible = bInRemoveItemControlVisible;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetActionContextIndex(const int32 inActionContextIndex)
{
	ActionContextIndex = inActionContextIndex;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetComboAllowsUnset(
	const bool bInComboAllowsUnset,
	const FString& unsetDisplayText)
{
	bComboAllowsUnset = bInComboAllowsUnset;
	ComboUnsetDisplayText = unsetDisplayText.IsEmpty() ? FString(TEXT("(unset)")) : unsetDisplayText;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetSliderSpec(
	FScenarioEditorSidebarFieldSliderSpec inSliderSpec)
{
	SliderSpec = NormalizeSliderSpec(inSliderSpec);
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	SynchronizeBaseProperties();
}

void UScenarioEditorSidebarFieldRow::InitializeFromItemViewModel(
	UScenarioTemplateFieldRowViewModel* itemViewModel)
{
	if (!itemViewModel)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetFieldLabel(itemViewModel->GetTitle());
	SetValueText(itemViewModel->GetValueText());
	SetRangeValueText(itemViewModel->GetMinValueText(), itemViewModel->GetMaxValueText());
	SetComboOptions(itemViewModel->GetComboOptions());
	SetInputType(itemViewModel->GetInputType());
	SetEditable(itemViewModel->IsFieldEditable());
	SetRangeInputEnabled(itemViewModel->IsRangeInputEnabled());
	SetArrayControlsEnabled(itemViewModel->HasArrayControls());
	SetComboAllowsUnset(itemViewModel->AllowsComboUnset(), itemViewModel->GetComboUnsetDisplayText());
	SetSliderSpec(itemViewModel->GetSliderSpec());
	SetVisibility(itemViewModel->IsFieldVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FString UScenarioEditorSidebarFieldRow::GetValueText() const
{
	if (UsesRangeInput())
	{
		return FString::Printf(TEXT("%s..%s"), *GetMinValueText(), *GetMaxValueText());
	}
	if (bEditable && !UsesComboInput() && ValueEditableTextBox)
	{
		return ValueEditableTextBox->GetCurrentText().ToString();
	}
	if (bEditable && UsesComboInput() && ValueComboBox)
	{
		const FName selectedId = ValueComboBox->GetSelectedId();
		if (bComboAllowsUnset && selectedId == UnsetDropdownItemId)
		{
			return FString();
		}
		return selectedId.IsNone() ? ValueText : selectedId.ToString();
	}

	return ValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMinValueText() const
{
	return MinValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMaxValueText() const
{
	return MaxValueText;
}

void UScenarioEditorSidebarFieldRow::HandleValueTextCommitted(
	UBaseTextInputWidget* widget,
	const FText& text)
{
	if (bSynchronizing)
	{
		return;
	}

	(void)widget;
	ValueText = text.ToString();
	SynchronizeBaseProperties();
	BroadcastValueCommitted(text, ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::HandleDropdownSelectionChanged(
	UWidget* widget,
	const FName selectedId)
{
	if (bSynchronizing)
	{
		return;
	}

	(void)widget;
	ValueText = bComboAllowsUnset && selectedId == UnsetDropdownItemId
		? FString()
		: selectedId.ToString();
	SynchronizeBaseProperties();
	BroadcastValueCommitted(FText::FromString(ValueText), ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::HandleRangeValueCommitted(
	UBaseTextInputWidget* widget,
	const float lowerValue,
	const float upperValue)
{
	if (bSynchronizing)
	{
		return;
	}

	const int32 displayDecimals = widget
		? widget->GetDisplayDecimals()
		: FMath::Max(CountDecimalPlaces(MinValueText), CountDecimalPlaces(MaxValueText));
	MinValueText = FormatRangeValueText(lowerValue, displayDecimals);
	MaxValueText = FormatRangeValueText(upperValue, displayDecimals);
	SynchronizeBaseProperties();
	OnRangeValueTextCommitted.Broadcast(
		FText::FromString(MinValueText),
		FText::FromString(MaxValueText),
		ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked(UBaseButtonWidget* button)
{
	(void)button;
	SetRangeInputEnabled(!bRangeInputEnabled);
}

void UScenarioEditorSidebarFieldRow::HandleAddItemClicked(UBaseButtonWidget* button)
{
	(void)button;
	OnAddItemRequested.Broadcast();
	OnIndexedAddItemRequested.Broadcast(ActionContextIndex);
}

void UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked(UBaseButtonWidget* button)
{
	(void)button;
	OnRemoveItemRequested.Broadcast();
	OnIndexedRemoveItemRequested.Broadcast(ActionContextIndex);
}

void UScenarioEditorSidebarFieldRow::HandleSliderValueChanged(
	UWidget* widget,
	const float value)
{
	if (bSynchronizing)
	{
		return;
	}

	(void)widget;
	const float resolvedValue = SliderSpec.bInteger ? FMath::RoundToFloat(value) : value;
	ValueText = SliderSpec.bInteger
		? FString::FromInt(FMath::RoundToInt(resolvedValue))
		: FString::Printf(TEXT("%.*f"), FMath::Clamp(SliderSpec.DisplayDecimals, 0, 6), resolvedValue);
	SynchronizeBaseProperties();
	BroadcastValueCommitted(FText::FromString(ValueText), ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::HandleSliderRangeValueChanged(
	UWidget* widget,
	const float lowerValue,
	const float upperValue)
{
	if (bSynchronizing)
	{
		return;
	}

	(void)widget;
	const int32 decimals = FMath::Clamp(SliderSpec.DisplayDecimals, 0, 6);
	const float lower = SliderSpec.bInteger ? FMath::RoundToFloat(lowerValue) : lowerValue;
	const float upper = SliderSpec.bInteger ? FMath::RoundToFloat(upperValue) : upperValue;
	MinValueText = SliderSpec.bInteger
		? FString::FromInt(FMath::RoundToInt(lower))
		: FString::Printf(TEXT("%.*f"), decimals, lower);
	MaxValueText = SliderSpec.bInteger
		? FString::FromInt(FMath::RoundToInt(upper))
		: FString::Printf(TEXT("%.*f"), decimals, upper);
	const float midpoint = (lower + upper) * 0.5f;
	ValueText = SliderSpec.bInteger
		? FString::FromInt(FMath::RoundToInt(midpoint))
		: FString::Printf(TEXT("%.*f"), decimals, midpoint);
	SynchronizeBaseProperties();
	OnRangeValueTextCommitted.Broadcast(
		FText::FromString(MinValueText),
		FText::FromString(MaxValueText),
		ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::BindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnTextSubmitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnTextSubmitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnRangeValueCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeValueCommitted);
		ValueEditableTextBox->OnRangeValueCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeValueCommitted);
	}
	if (ValueComboBox)
	{
		ValueComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleDropdownSelectionChanged);
		ValueComboBox->OnSelectionChanged.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleDropdownSelectionChanged);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
		RangeToggleButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
		AddItemButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
		RemoveItemButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderValueChanged);
		ValueSlider->OnValueChanged.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderValueChanged);
		ValueSlider->OnRangeValueChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderRangeValueChanged);
		ValueSlider->OnRangeValueChanged.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderRangeValueChanged);
	}
}

void UScenarioEditorSidebarFieldRow::UnbindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnTextSubmitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnRangeValueCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeValueCommitted);
	}
	if (ValueComboBox)
	{
		ValueComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleDropdownSelectionChanged);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderValueChanged);
		ValueSlider->OnRangeValueChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleSliderRangeValueChanged);
	}
}

bool UScenarioEditorSidebarFieldRow::UsesMultilineInput() const
{
	return bMultilineValue || InputType == EScenarioEditorSidebarFieldInputType::MultilineText;
}

bool UScenarioEditorSidebarFieldRow::UsesComboInput() const
{
	return InputType == EScenarioEditorSidebarFieldInputType::ComboBox;
}

bool UScenarioEditorSidebarFieldRow::IsRangeCapable() const
{
	return InputType == EScenarioEditorSidebarFieldInputType::Range;
}

bool UScenarioEditorSidebarFieldRow::UsesRangeInput() const
{
	return bEditable && IsRangeCapable() && bRangeInputEnabled;
}

bool UScenarioEditorSidebarFieldRow::IsSliderCapable() const
{
	return InputType == EScenarioEditorSidebarFieldInputType::Integer
		|| InputType == EScenarioEditorSidebarFieldInputType::Number
		|| InputType == EScenarioEditorSidebarFieldInputType::Range;
}

bool UScenarioEditorSidebarFieldRow::ShouldShowSlider() const
{
	if (!ValueSlider || !bEditable || !IsSliderCapable() || !SliderSpec.bEnabled)
	{
		return false;
	}

	if (UsesRangeInput())
	{
		float minValue = 0.0f;
		float maxValue = 0.0f;
		return TryParseSliderScalar(MinValueText, minValue)
			&& TryParseSliderScalar(MaxValueText, maxValue);
	}

	float value = 0.0f;
	return TryParseSliderScalar(ValueText, value);
}

void UScenarioEditorSidebarFieldRow::RefreshDropdownOptions()
{
	if (!ValueComboBox)
	{
		return;
	}

	TArray<FString> resolvedOptions = ComboOptions;
	if (!ValueText.IsEmpty() && !resolvedOptions.Contains(ValueText))
	{
		resolvedOptions.Add(ValueText);
	}

	TArray<FBaseDropdownItem> dropdownItems;
	dropdownItems.Reserve(resolvedOptions.Num() + (bComboAllowsUnset ? 1 : 0));
	if (bComboAllowsUnset)
	{
		FBaseDropdownItem unsetItem;
		unsetItem.Id = UnsetDropdownItemId;
		unsetItem.Label = FText::FromString(ComboUnsetDisplayText);
		dropdownItems.Add(unsetItem);
	}
	for (const FString& option : resolvedOptions)
	{
		if (option.IsEmpty())
		{
			continue;
		}

		FBaseDropdownItem item;
		item.Id = FName(*option);
		item.Label = ResolveComboOptionDisplayText(option);
		item.Icon = ResolveComboOptionThumbnail(option);
		dropdownItems.Add(item);
	}

	ValueComboBox->SetItems(dropdownItems);
	if (!ValueText.IsEmpty())
	{
		ValueComboBox->SetSelectedId(FName(*ValueText));
	}
	else if (bComboAllowsUnset)
	{
		ValueComboBox->SetSelectedId(UnsetDropdownItemId);
	}
	else
	{
		ValueComboBox->SetSelectedId(NAME_None);
	}
}

void UScenarioEditorSidebarFieldRow::RefreshSlider()
{
	if (!ValueSlider)
	{
		return;
	}

	ValueSlider->SetColorsOverride(ColorsOverride);
	ValueSlider->SetSizesOverride(SizesOverride);
	ValueSlider->SetDisabled(!bEditable);
	ValueSlider->SetValueRange(SliderSpec.MinValue, SliderSpec.MaxValue);
	ValueSlider->SetRangeMode(UsesRangeInput());

	if (!ShouldShowSlider())
	{
		ValueSlider->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (UsesRangeInput())
	{
		float minValue = SliderSpec.MinValue;
		float maxValue = SliderSpec.MaxValue;
		if (TryParseSliderScalar(MinValueText, minValue) && TryParseSliderScalar(MaxValueText, maxValue))
		{
			ValueSlider->SetRangeValue(minValue, maxValue);
		}
	}
	else
	{
		float value = SliderSpec.MinValue;
		if (TryParseSliderScalar(ValueText, value))
		{
			ValueSlider->SetValue(value);
		}
	}
	ValueSlider->SetVisibility(ESlateVisibility::Visible);
}

void UScenarioEditorSidebarFieldRow::BroadcastValueCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnValueTextCommitted.Broadcast(text, commitMethod);
	OnIndexedValueTextCommitted.Broadcast(ActionContextIndex, text, commitMethod);
}

FText UScenarioEditorSidebarFieldRow::ResolveComboOptionDisplayText(const FString& option) const
{
	if (const FText* displayText = ComboOptionDisplayTextByValue.Find(option))
	{
		return *displayText;
	}
	return FText::FromString(option);
}

UTexture2D* UScenarioEditorSidebarFieldRow::ResolveComboOptionThumbnail(
	const FString& option) const
{
	if (const TSoftObjectPtr<UTexture2D>* thumbnailTexture = ComboOptionThumbnailByValue.Find(option))
	{
		return thumbnailTexture->IsNull() ? nullptr : thumbnailTexture->LoadSynchronous();
	}
	return nullptr;
}

void UScenarioEditorSidebarFieldRow::SetTextBlockText(
	UTextBlock* textWidget,
	const FString& text) const
{
	if (!textWidget)
	{
		return;
	}

	textWidget->SetText(FText::FromString(text));
}

void UScenarioEditorSidebarFieldRow::SetTextInputState(
	UBaseTextInputWidget* inputWidget,
	const FString& text,
	const bool bVisible,
	const bool bTextWrap)
{
	if (!inputWidget)
	{
		return;
	}

	inputWidget->SetColorsOverride(ColorsOverride);
	inputWidget->SetSizesOverride(SizesOverride);
	SetValueInputSizeConstraints(inputWidget, bTextWrap);
	inputWidget->SetInputMode(EBaseTextInputMode::Text);
	inputWidget->SetTextWrap(bTextWrap);
	inputWidget->SetDisabled(!bEditable);
	inputWidget->SetText(FText::FromString(text));
	inputWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UScenarioEditorSidebarFieldRow::SetRangeTextInputState(
	UBaseTextInputWidget* inputWidget,
	const bool bVisible)
{
	if (!inputWidget)
	{
		return;
	}

	const int32 displayDecimals = SliderSpec.bEnabled
		? FMath::Clamp(SliderSpec.DisplayDecimals, 0, 6)
		: FMath::Max(CountDecimalPlaces(MinValueText), CountDecimalPlaces(MaxValueText));
	const float rangeMin = SliderSpec.bEnabled ? SliderSpec.MinValue : -UnboundedTextInputRangeLimit;
	const float rangeMax = SliderSpec.bEnabled ? SliderSpec.MaxValue : UnboundedTextInputRangeLimit;

	float lowerValue = rangeMin;
	float upperValue = rangeMin;
	if (!TryParseSliderScalar(MinValueText, lowerValue))
	{
		lowerValue = 0.0f;
	}
	if (!TryParseSliderScalar(MaxValueText, upperValue))
	{
		upperValue = lowerValue;
	}

	inputWidget->SetColorsOverride(ColorsOverride);
	inputWidget->SetSizesOverride(SizesOverride);
	SetValueInputSizeConstraints(inputWidget, false);
	inputWidget->SetInputMode(EBaseTextInputMode::NumberRange);
	inputWidget->SetTextWrap(false);
	inputWidget->SetDisplayDecimals(displayDecimals);
	inputWidget->SetValueRange(rangeMin, rangeMax);
	inputWidget->SetRangeValue(lowerValue, upperValue);
	inputWidget->SetDisabled(!bEditable);
	inputWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UScenarioEditorSidebarFieldRow::SetValueInputSizeConstraints(
	UBaseTextInputWidget* inputWidget,
	const bool bTextWrap)
{
	if (!inputWidget || inputWidget != ValueEditableTextBox.Get())
	{
		return;
	}

	if (!bValueInputBaseSizeConstraintsCaptured)
	{
		ValueInputBaseSizeConstraints = inputWidget->GetSizeConstraints();
		bValueInputBaseSizeConstraintsCaptured = true;
	}

	FBaseWidgetSizeConstraints sizeConstraints = ValueInputBaseSizeConstraints;
	if (bTextWrap)
	{
		sizeConstraints.MinHeight = FMath::Max(sizeConstraints.MinHeight, MultilineValueHeight);
	}
	inputWidget->SetSizeConstraints(sizeConstraints);
}

void UScenarioEditorSidebarFieldRow::SetActionButtonState(
	UBaseButtonWidget* button,
	const bool bVisible,
	UTexture2D* icon,
	const FText& fallbackGlyph) const
{
	if (!button)
	{
		return;
	}

	button->SetColorsOverride(ColorsOverride);
	button->SetSizesOverride(SizesOverride);
	button->SetVariant(EBaseWidgetVariant::Ghost);
	button->SetIcon(icon);
	button->SetIconSize(GeneratedActionIconSize);
	button->SetIconGlyphText(icon ? FText::GetEmpty() : fallbackGlyph);
	button->SetLabel(FText::GetEmpty());
	button->SetDisabled(!bVisible);
	button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FScenarioEditorSidebarFieldSliderSpec UScenarioEditorSidebarFieldRow::NormalizeSliderSpec(
	FScenarioEditorSidebarFieldSliderSpec inSliderSpec)
{
	inSliderSpec.DisplayDecimals = FMath::Clamp(inSliderSpec.DisplayDecimals, 0, 6);
	if (!FMath::IsFinite(inSliderSpec.MinValue)
		|| !FMath::IsFinite(inSliderSpec.MaxValue)
		|| inSliderSpec.MinValue >= inSliderSpec.MaxValue)
	{
		inSliderSpec.bEnabled = false;
	}
	if (inSliderSpec.bInteger)
	{
		inSliderSpec.DisplayDecimals = 0;
	}
	return inSliderSpec;
}
