#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/BaseWidget.h"
#include "BaseStatusBadgeWidget.generated.h"

class UBorder;
class UTextBlock;

// Compact semantic status badge component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseStatusBadgeWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies badge label and semantic color.
	virtual void SynchronizeBaseProperties() override;

	// Updates the badge label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetLabel(FText inLabel);

	// Returns the badge label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	FText GetLabel() const { return Label; }

	// Updates the badge semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the badge semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates the disabled visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetDisabled(bool bInDisabled);

	// Returns whether this badge should render as disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	bool IsDisabled() const { return bDisabled; }

	// Overrides the badge label color independently from semantic status color.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetLabelColorOverride(FLinearColor inLabelColor);

	// Restores the badge label color to semantic status color.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void ClearLabelColorOverride();

	// Overrides the badge label font independently from the base text role.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetLabelFontOverride(FSlateFontInfo inLabelFont);

	// Restores the badge label font to the base text role.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void ClearLabelFontOverride();

protected:
	// Badge text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Badge semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Disabled badge state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Explicit label color used when semantic status color should not drive text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	FLinearColor LabelColorOverride = FLinearColor::White;

	// Whether LabelColorOverride is currently applied to the badge label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	bool bHasLabelColorOverride = false;

	// Explicit label font used when the badge participates in mixed header layouts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	FSlateFontInfo LabelFontOverride;

	// Whether LabelFontOverride is currently applied to the badge label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	bool bHasLabelFontOverride = false;

	// Badge surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Square semantic status dot owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StatusDot;

	// Badge label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;
};
