#include "Scenario/Data/WidgetTextStyleCatalog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultWidgetTextStyleCatalogPath(
		TEXT("/Game/Data/UI/DA_WidgetTextStyleCatalog.DA_WidgetTextStyleCatalog"));
	const TCHAR* FreesentationFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation.Freesentation");

	FLinearColor MakeUiColor(const TCHAR* hex, const float alpha = 1.0f)
	{
		FLinearColor color = FLinearColor::FromSRGBColor(FColor::FromHex(hex));
		color.A = alpha;
		return color;
	}

	UObject* ResolveDefaultFontObject()
	{
		return LoadObject<UObject>(nullptr, FreesentationFontPath);
	}

	FName ResolveDefaultTypefaceName(const FName typefaceName)
	{
		if (typefaceName == FName(TEXT("Bold")))
		{
			return FName(TEXT("700 Bold"));
		}
		return FName(TEXT("400 Regular"));
	}

	FSlateFontInfo MakeDefaultFont(const FName typefaceName, const float size)
	{
		FSlateFontInfo fontInfo = FCoreStyle::GetDefaultFontStyle(typefaceName, size);
		if (UObject* fontObject = ResolveDefaultFontObject())
		{
			fontInfo.FontObject = fontObject;
			fontInfo.CompositeFont.Reset();
		}
		fontInfo.TypefaceFontName = ResolveDefaultTypefaceName(typefaceName);
		fontInfo.Size = size;
		return fontInfo;
	}

	FSlateFontInfo NormalizeFont(const FSlateFontInfo& candidate, const EWidgetTextStyleRole role)
	{
		FSlateFontInfo fallbackFont = UWidgetTextStyleCatalog::MakeDefaultStyle(role).Font;
		FSlateFontInfo normalizedFont = candidate;
		normalizedFont.FontObject = fallbackFont.FontObject;
		normalizedFont.FontMaterial = fallbackFont.FontMaterial;
		normalizedFont.CompositeFont = fallbackFont.CompositeFont;
		normalizedFont.OutlineSettings = fallbackFont.OutlineSettings;
		normalizedFont.TypefaceFontName = fallbackFont.TypefaceFontName;
		if (normalizedFont.Size <= 0.0f)
		{
			normalizedFont.Size = fallbackFont.Size;
		}
		if (normalizedFont.TypefaceFontName.IsNone())
		{
			normalizedFont.TypefaceFontName = fallbackFont.TypefaceFontName;
		}
		return normalizedFont;
	}

	const FWidgetTextStyle& SelectStyleForRole(
		const EWidgetTextStyleRole role,
		const FWidgetTextStyle& titleStyle,
		const FWidgetTextStyle& labelStyle,
		const FWidgetTextStyle& valueStyle,
		const FWidgetTextStyle& captionStyle)
	{
		switch (role)
		{
		case EWidgetTextStyleRole::Title:
			return titleStyle;
		case EWidgetTextStyleRole::Label:
			return labelStyle;
		case EWidgetTextStyleRole::Caption:
			return captionStyle;
		case EWidgetTextStyleRole::Value:
		default:
			return valueStyle;
		}
	}

	void ApplyResolvedTextBlockStyle(UTextBlock* textBlock, const FWidgetTextStyle& style)
	{
		if (!IsValid(textBlock))
		{
			return;
		}

		textBlock->SetFont(style.Font);
		textBlock->SetColorAndOpacity(FSlateColor(style.Color));
	}

	void ApplyResolvedEditableTextStyle(UEditableText* editableText, const FWidgetTextStyle& /*style*/)
	{
		if (!IsValid(editableText))
		{
			return;
		}

		// UEditableText only exposes whole-style replacement, which can crash Slate Prepass at runtime.
	}

	FSlateFontInfo MakeScenarioEditorEditableFont(const FWidgetTextStyle& style)
	{
		return NormalizeFont(style.Font, EWidgetTextStyleRole::Value);
	}

	FSlateBrush MakeScenarioEditorEditableBackgroundBrush()
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(MakeUiColor(TEXT("0F0F0F")));
		return brush;
	}

	FSlateBrush MakeScenarioEditorControlBrush(const TCHAR* hex)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(MakeUiColor(hex));
		return brush;
	}

	void ApplyScenarioEditorEditableTextBoxStyle(
		FEditableTextBoxStyle& textBoxStyle,
		const FWidgetTextStyle& style)
	{
		const FSlateFontInfo editableFont = MakeScenarioEditorEditableFont(style);
		const FSlateColor textColor(style.Color);
		FTextBlockStyle textStyle = textBoxStyle.TextStyle;
		textStyle.SetFont(editableFont);
		textStyle.SetColorAndOpacity(textColor);

		const FSlateBrush backgroundBrush = MakeScenarioEditorEditableBackgroundBrush();
		textBoxStyle
			.SetTextStyle(textStyle)
			.SetFont(editableFont)
			.SetForegroundColor(textColor)
			.SetReadOnlyForegroundColor(textColor)
			.SetFocusedForegroundColor(textColor)
			.SetBackgroundColor(FSlateColor(FLinearColor::White))
			.SetBackgroundImageNormal(backgroundBrush)
			.SetBackgroundImageHovered(MakeScenarioEditorControlBrush(TEXT("141414")))
			.SetBackgroundImageFocused(MakeScenarioEditorControlBrush(TEXT("151A20")))
			.SetBackgroundImageReadOnly(backgroundBrush)
			.SetPadding(FMargin(8.0f, 3.0f));
	}

	void ApplyResolvedEditableTextBoxStyle(UEditableTextBox* textBox, const FWidgetTextStyle& style)
	{
		if (!IsValid(textBox))
		{
			return;
		}

		ApplyScenarioEditorEditableTextBoxStyle(textBox->WidgetStyle, style);
		textBox->SynchronizeProperties();
		textBox->SetForegroundColor(style.Color);
	}

	void ApplyResolvedComboBoxStringStyle(UComboBoxString* comboBox, const FWidgetTextStyle& style)
	{
		if (!IsValid(comboBox))
		{
			return;
		}

		const FSlateFontInfo comboFont = MakeScenarioEditorEditableFont(style);
		const FSlateColor textColor(style.Color);
		const FSlateColor mutedTextColor(MakeUiColor(TEXT("878787")));
		FSlateBrush normalBrush = MakeScenarioEditorControlBrush(TEXT("0F0F0F"));
		FSlateBrush hoveredBrush = MakeScenarioEditorControlBrush(TEXT("141414"));
		FSlateBrush pressedBrush = MakeScenarioEditorControlBrush(TEXT("0A0A0A"));
		FSlateBrush focusedBrush = MakeScenarioEditorControlBrush(TEXT("151A20"));
		FSlateBrush panelBrush = MakeScenarioEditorControlBrush(TEXT("1B1B1B"));

		FButtonStyle buttonStyle = comboBox->GetWidgetStyle().ComboButtonStyle.ButtonStyle;
		buttonStyle
			.SetNormal(normalBrush)
			.SetHovered(hoveredBrush)
			.SetPressed(pressedBrush)
			.SetDisabled(normalBrush)
			.SetNormalPadding(FMargin(8.0f, 3.0f))
			.SetPressedPadding(FMargin(8.0f, 3.0f))
			.SetNormalForeground(textColor)
			.SetHoveredForeground(textColor)
			.SetPressedForeground(textColor)
			.SetDisabledForeground(mutedTextColor);

		FComboButtonStyle comboButtonStyle = comboBox->GetWidgetStyle().ComboButtonStyle;
		comboButtonStyle
			.SetButtonStyle(buttonStyle)
			.SetMenuBorderBrush(panelBrush)
			.SetMenuBorderPadding(FMargin(1.0f))
			.SetContentPadding(FMargin(8.0f, 3.0f));

		FComboBoxStyle comboStyle = comboBox->GetWidgetStyle();
		comboStyle
			.SetComboButtonStyle(comboButtonStyle)
			.SetContentPadding(FMargin(8.0f, 3.0f))
			.SetMenuRowPadding(FMargin(6.0f, 2.0f));
		comboBox->SetWidgetStyle(comboStyle);

		FTableRowStyle itemStyle = comboBox->GetItemStyle();
		itemStyle
			.SetTextColor(textColor)
			.SetSelectedTextColor(textColor)
			.SetEvenRowBackgroundBrush(panelBrush)
			.SetOddRowBackgroundBrush(panelBrush)
			.SetEvenRowBackgroundHoveredBrush(hoveredBrush)
			.SetOddRowBackgroundHoveredBrush(hoveredBrush)
			.SetSelectorFocusedBrush(focusedBrush)
			.SetActiveBrush(focusedBrush)
			.SetActiveHoveredBrush(hoveredBrush)
			.SetInactiveBrush(normalBrush)
			.SetInactiveHoveredBrush(hoveredBrush);
		comboBox->SetItemStyle(itemStyle);
		comboBox->SetContentPadding(FMargin(8.0f, 3.0f));

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		comboBox->Font = comboFont;
		comboBox->ForegroundColor = textColor;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		comboBox->SynchronizeProperties();
	}

	void ApplyResolvedMultiLineEditableTextBoxStyle(
		UMultiLineEditableTextBox* textBox,
		const FWidgetTextStyle& style)
	{
		if (!IsValid(textBox))
		{
			return;
		}

		ApplyScenarioEditorEditableTextBoxStyle(textBox->WidgetStyle, style);
		textBox->SynchronizeProperties();
		textBox->SetForegroundColor(style.Color);
	}

	FLinearColor NormalizeColor(const FLinearColor& candidate, const FLinearColor& fallbackColor)
	{
		if (!FMath::IsFinite(candidate.R)
			|| !FMath::IsFinite(candidate.G)
			|| !FMath::IsFinite(candidate.B)
			|| !FMath::IsFinite(candidate.A)
			|| candidate.A <= 0.0f)
		{
			return fallbackColor;
		}
		return candidate;
	}

	FWidgetTextStyle NormalizeStyle(
		const FWidgetTextStyle& candidate,
		const EWidgetTextStyleRole role)
	{
		const FWidgetTextStyle fallbackStyle = UWidgetTextStyleCatalog::MakeDefaultStyle(role);
		return FWidgetTextStyle(
			NormalizeFont(candidate.Font, role),
			NormalizeColor(candidate.Color, fallbackStyle.Color));
	}

	EWidgetTextStyleRole InferRoleFromWidgetName(const UWidget* widget)
	{
		if (!IsValid(widget))
		{
			return EWidgetTextStyleRole::Value;
		}

		const FString normalizedName = widget->GetName().ToLower();
		if (normalizedName.Contains(TEXT("title")) || normalizedName.Contains(TEXT("heading")))
		{
			return EWidgetTextStyleRole::Title;
		}
		if (normalizedName.Contains(TEXT("caption"))
			|| normalizedName.Contains(TEXT("path"))
			|| normalizedName.Contains(TEXT("address")))
		{
			return EWidgetTextStyleRole::Caption;
		}
		if (normalizedName.Contains(TEXT("label"))
			|| normalizedName.Contains(TEXT("category"))
			|| normalizedName.Contains(TEXT("button"))
			|| normalizedName.Contains(TEXT("tab")))
		{
			return EWidgetTextStyleRole::Label;
		}
		return EWidgetTextStyleRole::Value;
	}
}

