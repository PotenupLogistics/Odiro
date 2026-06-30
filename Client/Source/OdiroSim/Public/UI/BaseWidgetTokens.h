#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/BaseWidgetTypes.h"
#include "BaseWidgetTokens.generated.h"

// Font and color pair for one semantic base text role.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseTextStyleToken
{
	GENERATED_BODY()

	// Creates a neutral text token used only until a DA resolves real typography.
	FBaseTextStyleToken();

	// Creates a text token from an explicit font and color.
	FBaseTextStyleToken(const FSlateFontInfo& inFont, const FLinearColor& inColor, float inLineHeightPercentage = 1.0f);

	// Font face, size, and outline for this text role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FSlateFontInfo Font;

	// Text color for this role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FLinearColor Color = FLinearColor::White;

	// Line-height multiplier applied by UTextBlock layout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography", meta = (ClampMin = "0.1", UIMin = "0.8", UIMax = "2.0"))
	float LineHeightPercentage = 1.0f;
};

// Font metrics for one semantic base text role inside one size catalog.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseTypographyToken
{
	GENERATED_BODY()

	// Creates a neutral typography token for authored size assets.
	FBaseTypographyToken();

	// Creates a typography token from font metrics.
	FBaseTypographyToken(const FSlateFontInfo& inFont, float inLineHeightPercentage = 1.0f);

	// Font face, size, outline, and letter spacing for this text role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FSlateFontInfo Font;

	// Line-height multiplier applied by UTextBlock layout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography", meta = (ClampMin = "0.1", UIMin = "0.8", UIMax = "2.0"))
	float LineHeightPercentage = 1.0f;
};

// Color tokens for independent base WBP components.
UCLASS(BlueprintType)
class ODIROSIM_API UBaseWidgetColorCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Initializes Odiro's dense Unreal Editor-inspired default color tokens.
	UBaseWidgetColorCatalog();

	// Default project asset location for base widget colors.
	static TSoftObjectPtr<UBaseWidgetColorCatalog> MakeDefaultCatalogReference();

	// Resolves a color catalog reference with project-default asset fallback only.
	static const UBaseWidgetColorCatalog* ResolveCatalog(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& catalogReference);

	// Returns the primary color for a semantic variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Colors")
	FLinearColor GetVariantColor(EBaseWidgetVariant variant) const;

	// Returns the primary color for a semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Colors")
	FLinearColor GetStateColor(EBaseWidgetState state) const;

	// Returns the text color for one semantic typography role.
	UFUNCTION(BlueprintPure, Category = "UI|Base Colors")
	FLinearColor GetTextColor(EBaseTextRole role) const;

	// App-level component gallery background.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor BackgroundColor;

	// Default component surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor SurfaceColor;

	// Raised component surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor SurfaceRaisedColor;

	// Default framed component line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor BorderColor;

	// Default internal divider line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor DividerColor;

	// Primary action and selected-state color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor PrimaryColor;

	// Secondary action color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor SecondaryColor;

	// Success semantic color for badges and dashboard status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor SuccessColor;

	// Warning semantic color for badges and dashboard status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor WarningColor;

	// Danger semantic color for destructive or failed states.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor DangerColor;

	// Informational semantic color for neutral analysis states.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Compatibility")
	FLinearColor InfoColor;

	// Disabled state color used for inactive labels and icon tint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|State")
	FLinearColor DisabledColor;

	// Hover surface color used by interactive controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|State")
	FLinearColor HoverColor;

	// Pressed surface color used by interactive controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|State")
	FLinearColor PressedColor;

	// Input well and recessed field surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceWellColor;

	// Title bar, tab strip, and app chrome surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceChromeColor;

	// Main app canvas surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceAppColor;

	// Card, panel, outliner, and details panel surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfacePanelColor;

	// Property and section header row surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceRowHeadColor;

	// Card and panel header strip surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceHeaderColor;

	// Neutral button and field control surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceControlColor;

	// Neutral control hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceControlHoverColor;

	// Neutral control pressed surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceControlActiveColor;

	// List and table row hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceHoverColor;

	// Subtle title-bar hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Surfaces")
	FLinearColor SurfaceHoverSoftColor;

	// Hard chrome separator line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineBlackColor;

	// Subtle internal panel row separator line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineSubtleColor;

	// Quiet card internal divider line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineQuietColor;

	// Section divider and header underline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineDividerColor;

	// Field and card outline line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineFieldColor;

	// Button and segment inset line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineInsetColor;

	// Field hover outline line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Lines")
	FLinearColor LineFieldHoverColor;

	// Unreal blue accent for selected state and the single primary action.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Accent")
	FLinearColor AccentColor;

	// Accent hover color for primary controls and focused fields.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Accent")
	FLinearColor AccentHoverColor;

	// Accent active color for pressed primary controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Accent")
	FLinearColor AccentActiveColor;

	// Focus outline color for keyboard and field focus.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Accent")
	FLinearColor AccentFocusColor;

	// Strongest text color for selected labels and large stat numbers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextStrongColor;

	// Bright text color for section titles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextBrightColor;

	// Heading text color for property section headers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextHeadingColor;

	// Primary text color for body copy and control labels.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextPrimaryColor;

	// Secondary text color for metadata and secondary values.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextSecondaryColor;

	// Field caption and property label text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextLabelColor;

	// Helper and placeholder text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextMutedColor;

	// Disabled, hint, and chevron text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextFaintColor;

	// Viewport watermark and ghosted text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Text")
	FLinearColor TextGhostColor;

	// Completed or successful status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor StatusSuccessColor;

	// In-progress status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor StatusRunningColor;

	// Warning status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor StatusWarnColor;

	// Failure, collision, and high-severity status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor StatusDangerColor;

	// Low-severity information status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor StatusInfoColor;

	// Window close hover danger color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Status")
	FLinearColor CloseDangerColor;

	// X-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Axis")
	FLinearColor AxisXColor;

	// Y-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Axis")
	FLinearColor AxisYColor;

	// Z-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Axis")
	FLinearColor AxisZColor;

	// World entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntityWorldColor;

	// Folder entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntityFolderColor;

	// Light entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntityLightColor;

	// Fog entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntityFogColor;

	// Sky entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntitySkyColor;

	// Blueprint entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Entity")
	FLinearColor EntityBlueprintColor;

	// Scenario tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Tabs")
	FLinearColor TabScenarioColor;

	// Experiment tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Tabs")
	FLinearColor TabExperimentColor;

	// Settings tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Tabs")
	FLinearColor TabSettingsColor;

	// Detail tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Colors|Tabs")
	FLinearColor TabDetailColor;
};

