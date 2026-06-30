#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseCardWidget.generated.h"

class UBorder;
class UTextBlock;

// Card container component for dashboard and list surfaces.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseCardWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies title, description, and surface state to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the card title.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetLabel(FText inLabel);

	// Returns the card title.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	FText GetLabel() const { return Label; }

	// Updates the card description.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetDescription(FText inDescription);

	// Returns the card description.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	FText GetDescription() const { return Description; }

	// Updates the card color variant.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetVariant(EBaseWidgetVariant inVariant);

	// Returns the card color variant.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	EBaseWidgetVariant GetVariant() const { return Variant; }

	// Updates the card semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the card semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates the selected visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetSelected(bool bInSelected);

	// Returns whether this card should render as selected.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	bool IsBaseSelected() const { return bSelected; }

	// Updates the disabled visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Card")
	void SetDisabled(bool bInDisabled);

	// Returns whether this card should render as disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Card")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Feeds the rounded surface material its painted size each paint (capture-safe).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Card title text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Card supporting text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDescription", Setter = "SetDescription", BlueprintGetter = "GetDescription", BlueprintSetter = "SetDescription", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Description;

	// Card color variant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetVariant", Setter = "SetVariant", BlueprintGetter = "GetVariant", BlueprintSetter = "SetVariant", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetVariant Variant = EBaseWidgetVariant::Neutral;

	// Card state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Selected card state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsBaseSelected", Setter = "SetSelected", BlueprintGetter = "IsBaseSelected", BlueprintSetter = "SetSelected", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bSelected = false;

	// Disabled card state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Card surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Optional accent strip owned by the Widget Blueprint for state and selection.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StateMarker;

	// Title visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Description visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTextBlock;
};
