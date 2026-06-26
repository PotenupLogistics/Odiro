#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Styling/SlateTypes.h"

namespace
{
	// Applies padding when a field child is owned by a horizontal box row.
	void SetHorizontalSlotPadding(UWidget* widget, const FMargin& padding)
	{
		if (widget)
		{
			if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(widget->Slot))
			{
				horizontalSlot->SetPadding(padding);
			}
		}
	}

	// Applies padding when a field child is owned by a vertical box row.
	void SetVerticalSlotPadding(UWidget* widget, const FMargin& padding)
	{
		if (widget)
		{
			if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(widget->Slot))
			{
				verticalSlot->SetPadding(padding);
			}
		}
	}

	// Resolves compact typography for dense property rows inside nested detail blocks.
	FWidgetTextStyle ResolveCompactFieldStyle(
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		const EWidgetTextStyleRole role,
		const float fontSize)
	{
		FWidgetTextStyle style = UWidgetTextStyleCatalog::ResolveStyle(catalogReference, role);
		style.Font.Size = fontSize;
		return style;
	}

	// Applies compact text styling without changing the shared catalog asset.
	void ApplyCompactTextBlockStyle(
		UTextBlock* textBlock,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		const EWidgetTextStyleRole role,
		const float fontSize)
	{
		if (!IsValid(textBlock))
		{
			return;
		}

		const FWidgetTextStyle style = ResolveCompactFieldStyle(catalogReference, role, fontSize);
		textBlock->SetFont(style.Font);
		textBlock->SetColorAndOpacity(FSlateColor(style.Color));
	}

	// Applies compact editable text styling so value controls fit narrow sidebar columns.
	void ApplyCompactEditableTextBoxStyle(
		UEditableTextBox* textBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference)
	{
		if (!IsValid(textBox))
		{
			return;
		}

		const FWidgetTextStyle style =
			ResolveCompactFieldStyle(catalogReference, EWidgetTextStyleRole::Value, 11.0f);
		FTextBlockStyle textStyle = textBox->WidgetStyle.TextStyle;
		textStyle.SetFont(style.Font);
		textStyle.SetColorAndOpacity(FSlateColor(style.Color));
		textBox->WidgetStyle
			.SetTextStyle(textStyle)
			.SetFont(style.Font)
			.SetForegroundColor(FSlateColor(style.Color))
			.SetReadOnlyForegroundColor(FSlateColor(style.Color))
			.SetFocusedForegroundColor(FSlateColor(style.Color))
			.SetPadding(FMargin(5.0f, 1.0f));
		textBox->SynchronizeProperties();
		textBox->SetForegroundColor(style.Color);
	}

	// Applies compact multiline styling while preserving the existing editable-text box colors.
	void ApplyCompactMultiLineEditableTextBoxStyle(
		UMultiLineEditableTextBox* textBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference)
	{
		if (!IsValid(textBox))
		{
			return;
		}

		const FWidgetTextStyle style =
			ResolveCompactFieldStyle(catalogReference, EWidgetTextStyleRole::Value, 11.0f);
		FTextBlockStyle textStyle = textBox->WidgetStyle.TextStyle;
		textStyle.SetFont(style.Font);
		textStyle.SetColorAndOpacity(FSlateColor(style.Color));
		textBox->WidgetStyle
			.SetTextStyle(textStyle)
			.SetFont(style.Font)
			.SetForegroundColor(FSlateColor(style.Color))
			.SetReadOnlyForegroundColor(FSlateColor(style.Color))
			.SetFocusedForegroundColor(FSlateColor(style.Color))
			.SetPadding(FMargin(5.0f, 2.0f));
		textBox->SynchronizeProperties();
		textBox->SetForegroundColor(style.Color);
	}

	// Tightens combo-box typography and inner padding for long catalog values.
	void ApplyCompactComboBoxStringStyle(
		UComboBoxString* comboBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference)
	{
		if (!IsValid(comboBox))
		{
			return;
		}

		UWidgetTextStyleCatalog::ApplyComboBoxStringStyle(
			comboBox,
			catalogReference,
			EWidgetTextStyleRole::Value);
		const FWidgetTextStyle style =
			ResolveCompactFieldStyle(catalogReference, EWidgetTextStyleRole::Value, 11.0f);

		FComboBoxStyle comboStyle = comboBox->GetWidgetStyle();
		FComboButtonStyle comboButtonStyle = comboStyle.ComboButtonStyle;
		FButtonStyle buttonStyle = comboButtonStyle.ButtonStyle;
		buttonStyle
			.SetNormalPadding(FMargin(5.0f, 1.0f))
			.SetPressedPadding(FMargin(5.0f, 1.0f));
		comboButtonStyle
			.SetButtonStyle(buttonStyle)
			.SetContentPadding(FMargin(5.0f, 1.0f));
		comboStyle
			.SetComboButtonStyle(comboButtonStyle)
			.SetContentPadding(FMargin(5.0f, 1.0f))
			.SetMenuRowPadding(FMargin(5.0f, 1.0f));
		comboBox->SetWidgetStyle(comboStyle);
		comboBox->SetContentPadding(FMargin(5.0f, 1.0f));

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		comboBox->Font = style.Font;
		comboBox->ForegroundColor = FSlateColor(style.Color);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		comboBox->SynchronizeProperties();
	}
}