// Size and typography tokens for independent base WBP components.
UCLASS(BlueprintType)
class ODIROSIM_API UBaseWidgetSizeCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Initializes neutral class defaults; authored DA assets own production values.
	UBaseWidgetSizeCatalog();

	// Default project asset location for medium base widget sizes.
	static TSoftObjectPtr<UBaseWidgetSizeCatalog> MakeDefaultCatalogReference();

	// Project asset location for one deprecated Small/Medium/Large size preset.
	static TSoftObjectPtr<UBaseWidgetSizeCatalog> MakePresetCatalogReference(EBaseWidgetSize size);

	// Resolves a size catalog reference with project-default asset fallback only.
	static const UBaseWidgetSizeCatalog* ResolveCatalog(
		const TSoftObjectPtr<UBaseWidgetSizeCatalog>& catalogReference);

	// Returns typography metrics configured for one role.
	UFUNCTION(BlueprintPure, Category = "UI|Base Sizes")
	FBaseTypographyToken GetTypography(EBaseTextRole role) const;

	// Title typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FBaseTypographyToken TitleText;

	// Label typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FBaseTypographyToken LabelText;

	// Body typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FBaseTypographyToken BodyText;

	// Caption typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FBaseTypographyToken CaptionText;

	// Dashboard value typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Typography")
	FBaseTypographyToken ValueText;

	// Smallest spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space1 = 0.0f;

	// Compact spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space2 = 0.0f;

	// Dense spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space3 = 0.0f;

	// Default spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space4 = 0.0f;

	// Spacing unit for compact grouped controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space5 = 0.0f;

	// Spacing unit for component padding.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space6 = 0.0f;

	// Spacing unit for section gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space8 = 0.0f;

	// Spacing unit for major section gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space10 = 0.0f;

	// Spacing unit for wide groups.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space12 = 0.0f;

	// Spacing unit for broad layout gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space16 = 0.0f;

	// Spacing unit for largest gallery gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing", meta = (ClampMin = "0.0"))
	float Space20 = 0.0f;

	// Compact spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingSmall = 0.0f;

	// Default spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingMedium = 0.0f;

	// Roomy spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingLarge = 0.0f;

	// Standard control height for buttons, inputs, and selects.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float ControlHeight = 0.0f;

	// Compact control height for row actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float ControlHeightSmall = 0.0f;

	// Vector and numeric field height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float FieldHeight = 0.0f;

	// Outliner row height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float RowHeight = 0.0f;

	// Property header row height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float PropertyRowHeight = 0.0f;

	// App title bar height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float TitleBarHeight = 0.0f;

	// Document tab bar height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float TabBarHeight = 0.0f;

	// Panel header strip height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float PanelHeaderHeight = 0.0f;

	// Default square icon size for components that expose an icon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Sizing", meta = (ClampMin = "0.0"))
	float IconSize = 0.0f;

	// Sharp corner radius for rows, headers, segments, and cells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius", meta = (ClampMin = "0.0"))
	float RadiusNone = 0.0f;

	// Default radius for cards, buttons, badges, and wells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;

	// Pill radius for circular replay or icon actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius", meta = (ClampMin = "0.0"))
	float RadiusPill = 0.0f;

	// Compact corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusSmall = 0.0f;

	// Default corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusMedium = 0.0f;

	// Large corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusLarge = 0.0f;

	// Standard hairline border width.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Line Metrics", meta = (ClampMin = "0.0"))
	float BorderWidth = 0.0f;

	// Axis strip width for vector field bars.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Line Metrics", meta = (ClampMin = "0.0"))
	float AxisBarWidth = 0.0f;

	// Low elevation opacity used by subtle shadows.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ElevationLow = 0.0f;

	// Medium elevation opacity used by floating cards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Sizes|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ElevationMedium = 0.0f;
};
