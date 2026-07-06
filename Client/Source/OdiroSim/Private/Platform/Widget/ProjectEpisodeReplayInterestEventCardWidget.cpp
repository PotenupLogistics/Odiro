#include "Platform/Widget/ProjectEpisodeReplayInterestEventCardWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	FLinearColor GetReplayInterestEventColor(const FString& EventType)
	{
		if (EventType.Equals(TEXT("Stuck"), ESearchCase::IgnoreCase)
			|| EventType.Equals(TEXT("Collision"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.95f, 0.12f, 0.08f, 1.0f);
		}

		if (EventType.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(1.0f, 0.72f, 0.05f, 1.0f);
		}

		if (EventType.Equals(TEXT("RobotTipOver"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.75f, 0.18f, 1.0f, 1.0f);
		}

		if (EventType.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.18f, 0.85f, 0.25f, 1.0f);
		}

		return FLinearColor(0.50f, 0.72f, 1.0f, 1.0f);
	}

	FText FormatReplayInterestEventTime(const double TimeSeconds)
	{
		const double SafeTimeSeconds = FMath::Max(0.0, TimeSeconds);
		const int32 TotalSeconds = FMath::FloorToInt(SafeTimeSeconds);
		const int32 Minutes = TotalSeconds / 60;
		const int32 Seconds = TotalSeconds % 60;
		const int32 Centiseconds = FMath::FloorToInt(
			(SafeTimeSeconds - static_cast<double>(TotalSeconds)) * 100.0);
		return FText::FromString(FString::Printf(
			TEXT("%02d:%02d.%02d"),
			Minutes,
			Seconds,
			Centiseconds));
	}

	FText BuildReplayInterestEventTitle(const FScenarioReplayEventMarker& Marker)
	{
		if (Marker.EventType.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("경로 재계산"));
		}

		if (Marker.EventType.Equals(TEXT("Collision"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("충돌"));
		}

		if (Marker.EventType.Equals(TEXT("Stuck"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("정체"));
		}

		if (Marker.EventType.Equals(TEXT("RobotTipOver"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("전복"));
		}

		if (Marker.EventType.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("성공"));
		}

		return FText::FromString(Marker.EventType.IsEmpty()
			? TEXT("이벤트")
			: Marker.EventType);
	}

	FText BuildReplayInterestEventSummary(const FScenarioReplayEventMarker& Marker)
	{
		if (!Marker.Message.IsEmpty())
		{
			return FText::FromString(Marker.Message);
		}

		if (!Marker.Reason.IsEmpty())
		{
			return FText::FromString(Marker.Reason);
		}

		return FText::FromString(TEXT("상세 정보 없음"));
	}

	// ApplyRoundedSurface expects token-style sRGB input; WBP color pickers store
	// FLinearColor channels already linearized from values such as #242424.
	float EncodeLinearChannelForRoundedSurface(const float value)
	{
		const float clamped = FMath::Clamp(value, 0.0f, 1.0f);
		return clamped <= 0.0031308f
			? clamped * 12.92f
			: 1.055f * FMath::Pow(clamped, 1.0f / 2.4f) - 0.055f;
	}

	FLinearColor EncodeWbpColorForRoundedSurface(const FLinearColor& color)
	{
		return FLinearColor(
			EncodeLinearChannelForRoundedSurface(color.R),
			EncodeLinearChannelForRoundedSurface(color.G),
			EncodeLinearChannelForRoundedSurface(color.B),
			color.A);
	}
}

void UProjectEpisodeReplayInterestEventCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SynchronizeCardSurfaceProperties();

	if (CardButton)
	{
		CardButton->OnClicked.RemoveDynamic(
			this,
			&UProjectEpisodeReplayInterestEventCardWidget::HandleCardClicked);
		CardButton->OnClicked.AddDynamic(
			this,
			&UProjectEpisodeReplayInterestEventCardWidget::HandleCardClicked);
	}
}

void UProjectEpisodeReplayInterestEventCardWidget::NativeDestruct()
{
	if (CardButton)
	{
		CardButton->OnClicked.RemoveDynamic(
			this,
			&UProjectEpisodeReplayInterestEventCardWidget::HandleCardClicked);
	}

	OnInterestEventSelected.Clear();
	Super::NativeDestruct();
}

void UProjectEpisodeReplayInterestEventCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SynchronizeCardSurfaceProperties();
}

#if WITH_EDITOR
void UProjectEpisodeReplayInterestEventCardWidget::PostEditChangeProperty(
	FPropertyChangedEvent& propertyChangedEvent)
{
	Super::PostEditChangeProperty(propertyChangedEvent);
	SynchronizeCardSurfaceProperties();
}
#endif

int32 UProjectEpisodeReplayInterestEventCardWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const FVector2D fallbackSize = AllottedGeometry.GetLocalSize();
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(CardBackground.Get(), fallbackSize);
	return Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
}

void UProjectEpisodeReplayInterestEventCardWidget::InitializeFromEventMarker(
	const FScenarioReplayEventMarker& InMarker,
	bool bInitiallySelected)
{
	EventMarker = InMarker;

	const FString EventTypeLabel = EventMarker.EventType.IsEmpty()
		? TEXT("Event")
		: EventMarker.EventType;
	const FLinearColor EventColor = GetReplayInterestEventColor(EventMarker.EventType);

	if (EventTypeText)
	{
		EventTypeText->SetText(FText::FromString(EventTypeLabel));
	}
	if (EventTimeText)
	{
		EventTimeText->SetText(FormatReplayInterestEventTime(EventMarker.TimeSeconds));
	}
	if (EventTitleText)
	{
		EventTitleText->SetText(BuildReplayInterestEventTitle(EventMarker));
	}
	if (EventSummaryText)
	{
		EventSummaryText->SetText(BuildReplayInterestEventSummary(EventMarker));
	}
	if (EventTypePill)
	{
		FLinearColor PillColor = EventColor;
		PillColor.A = 0.85f;
		EventTypePill->SetBrushColor(PillColor);
	}
	if (EventAccentBar)
	{
		EventAccentBar->SetBrushColor(EventColor);
	}

	SetSelected(bInitiallySelected);
}

void UProjectEpisodeReplayInterestEventCardWidget::SetSelected(bool bNewSelected)
{
	bSelected = bNewSelected;
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SynchronizeCardSurfaceProperties()
{
	ApplyCardSurfaceStyle();
	ClearSelectedOverlayStyle();
	InvalidateLayoutAndVolatility();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetCardSurfaceFillColor(const FLinearColor inColor)
{
	CardSurfaceFillColor = inColor;
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetCardSurfaceStrokeColor(const FLinearColor inColor)
{
	CardSurfaceStrokeColor = inColor;
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetCardSurfaceRadius(const float inRadius)
{
	CardSurfaceRadius = FMath::Max(inRadius, 0.0f);
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetCardSurfaceBorderWidth(const float inBorderWidth)
{
	CardSurfaceBorderWidth = FMath::Max(inBorderWidth, 0.0f);
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetSelectedCardSurfaceFillColor(
	const FLinearColor inColor)
{
	SelectedOverlayFillColor = inColor;
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::SetSelectedCardSurfaceStrokeColor(
	const FLinearColor inColor)
{
	SelectedOverlayStrokeColor = inColor;
	SynchronizeCardSurfaceProperties();
}

void UProjectEpisodeReplayInterestEventCardWidget::ApplyCardSurfaceStyle()
{
	const FLinearColor fillColor = bSelected
		? SelectedOverlayFillColor
		: CardSurfaceFillColor;
	const FLinearColor strokeColor = bSelected
		? SelectedOverlayStrokeColor
		: CardSurfaceStrokeColor;

	BaseWidgetPrivate::ApplyRoundedSurface(
		nullptr,
		CardBackground.Get(),
		EncodeWbpColorForRoundedSurface(fillColor),
		EncodeWbpColorForRoundedSurface(strokeColor),
		CardSurfaceRadius,
		CardSurfaceBorderWidth);
}

void UProjectEpisodeReplayInterestEventCardWidget::ClearSelectedOverlayStyle()
{
	if (!SelectedOverlay)
	{
		return;
	}

	SelectedOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	BaseWidgetPrivate::MakeBorderVisualTransparent(SelectedOverlay.Get());
}

void UProjectEpisodeReplayInterestEventCardWidget::HandleCardClicked()
{
	OnInterestEventSelected.Broadcast(
		this,
		EventMarker.TimeSeconds,
		EventMarker.EventIndex);
}