void UScenarioEditorSidebarFieldRow::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	ApplyVisualStyle();
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarFieldRow::SetFieldLabel(const FString& label)
{
	FieldLabel = label;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetValueText(const FString& text)
{
	ValueText = text;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetRangeValueText(const FString& minText, const FString& maxText)
{
	MinValueText = minText;
	MaxValueText = maxText;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetComboOptions(const TArray<FString>& options)
{
	ComboOptions = options;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetInputType(const EScenarioEditorSidebarFieldInputType inInputType)
{
	InputType = inInputType;
	bMultilineValue = inInputType == EScenarioEditorSidebarFieldInputType::MultilineText;
	if (!IsRangeCapable())
	{
		bRangeInputEnabled = false;
	}
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetEditable(const bool bInEditable)
{
	bEditable = bInEditable;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetMultilineValue(const bool bInMultilineValue)
{
	bMultilineValue = bInMultilineValue;
	InputType = bInMultilineValue
		? EScenarioEditorSidebarFieldInputType::MultilineText
		: EScenarioEditorSidebarFieldInputType::Text;
	bRangeInputEnabled = false;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetRangeInputEnabled(const bool bInRangeInputEnabled)
{
	bRangeInputEnabled = bInRangeInputEnabled && IsRangeCapable();
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetArrayControlsEnabled(const bool bInArrayControlsEnabled)
{
	bArrayControlsEnabled = bInArrayControlsEnabled;
	bAddItemControlVisible = bInArrayControlsEnabled;
	bRemoveItemControlVisible = bInArrayControlsEnabled;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetAddItemControlVisible(const bool bInAddItemControlVisible)
{
	bAddItemControlVisible = bInAddItemControlVisible;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetRemoveItemControlVisible(const bool bInRemoveItemControlVisible)
{
	bRemoveItemControlVisible = bInRemoveItemControlVisible;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetActionContextIndex(const int32 inActionContextIndex)
{
	ActionContextIndex = inActionContextIndex;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetComboAllowsUnset(
	const bool bInComboAllowsUnset,
	const FString& unsetDisplayText)
{
	bComboAllowsUnset = bInComboAllowsUnset;
	ComboUnsetDisplayText = unsetDisplayText.IsEmpty() ? FString(TEXT("(unset)")) : unsetDisplayText;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyVisualStyle();
	RefreshRow();
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
	SetVisibility(itemViewModel->IsFieldVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FString UScenarioEditorSidebarFieldRow::GetValueText() const
{
	if (UsesRangeInput())
	{
		return FString::Printf(TEXT("%s..%s"), *GetMinValueText(), *GetMaxValueText());
	}
	if (bEditable && UsesMultilineInput() && ValueMultiLineEditableTextBox)
	{
		return ValueMultiLineEditableTextBox->GetText().ToString();
	}
	if (bEditable && !UsesMultilineInput() && ValueEditableTextBox)
	{
		return ValueEditableTextBox->GetText().ToString();
	}
	if (bEditable && UsesComboInput() && ValueComboBox)
	{
		const FString selectedOption = ValueComboBox->GetSelectedOption();
		if (bComboAllowsUnset && selectedOption == ComboUnsetDisplayText)
		{
			return FString();
		}
		return selectedOption.IsEmpty() ? ValueText : selectedOption;
	}

	return ValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMinValueText() const
{
	return MinValueEditableTextBox ? MinValueEditableTextBox->GetText().ToString() : MinValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMaxValueText() const
{
	return MaxValueEditableTextBox ? MaxValueEditableTextBox->GetText().ToString() : MaxValueText;
}

void UScenarioEditorSidebarFieldRow::HandleValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	ValueText = text.ToString();
	OnValueTextCommitted.Broadcast(text, commitMethod);
	OnIndexedValueTextCommitted.Broadcast(ActionContextIndex, text, commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleValueComboSelectionChanged(
	FString selectedItem,
	const ESelectInfo::Type selectionType)
{
	if (selectionType == ESelectInfo::Direct)
	{
		return;
	}

	const bool bSelectedUnset = bComboAllowsUnset && selectedItem == ComboUnsetDisplayText;
	ValueText = bSelectedUnset ? FString() : selectedItem;
	OnValueTextCommitted.Broadcast(FText::FromString(ValueText), ETextCommit::Default);
	OnIndexedValueTextCommitted.Broadcast(ActionContextIndex, FText::FromString(ValueText), ETextCommit::Default);
}

void UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	MinValueText = text.ToString();
	OnRangeValueTextCommitted.Broadcast(text, FText::FromString(GetMaxValueText()), commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	MaxValueText = text.ToString();
	OnRangeValueTextCommitted.Broadcast(FText::FromString(GetMinValueText()), text, commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked()
{
	SetRangeInputEnabled(!bRangeInputEnabled);
}

void UScenarioEditorSidebarFieldRow::HandleAddItemClicked()
{
	OnAddItemRequested.Broadcast();
	OnIndexedAddItemRequested.Broadcast(ActionContextIndex);
}

void UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked()
{
	OnRemoveItemRequested.Broadcast();
	OnIndexedRemoveItemRequested.Broadcast(ActionContextIndex);
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
	}
	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueMultiLineEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (ValueComboBox)
	{
		ValueComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueComboSelectionChanged);
		ValueComboBox->OnSelectionChanged.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueComboSelectionChanged);
	}
	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
		MinValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
		MaxValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
		RangeToggleButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
		AddItemButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
		RemoveItemButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
}

void UScenarioEditorSidebarFieldRow::UnbindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (ValueComboBox)
	{
		ValueComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueComboSelectionChanged);
	}
	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
}

void UScenarioEditorSidebarFieldRow::ApplyVisualStyle()
{
	if (WidgetTree)
	{
		if (USizeBox* rowSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(FName(TEXT("SizeBox_0")))))
		{
			rowSizeBox->SetMinDesiredHeight(26.0f);
			SetVerticalSlotPadding(rowSizeBox, FMargin(0.0f, 0.0f, 0.0f, 1.0f));
		}
	}

	SetHorizontalSlotPadding(LabelTextBlock.Get(), FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	SetHorizontalSlotPadding(SeparatorTextBlock.Get(), FMargin(0.0f, 0.0f, 3.0f, 0.0f));
	SetHorizontalSlotPadding(ValueTextBlock.Get(), FMargin(1.0f, 0.0f, 0.0f, 0.0f));
	SetHorizontalSlotPadding(ValueEditableTextBox.Get(), FMargin(1.0f, 1.0f, 0.0f, 1.0f));
	SetHorizontalSlotPadding(ValueComboBox.Get(), FMargin(1.0f, 1.0f, 0.0f, 1.0f));
	SetHorizontalSlotPadding(ValueRangeBox.Get(), FMargin(1.0f, 1.0f, 0.0f, 1.0f));
	SetHorizontalSlotPadding(MinValueEditableTextBox.Get(), FMargin(0.0f, 0.0f, 2.0f, 0.0f));
	SetHorizontalSlotPadding(RangeSeparatorTextBlock.Get(), FMargin(1.0f, 0.0f, 3.0f, 0.0f));
	SetHorizontalSlotPadding(RangeToggleButton.Get(), FMargin(4.0f, 1.0f, 0.0f, 1.0f));
	SetHorizontalSlotPadding(AddItemButton.Get(), FMargin(4.0f, 1.0f, 0.0f, 1.0f));
	SetHorizontalSlotPadding(RemoveItemButton.Get(), FMargin(1.0f, 1.0f, 0.0f, 1.0f));

	if (ValueMultiLineSizeBox)
	{
		ValueMultiLineSizeBox->SetMinDesiredHeight(104.0f);
		SetHorizontalSlotPadding(ValueMultiLineSizeBox.Get(), FMargin(1.0f, 2.0f, 0.0f, 2.0f));
	}

	ApplyCompactTextBlockStyle(
		LabelTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label,
		11.5f);
	ApplyCompactTextBlockStyle(
		SeparatorTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption,
		10.f);
	ApplyCompactTextBlockStyle(
		ValueTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value,
		11.5f);
	ApplyCompactTextBlockStyle(
		RangeSeparatorTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption,
		10.f);
	ApplyCompactTextBlockStyle(
		RangeToggleTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption,
		10.f);
	ApplyCompactTextBlockStyle(
		AddItemTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label,
		11.f);
	ApplyCompactTextBlockStyle(
		RemoveItemTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label,
		11.f);
	ApplyCompactEditableTextBoxStyle(
		ValueEditableTextBox.Get(),
		TextStyleCatalog);
	ApplyCompactEditableTextBoxStyle(
		MinValueEditableTextBox.Get(),
		TextStyleCatalog);
	ApplyCompactEditableTextBoxStyle(
		MaxValueEditableTextBox.Get(),
		TextStyleCatalog);
	ApplyCompactComboBoxStringStyle(
		ValueComboBox.Get(),
		TextStyleCatalog);
	ApplyCompactMultiLineEditableTextBoxStyle(
		ValueMultiLineEditableTextBox.Get(),
		TextStyleCatalog);
}

void UScenarioEditorSidebarFieldRow::RefreshRow()
{
	SetTextBlockText(LabelTextBlock.Get(), FieldLabel);
	SetTextBlockText(SeparatorTextBlock.Get(), TEXT(":"));
	SetTextBlockText(ValueTextBlock.Get(), ValueText);
	SetTextBlockText(RangeSeparatorTextBlock.Get(), TEXT(".."));
	SetTextBlockText(RangeToggleTextBlock.Get(), bRangeInputEnabled ? TEXT("1") : TEXT("2"));
	SetTextBlockText(AddItemTextBlock.Get(), TEXT("+"));
	SetTextBlockText(RemoveItemTextBlock.Get(), TEXT("-"));

	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetText(FText::FromString(ValueText));
		ValueEditableTextBox->SetVisibility(bEditable && !UsesMultilineInput() && !UsesComboInput() && !UsesRangeInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueComboBox)
	{
		RefreshComboBoxOptions();
		ValueComboBox->SetVisibility(bEditable && UsesComboInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->SetText(FText::FromString(ValueText));
		ValueMultiLineEditableTextBox->SetVisibility(bEditable && UsesMultilineInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineSizeBox)
	{
		ValueMultiLineSizeBox->SetVisibility(bEditable && UsesMultilineInput() && ValueMultiLineEditableTextBox
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->SetText(FText::FromString(MinValueText));
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->SetText(FText::FromString(MaxValueText));
	}
	if (ValueRangeBox)
	{
		ValueRangeBox->SetVisibility(UsesRangeInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->SetVisibility(bEditable && IsRangeCapable()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (AddItemButton)
	{
		AddItemButton->SetVisibility(bAddItemControlVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->SetVisibility(bRemoveItemControlVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueTextBlock)
	{
		const bool bHasEditableControl = bEditable
			&& ((!UsesMultilineInput() && !UsesComboInput() && !UsesRangeInput() && ValueEditableTextBox)
				|| (UsesMultilineInput() && ValueMultiLineEditableTextBox)
				|| (UsesComboInput() && ValueComboBox)
				|| (UsesRangeInput() && ValueRangeBox));
		ValueTextBlock->SetVisibility(bHasEditableControl
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
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

void UScenarioEditorSidebarFieldRow::RefreshComboBoxOptions()
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

	ValueComboBox->ClearOptions();
	if (bComboAllowsUnset)
	{
		ValueComboBox->AddOption(ComboUnsetDisplayText);
	}
	for (const FString& option : resolvedOptions)
	{
		if (!option.IsEmpty())
		{
			ValueComboBox->AddOption(option);
		}
	}

	if (!ValueText.IsEmpty() && resolvedOptions.Contains(ValueText))
	{
		ValueComboBox->SetSelectedOption(ValueText);
	}
	else if (bComboAllowsUnset)
	{
		ValueComboBox->SetSelectedOption(ComboUnsetDisplayText);
	}
	else
	{
		ValueComboBox->ClearSelection();
	}
}

void UScenarioEditorSidebarFieldRow::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}
