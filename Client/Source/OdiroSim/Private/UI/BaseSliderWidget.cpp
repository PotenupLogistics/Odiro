#include "UI/BaseSliderWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseTextInputWidget.h"
#include "Components/Border.h"
#include "Components/Widget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseSliderWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	TGuardValue<bool> synchronizingGuard(bSynchronizing, true);
	const bool bEnabled = !bDisabled;
	if (ValueSlider)
	{
		ValueSlider->SetIsEnabled(bEnabled);
		ValueSlider->SetValue(NormalizeValue(Value, MinValue, MaxValue));
		SetOptionalWidgetVisible(ValueSlider.Get(), !bRangeMode, ESlateVisibility::HitTestInvisible);
	}
	if (LowerSlider)
	{
		LowerSlider->SetIsEnabled(bEnabled);
		LowerSlider->SetValue(NormalizeValue(LowerValue, MinValue, MaxValue));
		SetOptionalWidgetVisible(LowerSlider.Get(), bRangeMode, ESlateVisibility::HitTestInvisible);
	}
	if (UpperSlider)
	{
		UpperSlider->SetIsEnabled(bEnabled);
		UpperSlider->SetValue(NormalizeValue(UpperValue, MinValue, MaxValue));
		SetOptionalWidgetVisible(UpperSlider.Get(), bRangeMode, ESlateVisibility::HitTestInvisible);
	}
	if (ValueInput)
	{
		ValueInput->SetInputMode(EBaseTextInputMode::Number);
		ValueInput->SetDisplayDecimals(DisplayDecimals);
		ValueInput->SetValueRange(MinValue, MaxValue);
		ValueInput->SetNumericValue(Value);
		ValueInput->SetDisabled(!bEnabled || !bShowValueField);
		SetOptionalWidgetVisible(ValueInput.Get(), !bRangeMode && bShowValueField, ESlateVisibility::Visible);
	}
	if (RangeInput)
	{
		RangeInput->SetInputMode(EBaseTextInputMode::NumberRange);
		RangeInput->SetDisplayDecimals(DisplayDecimals);
		RangeInput->SetValueRange(MinValue, MaxValue);
		RangeInput->SetRangeValue(LowerValue, UpperValue);
		RangeInput->SetDisabled(!bEnabled || !bShowValueField);
		SetOptionalWidgetVisible(RangeInput.Get(), bRangeMode && bShowValueField, ESlateVisibility::Visible);
	}

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (tokens && TrackBackground)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			nullptr,
			TrackBackground.Get(),
			bEnabled ? tokens->SurfaceControlColor : tokens->SurfaceChromeColor,
			bEnabled ? tokens->LineInsetColor : tokens->LineSubtleColor,
			tokens->Radius,
			0.0f);
	}
	if (tokens && TrackFill)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			nullptr,
			TrackFill.Get(),
			bEnabled ? tokens->AccentColor : tokens->LineInsetColor,
			FLinearColor::Transparent,
			tokens->Radius,
			0.0f);
	}
	if (tokens && LowerMask)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			nullptr,
			LowerMask.Get(),
			bEnabled ? tokens->SurfaceControlColor : tokens->SurfaceChromeColor,
			FLinearColor::Transparent,
			tokens->Radius,
			0.0f);
		SetOptionalWidgetVisible(LowerMask.Get(), false, ESlateVisibility::HitTestInvisible);
	}
	UpdateTrackFillTransform();
	StyleHandle(ValueSlider.Get());
	StyleHandle(LowerSlider.Get());
	StyleHandle(UpperSlider.Get());
}

void UBaseSliderWidget::StyleHandle(USlider* slider) const
{
	if (!slider)
	{
		return;
	}
	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	// The track/fill are drawn by the borders, so the slider contributes only a
	// thin accent handle over a transparent bar.
	FSlateBrush noDrawBrush;
	noDrawBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	noDrawBrush.TintColor = FSlateColor(FLinearColor::Transparent);
	noDrawBrush.ImageSize = FVector2D::ZeroVector;
	FSliderStyle sliderStyle = slider->GetWidgetStyle();
	sliderStyle.SetNormalBarImage(noDrawBrush);
	sliderStyle.SetHoveredBarImage(noDrawBrush);
	sliderStyle.SetDisabledBarImage(noDrawBrush);
	slider->SetWidgetStyle(sliderStyle);
	slider->SetSliderBarColor(FLinearColor::Transparent);
	slider->SetSliderHandleColor(tokens ? tokens->AccentColor : FLinearColor::White);
}

