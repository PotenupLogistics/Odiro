#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"

void UProjectEpisodeReplayCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheInactiveReplayCardSurfaceColor();
	RefreshActiveReplayVisual();
}

void UProjectEpisodeReplayCardWidget::NativeDestruct()
{
	OnReplayRequested.Clear();
	Super::NativeDestruct();
}

void UProjectEpisodeReplayCardWidget::InitializeFromEpisodeViewModel(
	const UExperimentResultEpisodeViewModel* episodeItem)
{
	EpisodeId = episodeItem ? episodeItem->GetEpisodeId() : FString();
	EpisodeDirectory = episodeItem ? episodeItem->GetEpisodeDirectory() : FString();
	FPaths::NormalizeDirectoryName(EpisodeDirectory);
	bReplayAvailable = episodeItem && episodeItem->IsReplayAvailable();

	if (EpisodeIdText)
	{
		EpisodeIdText->SetText(FText::FromString(EpisodeId.IsEmpty() ? TEXT("--") : EpisodeId));
	}
	if (EpisodeStateText)
	{
		const FString outcomeLabel = episodeItem ? episodeItem->GetOutcomeLabel() : FString();
		const bool bEpisodeSuccess = episodeItem && episodeItem->IsSuccess();
		EpisodeStateText->SetText(outcomeLabel.IsEmpty()
			? (bEpisodeSuccess
				? NSLOCTEXT("OdiroPlatform", "EpisodeCardSuccess", "성공")
				: NSLOCTEXT("OdiroPlatform", "EpisodeCardFailed", "실패"))
			: FText::FromString(outcomeLabel));
		EpisodeStateText->SetColorAndOpacity(FSlateColor(
			bEpisodeSuccess ? SuccessEpisodeStateTextColor : FailureEpisodeStateTextColor));
	}
	if (EpisodeDurationText)
	{
		EpisodeDurationText->SetText(FText::FromString(episodeItem ? episodeItem->GetDurationLabel() : FString(TEXT("--"))));
	}
	if (ReplayAvailabilityText)
	{
		ReplayAvailabilityText->SetText(bReplayAvailable
			? NSLOCTEXT("OdiroPlatform", "EpisodeCardReplayReady", "Replay")
			: NSLOCTEXT("OdiroPlatform", "EpisodeCardReplayMissing", "No replay"));
	}

	RefreshActiveReplayVisual();
}

void UProjectEpisodeReplayCardWidget::SetActiveReplay(const bool bInActiveReplay)
{
	if (bActiveReplay == bInActiveReplay)
	{
		return;
	}

	bActiveReplay = bInActiveReplay;
	RefreshActiveReplayVisual();
}

FReply UProjectEpisodeReplayCardWidget::NativeOnMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (inMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnReplayRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(inGeometry, inMouseEvent);
}

void UProjectEpisodeReplayCardWidget::RefreshActiveReplayVisual()
{
	if (ReplayCardSurface)
	{
		CacheInactiveReplayCardSurfaceColor();
		ReplayCardSurface->SetBrushColor(
			bActiveReplay ? ActiveReplayCardSurfaceColor : InactiveReplayCardSurfaceColor);
	}
}

void UProjectEpisodeReplayCardWidget::CacheInactiveReplayCardSurfaceColor()
{
	if (ReplayCardSurface && !bHasInactiveReplayCardSurfaceColor)
	{
		InactiveReplayCardSurfaceColor = ReplayCardSurface->GetBrushColor();
		bHasInactiveReplayCardSurfaceColor = true;
	}
}
