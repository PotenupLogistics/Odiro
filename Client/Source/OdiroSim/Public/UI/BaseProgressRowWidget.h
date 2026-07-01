#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseProgressRowWidget.generated.h"

class UBorder;
class UTextBlock;

// Dashboard progress row component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseProgressRowWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies row label, value text, and clamped progress.
	virtual void SynchronizeBaseProperties() override;

	// Updates the progress row label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetLabel(FText inLabel);

	// Returns the progress row label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	FText GetLabel() const { return Label; }

	// Updates the progress row description.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetDescription(FText inDescription);

	// Returns the progress row description.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	FText GetDescription() const { return Description; }

	// Updates the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetProgressPercent(float inProgressPercent);

	// Returns the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	float GetProgressPercent() const { return ProgressPercent; }

	// Updates the progress value text.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetValueText(FText inValueText);

	// Returns the progress value text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	FText GetValueText() const { return ValueText; }

	// Updates the progress semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the progress semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates vertical placement of row content inside spare height.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Row")
	void SetContentVAlign(EBaseVerticalContentAlign inContentVAlign);

	// Returns vertical placement of row content inside spare height.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Row")
	EBaseVerticalContentAlign GetContentVAlign() const { return ContentVAlign; }

protected:
	// Feeds the rounded surface material its painted size each paint (capture-safe).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Progress row label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Optional progress row description.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDescription", Setter = "SetDescription", BlueprintGetter = "GetDescription", BlueprintSetter = "SetDescription", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Description;

	// Numeric progress percentage in the 0-100 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetProgressPercent", Setter = "SetProgressPercent", BlueprintGetter = "GetProgressPercent", BlueprintSetter = "SetProgressPercent", Category = "UI|State", meta = (ExposeOnSpawn = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float ProgressPercent = 0.0f;

	// Text shown near the progress indicator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValueText", Setter = "SetValueText", BlueprintGetter = "GetValueText", BlueprintSetter = "SetValueText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText ValueText;

	// Row semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Vertical placement for row content when the widget has spare height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetContentVAlign", Setter = "SetContentVAlign", BlueprintGetter = "GetContentVAlign", BlueprintSetter = "SetContentVAlign", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseVerticalContentAlign ContentVAlign = EBaseVerticalContentAlign::Bottom;

	// Row surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Row label visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Row description visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTextBlock;

	// Progress value visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueTextBlock;

	// Progress track surface owned by the Widget Blueprint; the progress SDF material
	// draws the rounded track and the percent fill in one pass.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ProgressTrack;
};
