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
	// Resolves the configured base tokens, falling back to the project default asset or built-in defaults.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	const UBaseWidgetTokenCatalog* GetResolvedBaseTokens() const;

	// Returns a text token from the configured token catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FBaseTextStyleToken ResolveTextStyle(EBaseTextRole role) const;

	// Returns a variant color from the configured token catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FLinearColor ResolveVariantColor(EBaseWidgetVariant variant) const;

	// Returns a state color from the configured token catalog.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FLinearColor ResolveStateColor(EBaseWidgetState state) const;

	// Updates the shared size preset used when resolving token typography.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	void SetBaseSize(EBaseWidgetSize inSize);

	// Returns the shared size preset used when resolving token typography.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	EBaseWidgetSize GetBaseSize() const { return Size; }

	// Updates optional min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	void SetSizeConstraints(FBaseWidgetSizeConstraints inSizeConstraints);

	// Returns optional min/max desired-size constraints for the WBP root wrapper.
	UFUNCTION(BlueprintPure, Category = "UI|Base Widgets")
	FBaseWidgetSizeConstraints GetSizeConstraints() const { return SizeConstraints; }

	// Applies exposed properties to bound WBP-owned controls.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	virtual void SynchronizeBaseProperties();

protected:
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

	// Optional token catalog override; empty uses the shared DA_BaseTokens asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Widgets", meta = (DisplayName = "Base Token Overrides", ExposeOnSpawn = "true"))
	TSoftObjectPtr<UBaseWidgetTokenCatalog> BaseTokens;

	// Shared size preset used by token typography; layout remains WBP-owned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseSize", Setter = "SetBaseSize", BlueprintGetter = "GetBaseSize", BlueprintSetter = "SetBaseSize", Category = "UI|Base Widgets", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetSize Size = EBaseWidgetSize::Medium;

	// Optional desired-size constraints applied when RootSize or RootSizeBox is bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSizeConstraints", Setter = "SetSizeConstraints", BlueprintGetter = "GetSizeConstraints", BlueprintSetter = "SetSizeConstraints", Category = "UI|Base Widgets", meta = (ExposeOnSpawn = "true"))
	FBaseWidgetSizeConstraints SizeConstraints;

	// Optional outer frame owned by the Widget Blueprint for stroke or selected-state highlights.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BorderFrame;

	// Optional WBP root size wrapper used for min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSize;

	// Optional alternate WBP root size wrapper used for min/max desired-size constraints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox;
};
