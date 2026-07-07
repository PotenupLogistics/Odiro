#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"
#include "BaseButtonWidget.generated.h"

class UBaseButtonWidget;
class UBorder;
class UImage;
class USizeBox;
class USpacer;
class UTextBlock;
class UTexture2D;
class UWidget;

// Broadcasts input events from one base button view.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBaseButtonWidgetEvent, UBaseButtonWidget*, Button);

// Transparent CommonUI style used so base button visuals are owned only by WBP children.
UCLASS()
class ODIROSIM_API UBaseTransparentButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	// Creates a no-draw, zero-padding button style for the wrapped CommonUI button.
	UBaseTransparentButtonStyle();
};

// CommonUI button view with split base visual catalogs and bindable view API.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// Creates the transparent CommonUI shell used by WBP-owned visuals.
	UBaseButtonWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Resolves the configured base colors through the project default color asset.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	const UBaseWidgetColorCatalog* GetResolvedBaseColors() const;

	// Resolves the configured base sizes through the project default medium-size asset.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	const UBaseWidgetSizeCatalog* GetResolvedBaseSizes() const;

	// Returns a resolved text token from the configured color and size catalogs.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FBaseTextStyleToken ResolveTextStyle(EBaseTextRole role) const;

	// Returns a variant color from the configured color catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FLinearColor ResolveVariantColor(EBaseWidgetVariant variant) const;

	// Returns a state color from the configured color catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FLinearColor ResolveStateColor(EBaseWidgetState state) const;

	// Applies exposed properties to bound WBP-owned controls.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	virtual void SynchronizeBaseProperties();

	// Updates the button text and refreshes the bound WBP text block.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetLabel(FText inLabel);

	// Returns the current button text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FText GetLabel() const { return Label; }

	// Updates the optional icon texture.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetIcon(UTexture2D* inIcon);

	// Returns the optional icon texture.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	UTexture2D* GetIcon() const { return Icon; }

	// Updates the fallback glyph shown only when no icon image resource exists.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetIconGlyphText(FText inIconGlyphText);

	// Returns the fallback glyph text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FText GetIconGlyphText() const { return IconGlyphText; }

	// Updates the icon box and image size in pixels.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetIconSize(float inIconSize);

	// Returns the icon box and image size in pixels.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	float GetIconSize() const { return IconSize; }

	// Updates the non-primary visual variant.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetVariant(EBaseWidgetVariant inVariant);

	// Returns the non-primary visual variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	EBaseWidgetVariant GetVariant() const { return Variant; }

	// Updates whether this button forces the primary variant.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetPrimary(bool bInPrimary);

	// Returns whether this button forces the primary variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	bool IsPrimary() const { return bPrimary; }

	// Updates the optional color catalog override.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetColorsOverride(TSoftObjectPtr<UBaseWidgetColorCatalog> inColorsOverride);

	// Updates the optional size catalog override.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetSizesOverride(TSoftObjectPtr<UBaseWidgetSizeCatalog> inSizesOverride);

	// Updates responsive min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetSizeConstraints(FBaseWidgetSizeConstraints inSizeConstraints);

	// Returns responsive min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FBaseWidgetSizeConstraints GetSizeConstraints() const { return SizeConstraints; }

	// Updates the semantic state used by non-interaction visuals.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the semantic state used by non-interaction visuals.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates the selected state and CommonUI selected state together.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetSelected(bool bInSelected);

	// Returns the selected state cached by this view.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	bool IsBaseSelected() const { return bSelected; }

	// Updates the disabled state and CommonUI enabled state together.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetDisabled(bool bInDisabled);

	// Returns whether this button is disabled by the base component API.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	bool IsDisabled() const { return bDisabled; }

	// Updates horizontal content alignment inside the WBP-owned button surface.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetContentAlign(EBaseHorizontalContentAlign inContentAlign);

	// Returns horizontal content alignment inside the WBP-owned button surface.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	EBaseHorizontalContentAlign GetContentAlign() const { return ContentAlign; }

	// Broadcasts after CommonUI reports a click.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events", meta = (DisplayName = "On Clicked"))
	FBaseButtonWidgetEvent OnBaseClicked;

	// Broadcasts after CommonUI reports a hover start.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events", meta = (DisplayName = "On Hovered"))
	FBaseButtonWidgetEvent OnBaseHovered;

	// Broadcasts after CommonUI reports a hover end.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events", meta = (DisplayName = "On Unhovered"))
	FBaseButtonWidgetEvent OnBaseUnhovered;

	// Broadcasts after CommonUI reports a press.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events", meta = (DisplayName = "On Pressed"))
	FBaseButtonWidgetEvent OnBasePressed;

	// Broadcasts after CommonUI reports a release.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events", meta = (DisplayName = "On Released"))
	FBaseButtonWidgetEvent OnBaseReleased;

protected:
	// Lets Widget Blueprints own visual transitions for CommonUI interaction states.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Events")
	void ReceiveBaseVisualStateChanged(EBaseWidgetState state);

	// Emits a visual-state event without C++ color transitions or Tick.
	void NotifyBaseVisualStateChanged(bool bForce = false);

	// Feeds the rounded surface material its painted size each paint (capture-safe).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Reasserts the internal transparent CommonUI style before CommonUI rebuilds Slate styles.
	void UseTransparentCommonStyle();

	// Keeps Details-panel edits aligned with WBP-owned controls.
	virtual void SynchronizeProperties() override;

	// Reapplies WBP-owned visual state after CommonUI rebuilds the widget tree.
	virtual void OnWidgetRebuilt() override;

	// Keeps preview and runtime visuals aligned with exposed properties.
	virtual void NativePreConstruct() override;

#if WITH_EDITOR
	// Refreshes the designer preview immediately after Details-panel edits.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent) override;
