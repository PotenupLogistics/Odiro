#include "Scenario/Replay/ScenarioReplayDebugWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "Styling/SlateBrush.h"

void UScenarioReplayDebugWidget::SetDebugEpisodeDirectory(const FString& EpisodeDirectory)
{
	DebugEpisodeDirectory = EpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(DebugEpisodeDirectory);
}

void UScenarioReplayDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReplayButton)
	{
		ReplayButton->OnClicked.RemoveDynamic(this, &UScenarioReplayDebugWidget::HandleReplayClicked);
		ReplayButton->OnClicked.AddDynamic(this, &UScenarioReplayDebugWidget::HandleReplayClicked);
	}

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UScenarioReplayDebugWidget::HandlePlayPauseClicked);
		PlayPauseButton->OnClicked.AddDynamic(this, &UScenarioReplayDebugWidget::HandlePlayPauseClicked);
	}
}

void UScenarioReplayDebugWidget::NativeDestruct()
{
	if (ReplayButton)
	{
		ReplayButton->OnClicked.RemoveDynamic(this, &UScenarioReplayDebugWidget::HandleReplayClicked);
	}

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UScenarioReplayDebugWidget::HandlePlayPauseClicked);
	}

	Super::NativeDestruct();
}

void UScenarioReplayDebugWidget::HandleReplayClicked()
{
	UWorld* World = GetWorld();
	UScenarioReplaySubsystem* ReplaySubsystem = IsValid(World)
		? World->GetSubsystem<UScenarioReplaySubsystem>()
		: nullptr;
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	TArray<FString> Diagnostics;
	if (!ReplaySubsystem->LoadEpisodeReplay(DebugEpisodeDirectory, Diagnostics))
	{
		SetDiagnosticsText(FString::Join(Diagnostics, TEXT("\n")));
		return;
	}

	if (ReplayImage)
	{
		if (UTextureRenderTarget2D* RenderTarget = ReplaySubsystem->GetReplayRenderTarget())
		{
			FSlateBrush ReplayBrush;
			ReplayBrush.SetResourceObject(RenderTarget);
			ReplayBrush.ImageSize = FVector2D(RenderTarget->SizeX, RenderTarget->SizeY);
			ReplayImage->SetBrush(ReplayBrush);
		}
	}

	ReplaySubsystem->Play();
	SetDiagnosticsText(TEXT("Replay loaded."));
}

void UScenarioReplayDebugWidget::HandlePlayPauseClicked()
{
	UWorld* World = GetWorld();
	UScenarioReplaySubsystem* ReplaySubsystem = IsValid(World)
		? World->GetSubsystem<UScenarioReplaySubsystem>()
		: nullptr;
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

void UScenarioReplayDebugWidget::SetDiagnosticsText(const FString& Message)
{
	if (ReplayDiagnosticsText)
	{
		ReplayDiagnosticsText->SetText(FText::FromString(Message));
	}
}