FWidgetTextStyle::FWidgetTextStyle()
	: Font(MakeDefaultFont(TEXT("Regular"), 13.0f))
	, Color(FLinearColor::White)
{
}

FWidgetTextStyle::FWidgetTextStyle(const FSlateFontInfo& inFont, const FLinearColor& inColor)
	: Font(inFont)
	, Color(inColor)
{
}

UWidgetTextStyleCatalog::UWidgetTextStyleCatalog()
{
	Title = MakeDefaultStyle(EWidgetTextStyleRole::Title);
	Label = MakeDefaultStyle(EWidgetTextStyleRole::Label);
	Value = MakeDefaultStyle(EWidgetTextStyleRole::Value);
	Caption = MakeDefaultStyle(EWidgetTextStyleRole::Caption);
}

TSoftObjectPtr<UWidgetTextStyleCatalog> UWidgetTextStyleCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UWidgetTextStyleCatalog>(DefaultWidgetTextStyleCatalogPath);
}

FWidgetTextStyle UWidgetTextStyleCatalog::MakeDefaultStyle(const EWidgetTextStyleRole role)
{
	switch (role)
	{
	case EWidgetTextStyleRole::Title:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Bold"), 16.0f),
			MakeUiColor(TEXT("F1F1F1")));
	case EWidgetTextStyleRole::Label:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Bold"), 12.5f),
			MakeUiColor(TEXT("E6E6E6")));
	case EWidgetTextStyleRole::Value:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 12.5f),
			MakeUiColor(TEXT("CFCFCF")));
	case EWidgetTextStyleRole::Caption:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 10.5f),
			MakeUiColor(TEXT("8F8F8F")));
	default:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 12.5f),
			MakeUiColor(TEXT("CFCFCF")));
	}
}

