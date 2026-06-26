#include "UI/BaseWidgetTokens.h"

#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath BaseTokensPath(
		TEXT("/Game/Widgets/Common/DA_BaseTokens.DA_BaseTokens"));
	const TCHAR* RegularFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-4Regular_Font.Freesentation-4Regular_Font");
	const TCHAR* MediumFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-5Medium_Font.Freesentation-5Medium_Font");
	const TCHAR* BoldFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-7Bold_Font.Freesentation-7Bold_Font");

	// Converts documented sRGB hex tokens into Slate brush colors.
	// Slate applies UI vertex colors without an sRGB->linear decode, so the
	// authored sRGB byte fractions map straight through (ReinterpretAsLinear).
	// Using FromSRGBColor here double-darkens every surface and the accent.
	FLinearColor MakeBaseColor(const TCHAR* hex, const float alpha = 1.0f)
	{
		FLinearColor color = FColor::FromHex(hex).ReinterpretAsLinear();
		color.A = alpha;
		return color;
	}

	// Loads the Freesentation font asset for one semantic weight.
	FSlateFontInfo MakeBaseFont(const FName typefaceName, const float size)
	{
		const TCHAR* fontPath = RegularFontPath;
		if (typefaceName == FName(TEXT("Bold")))
		{
			fontPath = BoldFontPath;
		}
		else if (typefaceName == FName(TEXT("Medium")))
		{
			fontPath = MediumFontPath;
		}

		UObject* fontObject = LoadObject<UObject>(nullptr, fontPath);
		FSlateFontInfo fontInfo(fontObject, size, typefaceName);
		fontInfo.Size = size;
		return fontInfo;
	}
}

FBaseTextStyleToken::FBaseTextStyleToken()
	: Font(MakeBaseFont(TEXT("Regular"), 16.0f))
	, Color(MakeBaseColor(TEXT("CFCFCF")))
{
}

FBaseTextStyleToken::FBaseTextStyleToken(const FSlateFontInfo& inFont, const FLinearColor& inColor)
	: Font(inFont)
	, Color(inColor)
{
}

