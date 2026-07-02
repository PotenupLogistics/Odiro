#include "Platform/Widget/ProjectEpisodeReplayInterestEventCardWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

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
}

void UProjectEpisodeReplayInterestEventCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
	if (!SelectedOverlay)
	{
		return;
	}

	SelectedOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	SelectedOverlay->SetBrushColor(FLinearColor(
		0.0f,
		0.48f,
		1.0f,
		bNewSelected ? 0.22f : 0.0f));
}

void UProjectEpisodeReplayInterestEventCardWidget::HandleCardClicked()
{
	OnInterestEventSelected.Broadcast(this, EventMarker.TimeSeconds);
}
