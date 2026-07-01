#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"
#include "BaseWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;

// Base view for independent non-button base widget components.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Resolves the configured base colors through the project default color asset.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	const UBaseWidgetColorCatalog* GetResolvedBaseColors() const;

	// Resolves the configured base sizes through the project default medium-size asset.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	const UBaseWidgetSizeCatalog* GetResolvedBaseSizes() const;

	// Returns a resolved text token from the configured color and size catalogs.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FBaseTextStyleToken ResolveTextStyle(EBaseTextRole role) const;

	// Returns a variant color from the configured color catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FLinearColor ResolveVariantColor(EBaseWidgetVariant variant) const;

	// Returns a state color from the configured color catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FLinearColor ResolveStateColor(EBaseWidgetState state) const;

	// Updates the optional color catalog override.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	void SetColorsOverride(TSoftObjectPtr<UBaseWidgetColorCatalog> inColorsOverride);

	// Updates the optional size catalog override.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	void SetSizesOverride(TSoftObjectPtr<UBaseWidgetSizeCatalog> inSizesOverride);

	// Updates responsive min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	void SetSizeConstraints(FBaseWidgetSizeConstraints inSizeConstraints);

	// Returns responsive min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FBaseWidgetSizeConstraints GetSizeConstraints() const { return SizeConstraints; }

	// Applies exposed properties to bound WBP-owned controls.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	virtual void SynchronizeBaseProperties();

protected:
	// Lets Widget Blueprints own visual transitions for logical state changes.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Events")
	void ReceiveBaseVisualStateChanged(EBaseWidgetState state);

	// Emits a visual-state event without relying on Tick or C++ color transitions.
	void NotifyBaseVisualStateChanged(EBaseWidgetState state, bool bForce = false);

	// Reapplies WBP-owned visual state after the widget tree is rebuilt.
	virtual void OnWidgetRebuilt() override;

	// Applies bound-widget state after runtime construction.
	virtual void NativeConstruct() override;

	// Keeps preview and runtime visuals aligned with exposed properties.
	virtual void NativePreConstruct() override;

	// Keeps Details-panel edits aligned with WBP-owned controls.
	virtual void SynchronizeProperties() override;

	// Applies one semantic text token to a text block.
	void ApplyTextStyle(UTextBlock* textBlock, EBaseTextRole role) const;

	// Applies a semantic tint to an optional border while preserving its WBP-owned brush.
	void ApplyBorderColor(UBorder* border, const FLinearColor& color) const;

	// Optional color catalog override; empty uses the shared DA_BaseColors asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (DisplayName = "Colors Override", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetColorCatalog> ColorsOverride;

	// Optional size catalog override; empty uses the shared DA_MediumSizes asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (DisplayName = "Sizes Override", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetSizeCatalog> SizesOverride;

	// Responsive desired-size constraints applied when RootSize or RootSizeBox is bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSizeConstraints", Setter = "SetSizeConstraints", BlueprintGetter = "GetSizeConstraints", BlueprintSetter = "SetSizeConstraints", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	FBaseWidgetSizeConstraints SizeConstraints;

	// Optional outer frame owned by the Widget Blueprint for stroke or selected-state highlights.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BorderFrame;

	// Optional WBP root size wrapper used for responsive min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSize;

	// Optional alternate WBP root size wrapper used for responsive min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox;

	// Last visual state sent to the Widget Blueprint event graph.
	UPROPERTY(Transient)
	EBaseWidgetState LastBroadcastVisualState = EBaseWidgetState::Default;

	// Whether a visual state has been sent since the current widget object was constructed.
	UPROPERTY(Transient)
	bool bHasBroadcastVisualState = false;
};
