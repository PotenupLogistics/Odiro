#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseProgressCardWidget.generated.h"

class UBorder;
class UBaseProgressBarWidget;
class UTextBlock;

// Dashboard progress card component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseProgressCardWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies card label, value text, and clamped progress.
	virtual void SynchronizeBaseProperties() override;

	// Updates the progress card label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetLabel(FText inLabel);

	// Returns the progress card label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	FText GetLabel() const { return Label; }

	// Updates the progress card description.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetDescription(FText inDescription);

	// Returns the progress card description.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	FText GetDescription() const { return Description; }

	// Updates the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetProgressPercent(float inProgressPercent);

	// Returns the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	float GetProgressPercent() const { return ProgressPercent; }

	// Updates the progress value text.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetValueText(FText inValueText);

	// Returns the progress value text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	FText GetValueText() const { return ValueText; }

	// Updates the progress semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the progress semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates vertical placement of card content inside spare height.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Card")
	void SetContentVAlign(EBaseVerticalContentAlign inContentVAlign);

	// Returns vertical placement of card content inside spare height.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Card")
	EBaseVerticalContentAlign GetContentVAlign() const { return ContentVAlign; }

protected:
	// Feeds the rounded surface material its painted size each paint (capture-safe).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Progress card label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Optional progress card description.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDescription", Setter = "SetDescription", BlueprintGetter = "GetDescription", BlueprintSetter = "SetDescription", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Description;

	// Numeric progress percentage in the 0-100 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetProgressPercent", Setter = "SetProgressPercent", BlueprintGetter = "GetProgressPercent", BlueprintSetter = "SetProgressPercent", Category = "UI|State", meta = (ExposeOnSpawn = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float ProgressPercent = 0.0f;

	// Text shown near the progress indicator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValueText", Setter = "SetValueText", BlueprintGetter = "GetValueText", BlueprintSetter = "SetValueText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText ValueText;

	// Card semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Vertical placement for card content when the widget has spare height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetContentVAlign", Setter = "SetContentVAlign", BlueprintGetter = "GetContentVAlign", BlueprintSetter = "SetContentVAlign", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseVerticalContentAlign ContentVAlign = EBaseVerticalContentAlign::Bottom;

	// Card surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Card label visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Card description visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTextBlock;

	// Progress value visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueTextBlock;

	// Progress bar child owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseProgressBarWidget> ProgressBar;
};