UBaseWidgetTokenCatalog::UBaseWidgetTokenCatalog()
	: BackgroundColor(MakeBaseColor(TEXT("1A1A1A")))
	, SurfaceColor(MakeBaseColor(TEXT("242424")))
	, SurfaceRaisedColor(MakeBaseColor(TEXT("2E2E2E")))
	, BorderColor(MakeBaseColor(TEXT("3A3A3A")))
	, DividerColor(MakeBaseColor(TEXT("333333")))
	, PrimaryColor(MakeBaseColor(TEXT("0070E0")))
	, SecondaryColor(MakeBaseColor(TEXT("383838")))
	, SuccessColor(MakeBaseColor(TEXT("4CAF50")))
	, WarningColor(MakeBaseColor(TEXT("E0A030")))
	, DangerColor(MakeBaseColor(TEXT("E5534B")))
	, InfoColor(MakeBaseColor(TEXT("4A9FF5")))
	, DisabledColor(MakeBaseColor(TEXT("5E5E5E")))
	, HoverColor(MakeBaseColor(TEXT("454545")))
	, PressedColor(MakeBaseColor(TEXT("525252")))
	, SurfaceWellColor(MakeBaseColor(TEXT("0F0F0F")))
	, SurfaceChromeColor(MakeBaseColor(TEXT("151515")))
	, SurfaceAppColor(MakeBaseColor(TEXT("1A1A1A")))
	, SurfacePanelColor(MakeBaseColor(TEXT("242424")))
	, SurfaceRowHeadColor(MakeBaseColor(TEXT("2A2A2A")))
	, SurfaceHeaderColor(MakeBaseColor(TEXT("2E2E2E")))
	, SurfaceControlColor(MakeBaseColor(TEXT("383838")))
	, SurfaceControlHoverColor(MakeBaseColor(TEXT("454545")))
	, SurfaceControlActiveColor(MakeBaseColor(TEXT("525252")))
	, SurfaceHoverColor(MakeBaseColor(TEXT("2F2F2F")))
	, SurfaceHoverSoftColor(MakeBaseColor(TEXT("333333")))
	, LineBlackColor(MakeBaseColor(TEXT("0E0E0E")))
	, LineSubtleColor(MakeBaseColor(TEXT("262626")))
	, LineQuietColor(MakeBaseColor(TEXT("303030")))
	, LineDividerColor(MakeBaseColor(TEXT("333333")))
	, LineFieldColor(MakeBaseColor(TEXT("3A3A3A")))
	, LineInsetColor(MakeBaseColor(TEXT("4A4A4A")))
	, LineFieldHoverColor(MakeBaseColor(TEXT("5E5E5E")))
	, AccentColor(MakeBaseColor(TEXT("0070E0")))
	, AccentHoverColor(MakeBaseColor(TEXT("2589F5")))
	, AccentActiveColor(MakeBaseColor(TEXT("0059C2")))
	, AccentFocusColor(MakeBaseColor(TEXT("2589F5")))
	, TextStrongColor(MakeBaseColor(TEXT("FFFFFF")))
	, TextBrightColor(MakeBaseColor(TEXT("F2F2F2")))
	, TextHeadingColor(MakeBaseColor(TEXT("E2E2E2")))
	, TextPrimaryColor(MakeBaseColor(TEXT("CFCFCF")))
	, TextSecondaryColor(MakeBaseColor(TEXT("A8A8A8")))
	, TextLabelColor(MakeBaseColor(TEXT("8C8C8C")))
	, TextMutedColor(MakeBaseColor(TEXT("7A7A7A")))
	, TextFaintColor(MakeBaseColor(TEXT("5E5E5E")))
	, TextGhostColor(MakeBaseColor(TEXT("3A3A3A")))
	, StatusSuccessColor(MakeBaseColor(TEXT("4CAF50")))
	, StatusRunningColor(MakeBaseColor(TEXT("E0A030")))
	, StatusWarnColor(MakeBaseColor(TEXT("E0A030")))
	, StatusDangerColor(MakeBaseColor(TEXT("E5534B")))
	, StatusInfoColor(MakeBaseColor(TEXT("4A9FF5")))
	, CloseDangerColor(MakeBaseColor(TEXT("E5534B")))
	, AxisXColor(MakeBaseColor(TEXT("D9534F")))
	, AxisYColor(MakeBaseColor(TEXT("7CB342")))
	, AxisZColor(MakeBaseColor(TEXT("4A90D9")))
	, EntityWorldColor(MakeBaseColor(TEXT("8B94A0")))
	, EntityFolderColor(MakeBaseColor(TEXT("D2A23A")))
	, EntityLightColor(MakeBaseColor(TEXT("CBB24A")))
	, EntityFogColor(MakeBaseColor(TEXT("9AA0AA")))
	, EntitySkyColor(MakeBaseColor(TEXT("5F97CB")))
	, EntityBlueprintColor(MakeBaseColor(TEXT("4A90D9")))
	, TabScenarioColor(MakeBaseColor(TEXT("D2A23A")))
	, TabExperimentColor(MakeBaseColor(TEXT("4A9FF5")))
	, TabSettingsColor(MakeBaseColor(TEXT("9A9A9A")))
	, TabDetailColor(MakeBaseColor(TEXT("5CBA60")))
	, TitleText(MakeDefaultTextStyle(EBaseTextRole::Title))
	, LabelText(MakeDefaultTextStyle(EBaseTextRole::Label))
	, BodyText(MakeDefaultTextStyle(EBaseTextRole::Body))
	, CaptionText(MakeDefaultTextStyle(EBaseTextRole::Caption))
	, ValueText(MakeDefaultTextStyle(EBaseTextRole::Value))
{
}

