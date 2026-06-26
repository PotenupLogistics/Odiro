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

	// Creates the default body text token.
	FBaseTextStyleToken();

	// Creates a text token from an explicit font and color.
	FBaseTextStyleToken(const FSlateFontInfo& inFont, const FLinearColor& inColor);

	// Font face, size, and outline for this text role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FSlateFontInfo Font;

	// Text color for this role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FLinearColor Color = FLinearColor::White;
};

// DataAsset storing reusable visual tokens for independent base WBP components.
UCLASS(BlueprintType)
class ODIROSIM_API UBaseWidgetTokenCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Initializes Odiro's dense Unreal Editor-inspired default tokens.
	UBaseWidgetTokenCatalog();

	// Default project asset location for base widget tokens.
	static TSoftObjectPtr<UBaseWidgetTokenCatalog> MakeDefaultCatalogReference();

	// Built-in fallback typography token for one role.
	static FBaseTextStyleToken MakeDefaultTextStyle(EBaseTextRole role);

	// Resolves a token catalog reference with project-default fallback.
	static const UBaseWidgetTokenCatalog* ResolveCatalog(
		const TSoftObjectPtr<UBaseWidgetTokenCatalog>& catalogReference);

	// Returns the typography token configured for one role.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tokens")
	FBaseTextStyleToken GetTextStyle(EBaseTextRole role) const;

	// Returns the primary color for a semantic variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tokens")
	FLinearColor GetVariantColor(EBaseWidgetVariant variant) const;

	// Returns the primary color for a semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tokens")
	FLinearColor GetStateColor(EBaseWidgetState state) const;

	// App-level component gallery background, mapped to SurfaceAppColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor BackgroundColor;

	// Default component surface, mapped to SurfacePanelColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor SurfaceColor;

	// Raised component surface, mapped to SurfaceHeaderColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor SurfaceRaisedColor;

	// Default framed component line, mapped to LineFieldColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor BorderColor;

	// Default internal divider line, mapped to LineDividerColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor DividerColor;

	// Primary action and selected-state color, mapped to AccentColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor PrimaryColor;

	// Secondary action color, mapped to SurfaceControlColor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor SecondaryColor;

	// Success semantic color for badges and dashboard status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor SuccessColor;

	// Warning semantic color for badges and dashboard status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor WarningColor;

	// Danger semantic color for destructive or failed states.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor DangerColor;

	// Informational semantic color for neutral analysis states.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Compatibility")
	FLinearColor InfoColor;

	// Disabled state color used for inactive labels and icon tint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|State")
	FLinearColor DisabledColor;

	// Hover surface color used by interactive controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|State")
	FLinearColor HoverColor;

	// Pressed surface color used by interactive controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|State")
	FLinearColor PressedColor;

	// Input well and recessed field surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceWellColor;

	// Title bar, tab strip, and app chrome surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceChromeColor;

	// Main app canvas surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceAppColor;

	// Card, panel, outliner, and details panel surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfacePanelColor;

	// Property and section header row surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceRowHeadColor;

	// Card and panel header strip surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceHeaderColor;

	// Neutral button and field control surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceControlColor;

	// Neutral control hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceControlHoverColor;

	// Neutral control pressed surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceControlActiveColor;

	// List and table row hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceHoverColor;

	// Subtle title-bar hover surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Surfaces")
	FLinearColor SurfaceHoverSoftColor;

	// Hard chrome separator line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineBlackColor;

	// Subtle internal panel row separator line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineSubtleColor;

	// Quiet card internal divider line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineQuietColor;

	// Section divider and header underline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineDividerColor;

	// Field and card outline line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineFieldColor;

	// Button and segment inset line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineInsetColor;

	// Field hover outline line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Lines")
	FLinearColor LineFieldHoverColor;

	// Unreal blue accent for selected state and the single primary action.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Accent")
	FLinearColor AccentColor;

	// Accent hover color for primary controls and focused fields.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Accent")
	FLinearColor AccentHoverColor;

	// Accent active color for pressed primary controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Accent")
	FLinearColor AccentActiveColor;

	// Focus outline color for keyboard and field focus.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Accent")
	FLinearColor AccentFocusColor;

	// Strongest text color for selected labels and large stat numbers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextStrongColor;

	// Bright text color for section titles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextBrightColor;

	// Heading text color for property section headers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextHeadingColor;

	// Primary text color for body copy and control labels.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextPrimaryColor;

	// Secondary text color for metadata and secondary values.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextSecondaryColor;

	// Field caption and property label text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextLabelColor;

	// Helper and placeholder text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextMutedColor;

	// Disabled, hint, and chevron text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextFaintColor;

	// Viewport watermark and ghosted text color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Text")
	FLinearColor TextGhostColor;

	// Completed or successful status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor StatusSuccessColor;

	// In-progress status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor StatusRunningColor;

	// Warning status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor StatusWarnColor;

	// Failure, collision, and high-severity status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor StatusDangerColor;

	// Low-severity information status color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor StatusInfoColor;

	// Window close hover danger color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Status")
	FLinearColor CloseDangerColor;

	// X-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Axis")
	FLinearColor AxisXColor;

	// Y-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Axis")
	FLinearColor AxisYColor;

	// Z-axis vector field strip color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Axis")
	FLinearColor AxisZColor;

	// World entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntityWorldColor;

	// Folder entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntityFolderColor;

	// Light entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntityLightColor;

	// Fog entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntityFogColor;

	// Sky entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntitySkyColor;

	// Blueprint entity swatch color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Entity")
	FLinearColor EntityBlueprintColor;

	// Scenario tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Tabs")
	FLinearColor TabScenarioColor;

	// Experiment tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Tabs")
	FLinearColor TabExperimentColor;

	// Settings tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Tabs")
	FLinearColor TabSettingsColor;

	// Detail tab type indicator color.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Color|Tabs")
	FLinearColor TabDetailColor;

	// Title typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FBaseTextStyleToken TitleText;

	// Label typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FBaseTextStyleToken LabelText;

	// Body typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FBaseTextStyleToken BodyText;

	// Caption typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FBaseTextStyleToken CaptionText;

	// Dashboard value typography token.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Typography")
	FBaseTextStyleToken ValueText;

	// Smallest 2px spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space1 = 2.5f;

	// Compact 4px spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space2 = 5.0f;

	// Dense 6px spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space3 = 7.5f;

	// Default 8px spacing unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space4 = 10.0f;

	// 10px spacing unit for compact grouped controls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space5 = 12.5f;

	// 12px spacing unit for component padding.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space6 = 15.0f;

	// 16px spacing unit for section gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space8 = 20.0f;

	// 20px spacing unit for major section gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space10 = 25.0f;

	// 24px spacing unit for wide groups.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space12 = 30.0f;

	// 36px spacing unit for broad layout gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space16 = 45.0f;

	// 40px spacing unit for largest gallery gaps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing", meta = (ClampMin = "0.0"))
	float Space20 = 50.0f;

	// Compact spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingSmall = 5.0f;

	// Default spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingMedium = 10.0f;

	// Roomy spacing alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Spacing|Compatibility", meta = (ClampMin = "0.0"))
	float SpacingLarge = 15.0f;

	// Standard control height for buttons, inputs, and selects.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float ControlHeight = 38.0f;

	// Compact control height for row actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float ControlHeightSmall = 35.0f;

	// Vector and numeric field height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float FieldHeight = 29.0f;

	// Outliner row height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float RowHeight = 30.0f;

	// Property header row height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float PropertyRowHeight = 33.0f;

	// App title bar height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float TitleBarHeight = 40.0f;

	// Document tab bar height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float TabBarHeight = 45.0f;

	// Panel header strip height.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Sizing", meta = (ClampMin = "0.0"))
	float PanelHeaderHeight = 38.0f;

	// Sharp corner radius for rows, headers, segments, and cells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius", meta = (ClampMin = "0.0"))
	float RadiusNone = 0.0f;

	// Default radius for cards, buttons, badges, and wells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius", meta = (ClampMin = "0.0"))
	float Radius = 4.0f;

	// Pill radius for circular replay or icon actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius", meta = (ClampMin = "0.0"))
	float RadiusPill = 999.0f;

	// Compact corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusSmall = 0.0f;

	// Default corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusMedium = 4.0f;

	// Large corner radius alias used by existing base widgets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Radius|Compatibility", meta = (ClampMin = "0.0"))
	float RadiusLarge = 4.0f;

	// Standard hairline border width.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Line Metrics", meta = (ClampMin = "0.0"))
	float BorderWidth = 1.0f;

	// Axis strip width for vector field bars.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Line Metrics", meta = (ClampMin = "0.0"))
	float AxisBarWidth = 4.0f;

	// Low elevation opacity used by subtle shadows.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ElevationLow = 0.07f;

	// Medium elevation opacity used by floating cards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Base Tokens|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ElevationMedium = 0.10f;
};
