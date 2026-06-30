#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseSliderWidget.generated.h"

class UBorder;
class USlider;
class UTextBlock;

// Base-token styled single or range slider surface.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseSliderWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies value, range, and disabled state to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates whether this slider uses lower and upper handles.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider")
	void SetRangeMode(bool bInRangeMode);

	// Returns whether this slider uses lower and upper handles.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider")
	bool IsRangeMode() const { return bRangeMode; }

	// Updates the accepted value range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider")
	void SetValueRange(float inMinValue, float inMaxValue);

	// Updates the current single slider value.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider")
	void SetValue(float inValue);

	// Returns the current single slider value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider")
	float GetValue() const { return Value; }

	// Updates the current range slider values while preserving lower <= upper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider")
	void SetRangeValue(float inLowerValue, float inUpperValue);

	// Returns the current lower range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider")
	float GetLowerValue() const { return LowerValue; }

	// Returns the current upper range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider")
	float GetUpperValue() const { return UpperValue; }

	// Updates whether the slider is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider")
	void SetDisabled(bool bInDisabled);

	// Returns whether the slider is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after the single slider value changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSliderValueEvent OnValueChanged;

	// Broadcasts after the range slider values change.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSliderRangeEvent OnRangeValueChanged;

protected:
	// Binds optional slider child events after WBP construction.
	virtual void NativeConstruct() override;

	// Unbinds optional slider child events before destruction.
	virtual void NativeDestruct() override;

	// Keeps the track/fill material sizes fed and the active fill aligned each paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Starts owner-driven drag because child sliders are visual-only.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Moves the active drag handle along the WBP-authored track.
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Releases the active drag handle and mouse capture.
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Applies the accent thin handle + transparent bar style to one slider.
	void StyleHandle(USlider* slider) const;

	// Maps a screen-space cursor position to a 0..1 position along the track.
	float TrackPositionFromCursor(const FVector2D& screenPosition) const;

	// Applies a dragged 0..1 position to the active handle and broadcasts.
	void ApplyDragPosition(float normalized);

	// Applies the current value or range to the WBP-authored fill surface.
	void UpdateTrackFillTransform() const;

	// Handles the single value slider's normalized value.
	UFUNCTION()
	void HandleValueSliderChanged(float normalizedValue);

	// Handles the lower range slider's normalized value.
	UFUNCTION()
	void HandleLowerSliderChanged(float normalizedValue);

	// Handles the upper range slider's normalized value.
	UFUNCTION()
	void HandleUpperSliderChanged(float normalizedValue);

	// Whether lower and upper handles are active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsRangeMode", Setter = "SetRangeMode", BlueprintGetter = "IsRangeMode", BlueprintSetter = "SetRangeMode", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bRangeMode = false;

	// Minimum accepted slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Range", meta = (ExposeOnSpawn = "true"))
	float MinValue = 0.0f;

	// Maximum accepted slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Range", meta = (ExposeOnSpawn = "true"))
	float MaxValue = 100.0f;

	// Current single slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValue", Setter = "SetValue", BlueprintGetter = "GetValue", BlueprintSetter = "SetValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float Value = 0.0f;

	// Current lower range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLowerValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float LowerValue = 0.0f;

	// Current upper range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetUpperValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float UpperValue = 100.0f;

	// Disabled slider state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Prevents child value callbacks from echoing synchronization writes.
	UPROPERTY(Transient)
	bool bSynchronizing = false;

	// Active drag target: 0 none, 1 single/lower, 2 upper.
	UPROPERTY(Transient)
	int32 ActiveDragHandle = 0;

	// Rounded track bar (drawn as a progress surface: grey track + accent fill).
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TrackBackground;

	// Range mask: greys the 0..lower segment so the fill reads as lower..upper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> LowerMask;

	// Active fill surface whose size and vertical alignment are owned by the WBP.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TrackFill;

	// Single-value slider owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USlider> ValueSlider;

	// Lower range slider owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USlider> LowerSlider;

	// Upper range slider owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USlider> UpperSlider;

};