int32 UBaseSliderWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	(void)AllottedGeometry;
	const FVector2D trackSize = TrackBackground ? TrackBackground->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	UpdateTrackFillTransform();
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(TrackBackground.Get(), trackSize);
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(TrackFill.Get(), TrackFill ? TrackFill->GetCachedGeometry().GetLocalSize() : trackSize);
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(LowerMask.Get(), LowerMask ? LowerMask->GetCachedGeometry().GetLocalSize() : trackSize);
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

float UBaseSliderWidget::TrackPositionFromCursor(const FVector2D& screenPosition) const
{
	// Map against the track geometry (the child sliders fill it) so the value is
	// correct even though the slider widget also contains the numeric field.
	const FGeometry trackGeometry = TrackBackground
		? TrackBackground->GetCachedGeometry()
		: GetCachedGeometry();
	const FVector2D local = trackGeometry.AbsoluteToLocal(screenPosition);
	const float width = trackGeometry.GetLocalSize().X;
	return width > 0.0f ? FMath::Clamp(local.X / width, 0.0f, 1.0f) : 0.0f;
}

void UBaseSliderWidget::ApplyDragPosition(const float normalized)
{
	const float dragged = DenormalizeValue(normalized, MinValue, MaxValue);
	if (!bRangeMode)
	{
		SetValue(dragged);
		OnValueChanged.Broadcast(this, Value);
		return;
	}
	if (ActiveDragHandle == 2)
	{
		SetRangeValue(LowerValue, dragged);
	}
	else
	{
		SetRangeValue(dragged, UpperValue);
	}
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}

void UBaseSliderWidget::UpdateTrackFillTransform() const
{
	if (!TrackFill)
	{
		return;
	}

	const float lowerPercent = bRangeMode ? NormalizeValue(LowerValue, MinValue, MaxValue) : 0.0f;
	const float upperPercent = bRangeMode
		? NormalizeValue(UpperValue, MinValue, MaxValue)
		: NormalizeValue(Value, MinValue, MaxValue);
	const float fillStart = FMath::Min(lowerPercent, upperPercent);
	const float fillEnd = FMath::Max(lowerPercent, upperPercent);
	const float fillWidth = fillEnd - fillStart;

	const FGeometry trackGeometry = TrackBackground
		? TrackBackground->GetCachedGeometry()
		: TrackFill->GetCachedGeometry();
	const float trackWidth = trackGeometry.GetLocalSize().X;

	FWidgetTransform fillTransform;
	fillTransform.Translation = FVector2D(trackWidth > 0.0f ? trackWidth * fillStart : 0.0f, 0.0f);
	fillTransform.Scale = FVector2D(fillWidth, 1.0f);
	TrackFill->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	TrackFill->SetRenderTransform(fillTransform);
	SetOptionalWidgetVisible(TrackFill.Get(), fillWidth > KINDA_SMALL_NUMBER && !bDisabled, ESlateVisibility::Visible);
}

FReply UBaseSliderWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDisabled || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const float cursorNorm = TrackPositionFromCursor(InMouseEvent.GetScreenSpacePosition());
	if (bRangeMode)
	{
		// Grab whichever handle is nearer the press so both stay independently draggable.
		const float lowerNorm = NormalizeValue(LowerValue, MinValue, MaxValue);
		const float upperNorm = NormalizeValue(UpperValue, MinValue, MaxValue);
		ActiveDragHandle = FMath::Abs(cursorNorm - lowerNorm) <= FMath::Abs(cursorNorm - upperNorm) ? 1 : 2;
	}
	else
	{
		ActiveDragHandle = 1;
	}
	ApplyDragPosition(cursorNorm);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UBaseSliderWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ActiveDragHandle != 0 && HasMouseCapture())
	{
		ApplyDragPosition(TrackPositionFromCursor(InMouseEvent.GetScreenSpacePosition()));
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UBaseSliderWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ActiveDragHandle != 0 && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ActiveDragHandle = 0;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UBaseSliderWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Hit-testable so mouse-move can pick the nearest range handle.
	SetVisibility(ESlateVisibility::Visible);

	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleValueSliderChanged);
		ValueSlider->OnValueChanged.AddDynamic(this, &UBaseSliderWidget::HandleValueSliderChanged);
	}
	if (LowerSlider)
	{
		LowerSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleLowerSliderChanged);
		LowerSlider->OnValueChanged.AddDynamic(this, &UBaseSliderWidget::HandleLowerSliderChanged);
	}
	if (UpperSlider)
	{
		UpperSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleUpperSliderChanged);
		UpperSlider->OnValueChanged.AddDynamic(this, &UBaseSliderWidget::HandleUpperSliderChanged);
	}
	if (ValueInput)
	{
		ValueInput->OnNumericValueCommitted.RemoveDynamic(this, &UBaseSliderWidget::HandleValueInputCommitted);
		ValueInput->OnNumericValueCommitted.AddDynamic(this, &UBaseSliderWidget::HandleValueInputCommitted);
	}
	if (RangeInput)
	{
		RangeInput->OnRangeValueCommitted.RemoveDynamic(this, &UBaseSliderWidget::HandleRangeInputCommitted);
		RangeInput->OnRangeValueCommitted.AddDynamic(this, &UBaseSliderWidget::HandleRangeInputCommitted);
	}
}

