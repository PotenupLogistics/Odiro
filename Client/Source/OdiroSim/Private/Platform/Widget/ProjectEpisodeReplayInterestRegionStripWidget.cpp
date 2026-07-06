#include "Platform/Widget/ProjectEpisodeReplayInterestRegionStripWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Platform/Widget/PlatformWidgetRuntime.h"
#include "Platform/Widget/ProjectEpisodeReplayInterestEventCardWidget.h"
#include "UObject/ConstructorHelpers.h"

UProjectEpisodeReplayInterestRegionStripWidget::UProjectEpisodeReplayInterestRegionStripWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UProjectEpisodeReplayInterestEventCardWidget> DefaultEventCardClass(
		TEXT("/Game/Widgets/Platform/WBP_ReplayInterestEventCard"));
	if (DefaultEventCardClass.Succeeded())
	{
		EventCardWidgetClass = DefaultEventCardClass.Class;
	}
}

void UProjectEpisodeReplayInterestRegionStripWidget::NativeDestruct()
{
	ClearEventMarkers();
	OnInterestEventSelected.Clear();
	Super::NativeDestruct();
}

void UProjectEpisodeReplayInterestRegionStripWidget::SetEventMarkers(
	const TArray<FScenarioReplayEventMarker>& InMarkers,
	double CurrentTimeSeconds)
{
	ClearEventMarkers();
	EventMarkers = InMarkers;

	if (InterestCountText)
	{
		InterestCountText->SetText(FText::FromString(FString::Printf(
			TEXT("%d개"),
			EventMarkers.Num())));
	}

	if (!InterestCardRow || !EventCardWidgetClass)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 InitialSelectedCardIndex = ResolveSelectedCardIndex(CurrentTimeSeconds);
	for (int32 EventIndex = 0; EventIndex < EventMarkers.Num(); ++EventIndex)
	{
		UProjectEpisodeReplayInterestEventCardWidget* CardWidget =
			CreateWidget<UProjectEpisodeReplayInterestEventCardWidget>(
				this,
				EventCardWidgetClass);
		if (!CardWidget)
		{
			continue;
		}

		if (UHorizontalBoxSlot* CardSlot = InterestCardRow->AddChildToHorizontalBox(CardWidget))
		{
			CardSlot->SetPadding(FMargin(
				0.0f,
				0.0f,
				EventIndex == EventMarkers.Num() - 1 ? 0.0f : 8.0f,
				0.0f));
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}

		PlatformWidgetRuntime::ClearRuntimeTransactionFlags(CardWidget);
		CardWidget->InitializeFromEventMarker(
			EventMarkers[EventIndex],
			EventIndex == InitialSelectedCardIndex);
		CardWidget->OnInterestEventSelected.AddUObject(
			this,
			&UProjectEpisodeReplayInterestRegionStripWidget::HandleCardSelected);
		EventCards.Add(CardWidget);
	}

	SetVisibility(EventCards.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::Visible);
	SelectedCardIndex = INDEX_NONE;
	SetSelectedCardIndex(InitialSelectedCardIndex, false);
}