FWidgetTextStyle UWidgetTextStyleCatalog::ResolveStyle(
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (const UWidgetTextStyleCatalog* catalog = catalogReference.LoadSynchronous())
	{
		return catalog->GetStyle(role);
	}

	TSoftObjectPtr<UWidgetTextStyleCatalog> defaultCatalog = MakeDefaultCatalogReference();
	if (const UWidgetTextStyleCatalog* catalog = defaultCatalog.LoadSynchronous())
	{
		return catalog->GetStyle(role);
	}

	return MakeDefaultStyle(role);
}

FWidgetTextStyle UWidgetTextStyleCatalog::ResolveStyle(const EWidgetTextStyleRole role)
{
	return ResolveStyle(MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyTextBlockStyle(
	UTextBlock* textBlock,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (!IsValid(textBlock))
	{
		return;
	}
	const FWidgetTextStyle style = ResolveStyle(catalogReference, role);
	ApplyResolvedTextBlockStyle(textBlock, style);
}

void UWidgetTextStyleCatalog::ApplyTextBlockStyle(
	UTextBlock* textBlock,
	const EWidgetTextStyleRole role)
{
	ApplyTextBlockStyle(textBlock, MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyEditableTextStyle(
	UEditableText* editableText,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (!IsValid(editableText))
	{
		return;
	}
	const FWidgetTextStyle style = ResolveStyle(catalogReference, role);
	ApplyResolvedEditableTextStyle(editableText, style);
}

void UWidgetTextStyleCatalog::ApplyEditableTextStyle(
	UEditableText* editableText,
	const EWidgetTextStyleRole role)
{
	ApplyEditableTextStyle(editableText, MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(
	UEditableTextBox* textBox,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (!IsValid(textBox))
	{
		return;
	}
	const FWidgetTextStyle style = ResolveStyle(catalogReference, role);
	ApplyResolvedEditableTextBoxStyle(textBox, style);
}

void UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(
	UEditableTextBox* textBox,
	const EWidgetTextStyleRole role)
{
	ApplyEditableTextBoxStyle(textBox, MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyComboBoxStringStyle(
	UComboBoxString* comboBox,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (!IsValid(comboBox))
	{
		return;
	}
	const FWidgetTextStyle style = ResolveStyle(catalogReference, role);
	ApplyResolvedComboBoxStringStyle(comboBox, style);
}

void UWidgetTextStyleCatalog::ApplyComboBoxStringStyle(
	UComboBoxString* comboBox,
	const EWidgetTextStyleRole role)
{
	ApplyComboBoxStringStyle(comboBox, MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyMultiLineEditableTextBoxStyle(
	UMultiLineEditableTextBox* textBox,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
	const EWidgetTextStyleRole role)
{
	if (!IsValid(textBox))
	{
		return;
	}
	const FWidgetTextStyle style = ResolveStyle(catalogReference, role);
	ApplyResolvedMultiLineEditableTextBoxStyle(textBox, style);
}

void UWidgetTextStyleCatalog::ApplyMultiLineEditableTextBoxStyle(
	UMultiLineEditableTextBox* textBox,
	const EWidgetTextStyleRole role)
{
	ApplyMultiLineEditableTextBoxStyle(textBox, MakeDefaultCatalogReference(), role);
}

void UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(
	UWidgetTree* widgetTree,
	const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference)
{
	if (!IsValid(widgetTree))
	{
		return;
	}
	const FWidgetTextStyle titleStyle = ResolveStyle(catalogReference, EWidgetTextStyleRole::Title);
	const FWidgetTextStyle labelStyle = ResolveStyle(catalogReference, EWidgetTextStyleRole::Label);
	const FWidgetTextStyle valueStyle = ResolveStyle(catalogReference, EWidgetTextStyleRole::Value);
	const FWidgetTextStyle captionStyle = ResolveStyle(catalogReference, EWidgetTextStyleRole::Caption);

	widgetTree->ForEachWidgetAndDescendants(
		[&titleStyle, &labelStyle, &valueStyle, &captionStyle](UWidget* widget)
		{
			if (!IsValid(widget))
			{
				return;
			}

			if (UTextBlock* textBlock = Cast<UTextBlock>(widget))
			{
				ApplyResolvedTextBlockStyle(
					textBlock,
					SelectStyleForRole(
						InferRoleFromWidgetName(textBlock),
						titleStyle,
						labelStyle,
						valueStyle,
						captionStyle));
				return;
			}
			if (UEditableText* editableText = Cast<UEditableText>(widget))
			{
				ApplyResolvedEditableTextStyle(editableText, valueStyle);
				return;
			}
			if (UEditableTextBox* editableTextBox = Cast<UEditableTextBox>(widget))
			{
				ApplyResolvedEditableTextBoxStyle(editableTextBox, valueStyle);
				return;
			}
			if (UComboBoxString* comboBox = Cast<UComboBoxString>(widget))
			{
				ApplyResolvedComboBoxStringStyle(comboBox, valueStyle);
				return;
			}
			if (UMultiLineEditableTextBox* multilineTextBox = Cast<UMultiLineEditableTextBox>(widget))
			{
				ApplyResolvedMultiLineEditableTextBoxStyle(multilineTextBox, valueStyle);
			}
		});
}

void UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(UWidgetTree* widgetTree)
{
	ApplyWidgetTreeTextStyles(widgetTree, MakeDefaultCatalogReference());
}

FWidgetTextStyle UWidgetTextStyleCatalog::GetStyle(const EWidgetTextStyleRole role) const
{
	switch (role)
	{
	case EWidgetTextStyleRole::Title:
		return NormalizeStyle(Title, role);
	case EWidgetTextStyleRole::Label:
		return NormalizeStyle(Label, role);
	case EWidgetTextStyleRole::Value:
		return NormalizeStyle(Value, role);
	case EWidgetTextStyleRole::Caption:
		return NormalizeStyle(Caption, role);
	default:
		return NormalizeStyle(Value, EWidgetTextStyleRole::Value);
	}
}
