#include "Widget/WidgetTextStyleCatalog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Fonts/FontProviderInterface.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultWidgetTextStyleCatalogPath(
		TEXT("/Game/Data/UI/DA_WidgetTextStyleCatalog.DA_WidgetTextStyleCatalog"));

	FSlateFontInfo MakeDefaultFont(const FName typefaceName, const float size)
	{
		return FCoreStyle::GetDefaultFontStyle(typefaceName, size);
	}

	FSlateFontInfo NormalizeFont(const FSlateFontInfo& candidate, const EWidgetTextStyleRole role)
	{
		FSlateFontInfo fallbackFont = UWidgetTextStyleCatalog::MakeDefaultStyle(role).Font;
		FSlateFontInfo normalizedFont = candidate;
		const IFontProviderInterface* fontProvider =
			Cast<const IFontProviderInterface>(normalizedFont.FontObject);
		const bool bHasUsableFont = normalizedFont.CompositeFont.IsValid()
			|| (fontProvider && fontProvider->GetCompositeFont());
		if (!bHasUsableFont)
		{
			normalizedFont.FontObject = fallbackFont.FontObject;
			normalizedFont.FontMaterial = fallbackFont.FontMaterial;
			normalizedFont.CompositeFont = fallbackFont.CompositeFont;
			normalizedFont.OutlineSettings = fallbackFont.OutlineSettings;
			normalizedFont.TypefaceFontName = fallbackFont.TypefaceFontName;
		}
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
		brush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
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
			.SetBackgroundImageHovered(backgroundBrush)
			.SetBackgroundImageFocused(backgroundBrush)
			.SetBackgroundImageReadOnly(backgroundBrush)
			.SetPadding(FMargin(8.0f, 4.0f));
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
			MakeDefaultFont(TEXT("Bold"), 20.0f),
			FLinearColor(0.96f, 0.97f, 1.0f, 1.0f));
	case EWidgetTextStyleRole::Label:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Bold"), 14.0f),
			FLinearColor(0.93f, 0.94f, 0.98f, 1.0f));
	case EWidgetTextStyleRole::Value:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 13.0f),
			FLinearColor(0.88f, 0.90f, 0.95f, 1.0f));
	case EWidgetTextStyleRole::Caption:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 11.0f),
			FLinearColor(0.68f, 0.73f, 0.80f, 1.0f));
	default:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 13.0f),
			FLinearColor(0.88f, 0.90f, 0.95f, 1.0f));
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
