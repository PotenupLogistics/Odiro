#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"
#include "BaseWidget.generated.h"

class UBorder;
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

	// Applies exposed properties to bound WBP-owned controls.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Widgets")
	virtual void SynchronizeBaseProperties();

protected:
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

	// Optional outer frame owned by the Widget Blueprint for stroke or selected-state highlights.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BorderFrame;
};
