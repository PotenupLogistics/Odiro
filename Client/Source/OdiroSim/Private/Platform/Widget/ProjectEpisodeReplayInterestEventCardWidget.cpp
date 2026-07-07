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

	FText BuildReplayInterestEventTypeLabel(const FString& EventType)
	{
		if (EventType.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("경로 재계산"));
		}
		if (EventType.Equals(TEXT("Collision"), ESearchCase::IgnoreCase)
			|| EventType.Equals(TEXT("StaticObstacleCollision"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("충돌"));
		}
		if (EventType.Equals(TEXT("BlockedRegionCollision"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("차단 영역 충돌"));
		}
		if (EventType.Equals(TEXT("PedestrianCollision"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("보행자 충돌"));
		}
		if (EventType.Equals(TEXT("Stuck"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("정체"));
		}
		if (EventType.Equals(TEXT("RobotTipOver"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("전복"));
		}
		if (EventType.Equals(TEXT("GoalReached"), ESearchCase::IgnoreCase)
			|| EventType.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("목표 도달"));
		}
		if (EventType.Equals(TEXT("Timeout"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("시간 초과"));
		}
		if (EventType.Equals(TEXT("PathfindFail"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("경로 탐색 실패"));
		}
		if (EventType.Equals(TEXT("PolicyDecisionError"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("정책 판단 오류"));
		}
		if (EventType.Equals(TEXT("DeliveryBotSimulationFailure"), ESearchCase::IgnoreCase))
		{
			return FText::FromString(TEXT("시뮬레이션 실패"));
		}

		return FText::FromString(TEXT("이벤트"));
	}

	FText BuildReplayInterestEventTitle(const FScenarioReplayEventMarker& Marker)
	{
		return BuildReplayInterestEventTypeLabel(Marker.EventType);
	}

	bool TryBuildReplayInterestEventSummaryText(const FString& SourceText, FText& OutText)
	{
		const FString NormalizedText = SourceText.TrimStartAndEnd();
		if (NormalizedText.IsEmpty())
		{
			return false;
		}

		if (NormalizedText.Equals(TEXT("local_repath_ready"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("로컬 경로 재계산을 실행했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("collision_repath_ready"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("충돌 상황 이후 경로를 다시 계산했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("dynamic_repath_ready"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("동적 장애물 감지로 경로를 다시 계산했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("경로 재계산 이벤트가 기록되었습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("Collision"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("StaticObstacleCollision"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("BlockedRegionCollision"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("PedestrianCollision"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("충돌이 기록되었습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("Stuck"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("로봇 정체가 기록되었습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("RobotTipOver"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("로봇 전복이 기록되었습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("GoalReached"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("Success"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("목표 지점에 도달했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("Timeout"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("제한 시간이 초과되었습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("PathfindFail"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("PathFindingFailed"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("PATH_NOT_FOUND"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("path_not_found"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("start_cell_blocked"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("goal_cell_blocked"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("경로 탐색에 실패했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("PolicyDecisionError"), ESearchCase::IgnoreCase)
			|| NormalizedText.Equals(TEXT("PYTHON_REQUEST_FAILED"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("정책 판단 중 오류가 발생했습니다."));
			return true;
		}
		if (NormalizedText.Equals(TEXT("DeliveryBotSimulationFailure"), ESearchCase::IgnoreCase))
		{
			OutText = FText::FromString(TEXT("주행 시뮬레이션 실패가 기록되었습니다."));
			return true;
		}

		return false;
	}

	FText BuildReplayInterestEventSummary(const FScenarioReplayEventMarker& Marker)
	{
		FText SummaryText;
		if (!Marker.Reason.IsEmpty()
			&& TryBuildReplayInterestEventSummaryText(Marker.Reason, SummaryText))
		{
			return SummaryText;
		}

		if (!Marker.Message.IsEmpty()
			&& TryBuildReplayInterestEventSummaryText(Marker.Message, SummaryText))
		{
			return SummaryText;
		}

		if (!Marker.EventType.IsEmpty()
			&& TryBuildReplayInterestEventSummaryText(Marker.EventType, SummaryText))
		{
			return SummaryText;
		}

		return FText::FromString(TEXT("이벤트 상세 정보가 기록되었습니다."));
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

	const FText EventTypeLabel = BuildReplayInterestEventTypeLabel(EventMarker.EventType);
	const FLinearColor EventColor = GetReplayInterestEventColor(EventMarker.EventType);

	if (EventTypeText)
	{
		EventTypeText->SetText(EventTypeLabel);
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
