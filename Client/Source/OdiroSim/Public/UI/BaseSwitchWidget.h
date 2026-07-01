#pragma once

#include "CoreMinimal.h"
#include "Components/CheckBox.h"
#include "UI/BaseWidget.h"
#include "BaseSwitchWidget.generated.h"

class UBorder;
class UWidget;

// Non-interactive switch visual used by toggle buttons while WBP owns size and layout.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseSwitchWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies checked and disabled state to the WBP-authored track and thumb.
	virtual void SynchronizeBaseProperties() override;

	// Updates the switch checked state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Switch")
	void SetCheckState(ECheckBoxState inCheckState);

	// Returns the switch checked state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Switch")
	ECheckBoxState GetCheckState() const { return CheckState; }

	// Updates whether the switch is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Switch")
	void SetDisabled(bool bInDisabled);

	// Returns whether the switch is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Switch")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Feeds rounded surface materials their WBP-authored size each paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Current visual check state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCheckState", Setter = "SetCheckState", BlueprintGetter = "GetCheckState", BlueprintSetter = "SetCheckState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	// Disabled visual state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Track surface whose dimensions and padding are owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TrackSurface;

	// Thumb surface whose dimensions and padding are owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ThumbSurface;

	// Thumb layout slot whose left/right alignment reflects the checked state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ThumbSlot;
};
