#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "Styling/SlateBrush.h"

bool UProjectEpisodeReplayViewerWidget::OpenEpisodeReplay(const FString& EpisodeDirectory)
{
	LoadedEpisodeDirectory = EpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(LoadedEpisodeDirectory);
	if (LoadedEpisodeDirectory.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Replay episode directory is empty."));
		return false;
	}

	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return false;
	}

	TArray<FString> Diagnostics;
	if (!ReplaySubsystem->LoadEpisodeReplay(LoadedEpisodeDirectory, Diagnostics))
	{
		SetDiagnosticsText(Diagnostics.IsEmpty()
			? TEXT("Replay load failed.")
			: FString::Join(Diagnostics, TEXT("\n")));
		return false;
	}

	ApplyReplayRenderTarget();
	ReplaySubsystem->Play();
	SetVisibility(ESlateVisibility::Visible);
	SetDiagnosticsText(FString::Printf(TEXT("Replay playing: %s"), *LoadedEpisodeDirectory));
	return true;
}

void UProjectEpisodeReplayViewerWidget::ResetReplay()
{
	if (UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem())
	{
		ReplaySubsystem->UnloadReplay();
	}

	LoadedEpisodeDirectory.Reset();
	if (ReplayImage)
	{
		ReplayImage->SetBrush(FSlateBrush());
	}
	SetDiagnosticsText(TEXT("Replay stopped."));
}

void UProjectEpisodeReplayViewerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		PlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		PauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (StopButton)
	{
		StopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
		StopButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
		ResetButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCloseClicked);
	}
}

void UProjectEpisodeReplayViewerWidget::NativeDestruct()
{
	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (StopButton)
	{
		StopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCloseClicked);
	}

	ResetReplay();
	Super::NativeDestruct();
}

void UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (ReplaySubsystem->GetPlaybackState() == EScenarioReplayPlaybackState::Playing)
	{
		ReplaySubsystem->Pause();
		SetDiagnosticsText(TEXT("Replay paused."));
		return;
	}

	ReplaySubsystem->Play();
	SetDiagnosticsText(TEXT("Replay playing."));
}

void UProjectEpisodeReplayViewerWidget::HandleStopClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	ReplaySubsystem->Pause();
	SetDiagnosticsText(TEXT("Replay paused at current frame."));
}

void UProjectEpisodeReplayViewerWidget::HandleResetClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	ReplaySubsystem->Stop();
	SetDiagnosticsText(TEXT("Replay reset to first frame."));
}

void UProjectEpisodeReplayViewerWidget::HandleCloseClicked()
{
	ResetReplay();
	SetVisibility(ESlateVisibility::Collapsed);
}

UScenarioReplaySubsystem* UProjectEpisodeReplayViewerWidget::GetReplaySubsystem() const
{
	UWorld* World = GetWorld();
	return IsValid(World)
		? World->GetSubsystem<UScenarioReplaySubsystem>()
		: nullptr;
}

void UProjectEpisodeReplayViewerWidget::ApplyReplayRenderTarget()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem || !ReplayImage)
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = ReplaySubsystem->GetReplayRenderTarget();
	if (!RenderTarget)
	{
		return;
	}

	FSlateBrush ReplayBrush;
	ReplayBrush.SetResourceObject(RenderTarget);
	ReplayBrush.ImageSize = FVector2D(RenderTarget->SizeX, RenderTarget->SizeY);
	ReplayImage->SetBrush(ReplayBrush);
}

void UProjectEpisodeReplayViewerWidget::SetDiagnosticsText(const FString& Message)
{
	LastDiagnosticsText = Message;
	if (ReplayDiagnosticsText)
	{
		ReplayDiagnosticsText->SetText(FText::FromString(Message));
	}
}
