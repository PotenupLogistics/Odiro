#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"
#include "BaseButtonWidget.generated.h"

class UBaseButtonWidget;
class UBorder;
class UImage;
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

// CommonUI button view with base-token styling and bindable view API.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// Creates the transparent CommonUI shell used by WBP-owned visuals.
	UBaseButtonWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Resolves the configured base tokens, falling back to the project default asset or built-in defaults.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	const UBaseWidgetTokenCatalog* GetResolvedBaseTokens() const;

	// Returns a text token from the configured token catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FBaseTextStyleToken ResolveTextStyle(EBaseTextRole role) const;

	// Returns a variant color from the configured token catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	FLinearColor ResolveVariantColor(EBaseWidgetVariant variant) const;

	// Returns a state color from the configured token catalog.
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

	// Updates the size hint used by the WBP layout.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Button")
	void SetBaseSize(EBaseWidgetSize inSize);

	// Returns the size hint used by the WBP layout.
	UFUNCTION(BlueprintPure, Category = "UI|Base Button")
	EBaseWidgetSize GetBaseSize() const { return Size; }

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

	// Broadcasts after CommonUI reports a click.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Button|Events", meta = (DisplayName = "On Clicked"))
	FBaseButtonWidgetEvent OnBaseClicked;

	// Broadcasts after CommonUI reports a hover start.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Button|Events", meta = (DisplayName = "On Hovered"))
	FBaseButtonWidgetEvent OnBaseHovered;

	// Broadcasts after CommonUI reports a hover end.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Button|Events", meta = (DisplayName = "On Unhovered"))
	FBaseButtonWidgetEvent OnBaseUnhovered;

	// Broadcasts after CommonUI reports a press.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Button|Events", meta = (DisplayName = "On Pressed"))
	FBaseButtonWidgetEvent OnBasePressed;

	// Broadcasts after CommonUI reports a release.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Button|Events", meta = (DisplayName = "On Released"))
	FBaseButtonWidgetEvent OnBaseReleased;

protected:
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

	// Applies a semantic tint to an optional border while preserving its WBP-owned brush.
	void ApplyBorderColor(UBorder* border, const FLinearColor& color) const;

	// Returns the variant after applying the bPrimary shortcut.
	EBaseWidgetVariant GetEffectiveVariant() const;

	// Returns the visual state after disabled, interaction, and selected overrides.
	EBaseWidgetState GetEffectiveState() const;

	// Optional token catalog override; empty uses the shared DA_BaseTokens asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Button", meta = (DisplayName = "Base Token Overrides", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetTokenCatalog> BaseTokens;

	// Button text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Optional icon texture shown before the label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIcon", Setter = "SetIcon", BlueprintGetter = "GetIcon", BlueprintSetter = "SetIcon", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> Icon;

	// Optional fallback glyph shown only when no icon image resource is assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIconGlyphText", Setter = "SetIconGlyphText", BlueprintGetter = "GetIconGlyphText", BlueprintSetter = "SetIconGlyphText", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	FText IconGlyphText;

	// Icon box and image size in pixels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIconSize", Setter = "SetIconSize", BlueprintGetter = "GetIconSize", BlueprintSetter = "SetIconSize", Category = "UI|Base Button", meta = (DisplayName = "Icon Size (px)", ClampMin = "1.0", UIMin = "8.0", UIMax = "64.0", ExposeOnSpawn = "true"))
	float IconSize = 24.0f;

	// Forces the primary variant for the uncommon single-primary-action case.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsPrimary", Setter = "SetPrimary", BlueprintGetter = "IsPrimary", BlueprintSetter = "SetPrimary", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	bool bPrimary = false;

	// Button semantic variant used when bPrimary is false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetVariant", Setter = "SetVariant", BlueprintGetter = "GetVariant", BlueprintSetter = "SetVariant", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true", EditCondition = "!bPrimary"))
	EBaseWidgetVariant Variant = EBaseWidgetVariant::Secondary;

	// Button size hint used by WBP variants.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseSize", Setter = "SetBaseSize", BlueprintGetter = "GetBaseSize", BlueprintSetter = "SetBaseSize", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetSize Size = EBaseWidgetSize::Medium;

	// Button semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Selected button state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsBaseSelected", Setter = "SetSelected", BlueprintGetter = "IsBaseSelected", BlueprintSetter = "SetSelected", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	bool bSelected = false;

	// Disabled button state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Button", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Interaction state generated by CommonUI hover and press callbacks.
	UPROPERTY(Transient)
	EBaseWidgetState InteractionState = EBaseWidgetState::Default;

	// Optional outer frame owned by the Widget Blueprint for button stroke states.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BorderFrame;

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