TSoftObjectPtr<UBaseWidgetTokenCatalog> UBaseWidgetTokenCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UBaseWidgetTokenCatalog>(BaseTokensPath);
}

FBaseTextStyleToken UBaseWidgetTokenCatalog::MakeDefaultTextStyle(const EBaseTextRole role)
{
	switch (role)
	{
	case EBaseTextRole::Title:
		return FBaseTextStyleToken(MakeBaseFont(TEXT("Bold"), 17.5f), MakeBaseColor(TEXT("F2F2F2")));
	case EBaseTextRole::Label:
		return FBaseTextStyleToken(MakeBaseFont(TEXT("Medium"), 15.0f), MakeBaseColor(TEXT("CFCFCF")));
	case EBaseTextRole::Caption:
		return FBaseTextStyleToken(MakeBaseFont(TEXT("Regular"), 14.0f), MakeBaseColor(TEXT("A8A8A8")));
	case EBaseTextRole::Value:
		return FBaseTextStyleToken(MakeBaseFont(TEXT("Bold"), 35.0f), MakeBaseColor(TEXT("FFFFFF")));
	case EBaseTextRole::Body:
	default:
		return FBaseTextStyleToken(MakeBaseFont(TEXT("Regular"), 16.0f), MakeBaseColor(TEXT("CFCFCF")));
	}
}

const UBaseWidgetTokenCatalog* UBaseWidgetTokenCatalog::ResolveCatalog(
	const TSoftObjectPtr<UBaseWidgetTokenCatalog>& catalogReference)
{
	if (const UBaseWidgetTokenCatalog* catalog = catalogReference.LoadSynchronous())
	{
		return catalog;
	}

	TSoftObjectPtr<UBaseWidgetTokenCatalog> defaultCatalog = MakeDefaultCatalogReference();
	return defaultCatalog.LoadSynchronous();
}

FBaseTextStyleToken UBaseWidgetTokenCatalog::GetTextStyle(const EBaseTextRole role) const
{
	switch (role)
	{
	case EBaseTextRole::Title:
		return TitleText;
	case EBaseTextRole::Label:
		return LabelText;
	case EBaseTextRole::Caption:
		return CaptionText;
	case EBaseTextRole::Value:
		return ValueText;
	case EBaseTextRole::Body:
	default:
		return BodyText;
	}
}

FLinearColor UBaseWidgetTokenCatalog::GetVariantColor(const EBaseWidgetVariant variant) const
{
	switch (variant)
	{
	case EBaseWidgetVariant::Primary:
		return AccentColor;
	case EBaseWidgetVariant::Secondary:
		return SurfaceControlColor;
	case EBaseWidgetVariant::Success:
		return StatusSuccessColor;
	case EBaseWidgetVariant::Warning:
		return StatusWarnColor;
	case EBaseWidgetVariant::Danger:
		return StatusDangerColor;
	case EBaseWidgetVariant::Info:
		return StatusInfoColor;
	case EBaseWidgetVariant::Ghost:
		return FLinearColor::Transparent;
	case EBaseWidgetVariant::Neutral:
	default:
		return SurfacePanelColor;
	}
}

FLinearColor UBaseWidgetTokenCatalog::GetStateColor(const EBaseWidgetState state) const
{
	switch (state)
	{
	case EBaseWidgetState::Hovered:
		return SurfaceControlHoverColor;
	case EBaseWidgetState::Pressed:
		return SurfaceControlActiveColor;
	case EBaseWidgetState::Selected:
		return AccentColor;
	case EBaseWidgetState::Disabled:
		return TextFaintColor;
	case EBaseWidgetState::Success:
		return StatusSuccessColor;
	case EBaseWidgetState::Warning:
		return StatusWarnColor;
	case EBaseWidgetState::Error:
		return StatusDangerColor;
	case EBaseWidgetState::Loading:
		return StatusRunningColor;
	case EBaseWidgetState::Default:
	default:
		return SurfacePanelColor;
	}
}
