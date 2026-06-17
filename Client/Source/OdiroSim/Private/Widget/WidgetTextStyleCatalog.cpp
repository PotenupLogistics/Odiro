#include "Widget/WidgetTextStyleCatalog.h"

#include "Styling/CoreStyle.h"
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
		if (!normalizedFont.FontObject && !normalizedFont.CompositeFont.IsValid())
		{
			normalizedFont.FontObject = fallbackFont.FontObject;
			normalizedFont.CompositeFont = fallbackFont.CompositeFont;
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
	default:
		return FWidgetTextStyle(
			MakeDefaultFont(TEXT("Regular"), 13.0f),
			FLinearColor(0.88f, 0.90f, 0.95f, 1.0f));
	}
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
	default:
		return NormalizeStyle(Value, role);
	}
}
