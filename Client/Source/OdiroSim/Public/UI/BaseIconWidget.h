#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseIconWidget.generated.h"

class UImage;
class UTexture2D;
class UWidget;

// Standalone icon component for base widget layouts.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseIconWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies icon texture, variant color, and disabled state to the WBP-owned image.
	virtual void SynchronizeBaseProperties() override;

	// Updates the icon texture displayed by this component.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Icon")
	void SetIcon(UTexture2D* inIcon);

	// Returns the icon texture displayed by this component.
	UFUNCTION(BlueprintPure, Category = "UI|Base Icon")
	UTexture2D* GetIcon() const { return Icon; }

	// Updates the icon color variant.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Icon")
	void SetVariant(EBaseWidgetVariant inVariant);

	// Returns the icon color variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Icon")
	EBaseWidgetVariant GetVariant() const { return Variant; }

	// Updates the size hint used by the WBP layout.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Icon")
	void SetBaseSize(EBaseWidgetSize inSize);

	// Returns the size hint used by the WBP layout.
	UFUNCTION(BlueprintPure, Category = "UI|Base Icon")
	EBaseWidgetSize GetBaseSize() const { return Size; }

	// Updates the disabled visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Icon")
	void SetDisabled(bool bInDisabled);

	// Returns whether this icon should render as disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Icon")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Icon texture displayed by this component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIcon", Setter = "SetIcon", BlueprintGetter = "GetIcon", BlueprintSetter = "SetIcon", Category = "UI|Base Icon", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> Icon;

	// Icon color variant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetVariant", Setter = "SetVariant", BlueprintGetter = "GetVariant", BlueprintSetter = "SetVariant", Category = "UI|Base Icon", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetVariant Variant = EBaseWidgetVariant::Neutral;

	// Size hint used by WBP variants.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseSize", Setter = "SetBaseSize", BlueprintGetter = "GetBaseSize", BlueprintSetter = "SetBaseSize", Category = "UI|Base Icon", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetSize Size = EBaseWidgetSize::Medium;

	// Disabled icon state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Icon", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Icon visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// Fixed-size wrapper hidden when no icon image resource is assigned.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IconBox;
};