#endif

	// Prevents editor-loaded template CommonUI styles from drawing outside the WBP root.
	virtual void PostLoad() override;

	// Forwards CommonUI click events to the base component API.
	virtual void NativeOnClicked() override;

	// Forwards CommonUI hover events to the base component API.
	virtual void NativeOnHovered() override;

	// Forwards CommonUI unhover events to the base component API.
	virtual void NativeOnUnhovered() override;

	// Forwards CommonUI press events to the base component API.
	virtual void NativeOnPressed() override;

	// Forwards CommonUI release events to the base component API.
	virtual void NativeOnReleased() override;

	// Tracks CommonUI selected state inside the base component API.
	virtual void NativeOnSelected(bool bBroadcast) override;

	// Tracks CommonUI deselected state inside the base component API.
	virtual void NativeOnDeselected(bool bBroadcast) override;

	// Applies one semantic text token to a text block.
	void ApplyTextStyle(UTextBlock* textBlock, EBaseTextRole role) const;

	// Returns whether base sync may apply catalog typography to the label widget.
	virtual bool ShouldApplyLabelTextStyle() const;

	// Applies a semantic tint to an optional border while preserving its WBP-owned brush.
	void ApplyBorderColor(UBorder* border, const FLinearColor& color) const;

	// Caches WBP-authored content padding before icon-only adjustments are applied.
	void CacheAuthoredContentPadding();

	// Removes content padding only for icon-only buttons so tiny controls remain centered.
	void ApplyIconOnlyContentPadding(bool bIconOnly);

	// Returns the variant after applying the bPrimary shortcut.
	EBaseWidgetVariant GetEffectiveVariant() const;

	// Returns the visual state after disabled, interaction, and selected overrides.
	EBaseWidgetState GetEffectiveState() const;

	// Optional color catalog override; empty uses the shared DA_BaseColors asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (DisplayName = "Colors Override", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetColorCatalog> ColorsOverride;

	// Optional size catalog override; empty uses the shared DA_MediumSizes asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (DisplayName = "Sizes Override", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetSizeCatalog> SizesOverride;

	// Button text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Optional icon texture shown before the label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIcon", Setter = "SetIcon", BlueprintGetter = "GetIcon", BlueprintSetter = "SetIcon", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> Icon;

	// Optional fallback glyph shown only when no icon image resource is assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIconGlyphText", Setter = "SetIconGlyphText", BlueprintGetter = "GetIconGlyphText", BlueprintSetter = "SetIconGlyphText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText IconGlyphText;

	// Icon box and image size override in pixels; 0 uses the resolved size catalog value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIconSize", Setter = "SetIconSize", BlueprintGetter = "GetIconSize", BlueprintSetter = "SetIconSize", Category = "UI|Contents", meta = (DisplayName = "Icon Size Override (px)", ClampMin = "0.0", UIMin = "0.0", UIMax = "64.0", ExposeOnSpawn = "true"))
	float IconSize = 0.0f;

	// Horizontal alignment for icon and label content inside the button surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetContentAlign", Setter = "SetContentAlign", BlueprintGetter = "GetContentAlign", BlueprintSetter = "SetContentAlign", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseHorizontalContentAlign ContentAlign = EBaseHorizontalContentAlign::Center;

	// Forces the primary variant for the uncommon single-primary-action case.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsPrimary", Setter = "SetPrimary", BlueprintGetter = "IsPrimary", BlueprintSetter = "SetPrimary", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bPrimary = false;

	// Button semantic variant used when bPrimary is false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetVariant", Setter = "SetVariant", BlueprintGetter = "GetVariant", BlueprintSetter = "SetVariant", Category = "UI|State", meta = (ExposeOnSpawn = "true", EditCondition = "!bPrimary"))
	EBaseWidgetVariant Variant = EBaseWidgetVariant::Secondary;

	// Responsive desired-size constraints applied when RootSize or RootSizeBox is bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSizeConstraints", Setter = "SetSizeConstraints", BlueprintGetter = "GetSizeConstraints", BlueprintSetter = "SetSizeConstraints", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	FBaseWidgetSizeConstraints SizeConstraints;

	// Button semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Selected button state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsBaseSelected", Setter = "SetSelected", BlueprintGetter = "IsBaseSelected", BlueprintSetter = "SetSelected", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bSelected = false;

	// Disabled button state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Interaction state generated by CommonUI hover and press callbacks.
	UPROPERTY(Transient)
	EBaseWidgetState InteractionState = EBaseWidgetState::Default;

	// Last visual state sent to the Widget Blueprint event graph.
	UPROPERTY(Transient)
	EBaseWidgetState LastBroadcastVisualState = EBaseWidgetState::Default;

	// Whether a visual state has been sent since the current widget object was constructed.
	UPROPERTY(Transient)
	bool bHasBroadcastVisualState = false;

	// WBP-authored SurfaceBorder padding restored when the button is not icon-only.
	UPROPERTY(Transient)
	FMargin AuthoredSurfaceContentPadding;

	// Whether the WBP-authored content padding has been captured for this rebuilt tree.
	UPROPERTY(Transient)
	bool bHasAuthoredSurfaceContentPadding = false;

	// Optional outer frame owned by the Widget Blueprint for button stroke states.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BorderFrame;

	// Optional WBP root size wrapper used for responsive min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSize;

	// Optional alternate WBP root size wrapper used for responsive min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox;

	// Surface wrapper owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Label visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Icon visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// Explicit spacing widget between icon and label, preferred over hidden slot padding.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USpacer> IconLabelSpacer;

	// Fixed-size wrapper that prevents optional icon textures from changing button layout.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IconBox;

	// Optional fallback glyph shown by icon-only WBP assets when no texture is assigned.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> IconGlyph;
};