void UProjectEpisodeReplayInterestRegionStripWidget::ClearEventMarkers()
{
	for (UProjectEpisodeReplayInterestEventCardWidget* CardWidget : EventCards)
	{
		if (CardWidget)
		{
			CardWidget->OnInterestEventSelected.RemoveAll(this);
		}
	}

	EventCards.Reset();
	EventMarkers.Reset();
	SelectedCardIndex = INDEX_NONE;
	FocusedEventIndex = INDEX_NONE;

	if (InterestCardRow)
	{
		InterestCardRow->ClearChildren();
	}
	if (InterestCountText)
	{
		InterestCountText->SetText(FText::FromString(TEXT("0개")));
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UProjectEpisodeReplayInterestRegionStripWidget::SetCurrentTime(
	double CurrentTimeSeconds,
	bool bScrollSelectedIntoView)
{
	if (FocusedEventIndex != INDEX_NONE)
	{
		const int32 FocusedCardIndex = FindCardIndexByEventIndex(FocusedEventIndex);
		if (EventMarkers.IsValidIndex(FocusedCardIndex))
		{
			const double FocusedDistanceSeconds =
				FMath::Abs(EventMarkers[FocusedCardIndex].TimeSeconds - CurrentTimeSeconds);
			if (FocusedDistanceSeconds <= SelectionWindowSeconds)
			{
				SetSelectedCardIndex(FocusedCardIndex, bScrollSelectedIntoView);
				return;
			}
		}

		FocusedEventIndex = INDEX_NONE;
	}

	SetSelectedCardIndex(
		ResolveSelectedCardIndex(CurrentTimeSeconds),
		bScrollSelectedIntoView);
}

bool UProjectEpisodeReplayInterestRegionStripWidget::FocusEventByIndex(int32 EventIndex)
{
	const int32 CardIndex = FindCardIndexByEventIndex(EventIndex);
	if (EventMarkers.IsValidIndex(CardIndex))
	{
		FocusedEventIndex = EventIndex;
		SetSelectedCardIndex(CardIndex, true);
		return true;
	}

	return false;
}

void UProjectEpisodeReplayInterestRegionStripWidget::HandleCardSelected(
	UProjectEpisodeReplayInterestEventCardWidget* CardWidget,
	double /*TimeSeconds*/,
	int32 /*EventIndex*/)
{
	const int32 CardIndex = FindCardIndex(CardWidget);
	if (!EventMarkers.IsValidIndex(CardIndex))
	{
		return;
	}

	FocusedEventIndex = EventMarkers[CardIndex].EventIndex;
	SetSelectedCardIndex(CardIndex, true);
	OnInterestEventSelected.Broadcast(
		this,
		EventMarkers[CardIndex].TimeSeconds,
		FocusedEventIndex);
}

int32 UProjectEpisodeReplayInterestRegionStripWidget::ResolveSelectedCardIndex(
	double CurrentTimeSeconds) const
{
	int32 BestCardIndex = INDEX_NONE;
	double BestDistanceSeconds = SelectionWindowSeconds;

	for (int32 CardIndex = 0; CardIndex < EventMarkers.Num(); ++CardIndex)
	{
		const double DistanceSeconds =
			FMath::Abs(EventMarkers[CardIndex].TimeSeconds - CurrentTimeSeconds);
		if (DistanceSeconds <= SelectionWindowSeconds
			&& (BestCardIndex == INDEX_NONE || DistanceSeconds < BestDistanceSeconds))
		{
			BestCardIndex = CardIndex;
			BestDistanceSeconds = DistanceSeconds;
		}
	}

	return BestCardIndex;
}

void UProjectEpisodeReplayInterestRegionStripWidget::SetSelectedCardIndex(
	int32 NewSelectedCardIndex,
	bool bScrollSelectedIntoView)
{
	const bool bSelectionChanged = SelectedCardIndex != NewSelectedCardIndex;
	SelectedCardIndex = NewSelectedCardIndex;

	for (int32 CardIndex = 0; CardIndex < EventCards.Num(); ++CardIndex)
	{
		if (UProjectEpisodeReplayInterestEventCardWidget* CardWidget = EventCards[CardIndex])
		{
			CardWidget->SetSelected(CardIndex == SelectedCardIndex);
		}
	}

	if (bScrollSelectedIntoView
		&& InterestScrollBox
		&& EventCards.IsValidIndex(SelectedCardIndex)
		&& EventCards[SelectedCardIndex])
	{
		InterestScrollBox->ScrollWidgetIntoView(
			EventCards[SelectedCardIndex].Get(),
			bSelectionChanged,
			EDescendantScrollDestination::TopOrLeft,
			12.0f);
	}
}

int32 UProjectEpisodeReplayInterestRegionStripWidget::FindCardIndex(
	const UProjectEpisodeReplayInterestEventCardWidget* CardWidget) const
{
	for (int32 CardIndex = 0; CardIndex < EventCards.Num(); ++CardIndex)
	{
		if (EventCards[CardIndex] == CardWidget)
		{
			return CardIndex;
		}
	}

	return INDEX_NONE;
}

int32 UProjectEpisodeReplayInterestRegionStripWidget::FindCardIndexByEventIndex(
	int32 EventIndex) const
{
	for (int32 CardIndex = 0; CardIndex < EventMarkers.Num(); ++CardIndex)
	{
		if (EventMarkers[CardIndex].EventIndex == EventIndex)
		{
			return CardIndex;
		}
	}

	return INDEX_NONE;
}