void UBaseSliderWidget::NativeDestruct()
{
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleValueSliderChanged);
	}
	if (LowerSlider)
	{
		LowerSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleLowerSliderChanged);
	}
	if (UpperSlider)
	{
		UpperSlider->OnValueChanged.RemoveDynamic(this, &UBaseSliderWidget::HandleUpperSliderChanged);
	}
	if (ValueInput)
	{
		ValueInput->OnNumericValueCommitted.RemoveDynamic(this, &UBaseSliderWidget::HandleValueInputCommitted);
	}
	if (RangeInput)
	{
		RangeInput->OnRangeValueCommitted.RemoveDynamic(this, &UBaseSliderWidget::HandleRangeInputCommitted);
	}

	Super::NativeDestruct();
}

void UBaseSliderWidget::SetRangeMode(const bool bInRangeMode)
{
	bRangeMode = bInRangeMode;
	SynchronizeBaseProperties();
}

void UBaseSliderWidget::SetShowValueField(const bool bInShowValueField)
{
	bShowValueField = bInShowValueField;
	SynchronizeBaseProperties();
}

void UBaseSliderWidget::SetValueRange(const float inMinValue, const float inMaxValue)
{
	MinValue = inMinValue;
	MaxValue = inMaxValue;
	NormalizeMinMax(MinValue, MaxValue);
	SetValue(Value);
	SetRangeValue(LowerValue, UpperValue);
}

void UBaseSliderWidget::SetValue(const float inValue)
{
	Value = FMath::Clamp(inValue, MinValue, MaxValue);
	SynchronizeBaseProperties();
}

void UBaseSliderWidget::SetRangeValue(const float inLowerValue, const float inUpperValue)
{
	LowerValue = FMath::Clamp(inLowerValue, MinValue, MaxValue);
	UpperValue = FMath::Clamp(inUpperValue, MinValue, MaxValue);
	NormalizeRange(LowerValue, UpperValue);
	SynchronizeBaseProperties();
}

void UBaseSliderWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseSliderWidget::HandleValueSliderChanged(const float normalizedValue)
{
	if (bSynchronizing)
	{
		return;
	}

	SetValue(DenormalizeValue(normalizedValue, MinValue, MaxValue));
	OnValueChanged.Broadcast(this, Value);
}

void UBaseSliderWidget::HandleLowerSliderChanged(const float normalizedValue)
{
	if (bSynchronizing)
	{
		return;
	}

	SetRangeValue(DenormalizeValue(normalizedValue, MinValue, MaxValue), UpperValue);
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}

void UBaseSliderWidget::HandleUpperSliderChanged(const float normalizedValue)
{
	if (bSynchronizing)
	{
		return;
	}

	SetRangeValue(LowerValue, DenormalizeValue(normalizedValue, MinValue, MaxValue));
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}

void UBaseSliderWidget::HandleValueInputCommitted(UBaseTextInputWidget* inputWidget, const float inValue)
{
	(void)inputWidget;
	SetValue(inValue);
	OnValueChanged.Broadcast(this, Value);
}

void UBaseSliderWidget::HandleRangeInputCommitted(UBaseTextInputWidget* inputWidget, const float inLowerValue, const float inUpperValue)
{
	(void)inputWidget;
	SetRangeValue(inLowerValue, inUpperValue);
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}
