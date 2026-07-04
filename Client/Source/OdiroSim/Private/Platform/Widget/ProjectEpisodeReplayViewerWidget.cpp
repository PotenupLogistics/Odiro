#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Platform/Widget/ProjectEpisodeReplayInterestRegionStripWidget.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "Styling/SlateBrush.h"
#include "UI/BaseTextWidget.h"
#include "Widgets/SWidget.h"

namespace
{
	const int32 ReplayFullscreenLayerZOrder = 100;
	const FVector2D ReplayEventMarkerSize(7.0f, 14.0f);
	// Fixed icon size used by the compact fullscreen replay control bar.
	const FVector2D ReplayPlaybackIconSize(24.0f, 24.0f);
	// Fixed icon size used by the compact camera mode button.
	const FVector2D ReplayCameraIconSize(22.0f, 22.0f);
	// Default play icon asset used when the replay is paused or ready.
	const TCHAR* ReplayPlayIconPath = TEXT("/Game/Textures/Icon/T_Icon_MediaPlay.T_Icon_MediaPlay");
	// Default pause icon asset used when the replay is actively playing.
	const TCHAR* ReplayPauseIconPath = TEXT("/Game/Textures/Icon/T_Icon_MediaPause.T_Icon_MediaPause");
	// Top-down camera icon asset used by the compact camera cycle button.
	const TCHAR* ReplayTopDownCameraIconPath = TEXT("/Game/Textures/Icon/T_Icon_ViewTop.T_Icon_ViewTop");
	// Third-person camera icon asset used by orbit and free camera modes.
	const TCHAR* ReplayThirdPersonCameraIconPath = TEXT("/Game/Textures/Icon/T_Icon_ViewTpv.T_Icon_ViewTpv");
	// Free camera icon asset used by the compact camera cycle button.
	const TCHAR* ReplayFreeCameraIconPath = TEXT("/Game/Textures/Icon/T_Icon_TargetLockOn.T_Icon_TargetLockOn");
	// First-person camera icon asset used by the vehicle-front camera mode.
	const TCHAR* ReplayFirstPersonCameraIconPath = TEXT("/Game/Textures/Icon/T_Icon_ViewFpv.T_Icon_ViewFpv");

	// Finds the numeric suffix used by episode replay directories.
	FString ExtractReplayEpisodeNumberToken(const FString& EpisodeDirectory)
	{
		FString normalizedDirectory = EpisodeDirectory.TrimStartAndEnd();
		FPaths::NormalizeDirectoryName(normalizedDirectory);
		const FString cleanName = FPaths::GetCleanFilename(normalizedDirectory);
		if (cleanName.IsEmpty())
		{
			return FString();
		}

		int32 digitEnd = cleanName.Len() - 1;
		while (digitEnd >= 0 && !FChar::IsDigit(cleanName[digitEnd]))
		{
			--digitEnd;
		}
		if (digitEnd < 0)
		{
			return cleanName;
		}

		int32 digitStart = digitEnd;
		while (digitStart >= 0 && FChar::IsDigit(cleanName[digitStart]))
		{
			--digitStart;
		}
		return cleanName.Mid(digitStart + 1, digitEnd - digitStart);
	}

	// Formats the current replay episode number for compact replay chrome.
	FText FormatReplayEpisodeNumberText(const FString& EpisodeDirectory)
	{
		const FString numberToken = ExtractReplayEpisodeNumberToken(EpisodeDirectory);
		if (numberToken.IsEmpty())
		{
			return FText::GetEmpty();
		}

		if (numberToken.IsNumeric())
		{
			const int64 episodeNumber = FCString::Atoi64(*numberToken);
			return FText::FromString(FString::Printf(TEXT("#%lld"), static_cast<long long>(episodeNumber)));
		}
		return FText::FromString(numberToken);
	}

	// Sets optional replay chrome text regardless of whether WBP uses BaseText or TextBlock.
	void SetReplayChromeText(UWidget* Widget, const FText& Text)
	{
		if (!Widget)
		{
			return;
		}

		if (UBaseTextWidget* BaseText = Cast<UBaseTextWidget>(Widget))
		{
			BaseText->SetText(Text);
		}
		else if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			TextBlock->SetText(Text);
		}

		Widget->SetVisibility(Text.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	// Forces WBP-authored fullscreen slots to fill their parent instead of keeping designer-time fixed offsets.
	void ApplyReplayFillSlot(UWidget* widget, const int32 zOrder)
	{
		if (!widget)
		{
			return;
		}

		if (UCanvasPanelSlot* canvasSlot = Cast<UCanvasPanelSlot>(widget->Slot))
		{
			canvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			canvasSlot->SetAlignment(FVector2D::ZeroVector);
			canvasSlot->SetOffsets(FMargin(0.0f));
			if (zOrder != INDEX_NONE)
			{
				canvasSlot->SetZOrder(zOrder);
			}
			return;
		}

		if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(widget->Slot))
		{
			overlaySlot->SetHorizontalAlignment(HAlign_Fill);
			overlaySlot->SetVerticalAlignment(VAlign_Fill);
			overlaySlot->SetPadding(FMargin(0.0f));
		}
	}

	// Returns the timeline marker color used for one replay event type.
	FLinearColor GetReplayEventMarkerColor(const FString& EventType)
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

		return FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);
	}

	// Builds the hover text shown for one timeline event marker.
	FText BuildReplayEventMarkerTooltip(const FScenarioReplayEventMarker& Marker)
	{
		const FString Label = Marker.EventType.IsEmpty()
			? TEXT("Event")
			: Marker.EventType;
		const FString Detail = !Marker.Message.IsEmpty()
			? Marker.Message
			: Marker.Reason;

		return Detail.IsEmpty()
			? FText::FromString(FString::Printf(
				TEXT("%s\n%.2fs"),
				*Label,
				Marker.TimeSeconds))
			: FText::FromString(FString::Printf(
				TEXT("%s\n%.2fs\n%s"),
				*Label,
				Marker.TimeSeconds,
				*Detail));
	}

	// Clears transient timeline marker widgets without leaving Slate tooltip state alive.
	void ClearReplayMarkerCanvas(UCanvasPanel* MarkerCanvas)
	{
		if (!MarkerCanvas)
		{
			return;
		}

		const int32 ChildCount = MarkerCanvas->GetChildrenCount();
		if (ChildCount <= 0)
		{
			return;
		}

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().CloseToolTip();
		}

		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			if (UWidget* MarkerWidget = MarkerCanvas->GetChildAt(ChildIndex))
			{
				MarkerWidget->SetToolTipText(FText::GetEmpty());
				MarkerWidget->SetToolTip(nullptr);
			}
		}

		MarkerCanvas->ClearChildren();
	}

	// Applies one texture brush to a WBP-authored icon image.
	void ApplyReplayIconBrush(
		UImage* IconImage,
		UTexture2D* IconTexture,
		const FVector2D& IconSize,
		const bool bEnabled)
	{
		if (!IconImage || !IconTexture)
		{
			return;
		}

		FSlateBrush IconBrush;
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		IconBrush.SetResourceObject(IconTexture);
		IconBrush.ImageSize = IconSize;
		IconBrush.TintColor = FSlateColor(
			bEnabled
				? FLinearColor::White
				: FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
		IconImage->SetBrush(IconBrush);
	}
}

UProjectEpisodeReplayViewerWidget::UProjectEpisodeReplayViewerWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	ReplayPlayIconTexture = LoadObject<UTexture2D>(nullptr, ReplayPlayIconPath);
	ReplayPauseIconTexture = LoadObject<UTexture2D>(nullptr, ReplayPauseIconPath);
	ReplayTopDownCameraIconTexture = LoadObject<UTexture2D>(nullptr, ReplayTopDownCameraIconPath);
	ReplayThirdPersonCameraIconTexture = LoadObject<UTexture2D>(nullptr, ReplayThirdPersonCameraIconPath);
	ReplayFreeCameraIconTexture = LoadObject<UTexture2D>(nullptr, ReplayFreeCameraIconPath);
	ReplayFirstPersonCameraIconTexture = LoadObject<UTexture2D>(nullptr, ReplayFirstPersonCameraIconPath);
}

bool UProjectEpisodeReplayViewerWidget::OpenEpisodeReplay(const FString& EpisodeDirectory)
{
	LoadedEpisodeDirectory = EpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(LoadedEpisodeDirectory);
	if (LoadedEpisodeDirectory.IsEmpty())
	{
		UpdateReplayEpisodeNumberText();
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
		LoadedEpisodeDirectory.Reset();
		UpdateReplayEpisodeNumberText();
		SetDiagnosticsText(Diagnostics.IsEmpty()
			? TEXT("Replay load failed.")
			: FString::Join(Diagnostics, TEXT("\n")));
		return false;
	}

	ApplyReplayRenderTarget();
	ReplaySubsystem->Play();
	SetReplayFullscreen(false);
	SetVisibility(ESlateVisibility::Visible);
	UpdateCameraModeText();
	UpdateReplayEpisodeNumberText();
	UpdateReplayTimelineUi();
	RebuildReplayEventMarkers();
	RebuildReplayInterestRegions();
	SetDiagnosticsText(FString::Printf(TEXT("Replay playing: %s"), *LoadedEpisodeDirectory));
	RequestReplayInputFocus();
	return true;
}

void UProjectEpisodeReplayViewerWidget::ResetReplay()
{
	ClearReplayMovementInput();
	ClearReplayLookInput();
	if (UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem())
	{
		ReplaySubsystem->UnloadReplay();
	}

	LoadedEpisodeDirectory.Reset();
	UpdateReplayEpisodeNumberText();
	if (ReplayImage)
	{
		ReplayImage->SetBrush(FSlateBrush());
	}
	if (ReplayFullscreenImage)
	{
		ReplayFullscreenImage->SetBrush(FSlateBrush());
	}
	ClearReplayEventMarkers();
	ClearReplayInterestRegions();
	SetReplayFullscreen(false);
	SetDiagnosticsText(TEXT("Replay stopped."));
	UpdateCameraModeText();
	UpdateReplayTimelineUi();
}

void UProjectEpisodeReplayViewerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ResolveCompactReplayWidgets();
	ResolveReplayInterestRegionWidgets();
	BindReplayInterestRegionStrip();

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		PlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (PlayButton)
	{
		PlayButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayClicked);
		PlayButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePauseClicked);
		PauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePauseClicked);
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

	if (CameraModeButton)
	{
		CameraModeButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked);
		CameraModeButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked);
	}

	if (!FullscreenButton)
	{
		FullscreenButton = Cast<UButton>(GetWidgetFromName(TEXT("FullscreenButton")));
	}
	if (FullscreenButton)
	{
		FullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
		FullscreenButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
	}

	BindReplayTimelineSlider(ReplayTimelineSlider);
	BindReplayTimelineSlider(ReplayCompactTimelineSlider);

	if (FullscreenPlayPauseButton)
	{
		FullscreenPlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		FullscreenPlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}
	if (!FullscreenPlayPauseImage)
	{
		FullscreenPlayPauseImage = Cast<UImage>(GetWidgetFromName(TEXT("FullscreenPlayPauseImage")));
	}

	if (FullscreenStopButton)
	{
		FullscreenStopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
		FullscreenStopButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (FullscreenResetButton)
	{
		FullscreenResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
		FullscreenResetButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (FullscreenTopDownCameraButton)
	{
		FullscreenTopDownCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked);
		FullscreenTopDownCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked);
	}

	if (FullscreenOrbitCameraButton)
	{
		FullscreenOrbitCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked);
		FullscreenOrbitCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked);
	}

	if (FullscreenFreeCameraButton)
	{
		FullscreenFreeCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked);
		FullscreenFreeCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked);
	}

	if (FullscreenVehicleFrontCameraButton)
	{
		FullscreenVehicleFrontCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked);
		FullscreenVehicleFrontCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked);
	}

	ResolveFullscreenLayerToggleWidgets();

	if (FullscreenMapToggleButton)
	{
		FullscreenMapToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked);
		FullscreenMapToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked);
	}

	if (FullscreenPointCloudToggleButton)
	{
		FullscreenPointCloudToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked);
		FullscreenPointCloudToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked);
	}

	if (FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked);
		FullscreenRayToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked);
	}

	if (ExitFullscreenButton)
	{
		ExitFullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked);
		ExitFullscreenButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked);
	}

	UpdateReplayFullscreenVisibility();
	UpdateCameraModeText();
	UpdateReplayTimelineUi();
	RebuildReplayEventMarkers();
	RebuildReplayInterestRegions();
}

void UProjectEpisodeReplayViewerWidget::RefreshReplayControlBindings()
{
	ResolveCompactReplayWidgets();
	ResolveReplayInterestRegionWidgets();
	BindReplayInterestRegionStrip();

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		PlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (PlayButton)
	{
		PlayButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayClicked);
		PlayButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePauseClicked);
		PauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePauseClicked);
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

	if (CameraModeButton)
	{
		CameraModeButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked);
		CameraModeButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked);
	}

	if (!FullscreenButton)
	{
		FullscreenButton = Cast<UButton>(GetWidgetFromName(TEXT("FullscreenButton")));
	}
	if (FullscreenButton)
	{
		FullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
		FullscreenButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
	}

	BindReplayTimelineSlider(ReplayTimelineSlider);
	BindReplayTimelineSlider(ReplayCompactTimelineSlider);

	if (FullscreenPlayPauseButton)
	{
		FullscreenPlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		FullscreenPlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}
	if (!FullscreenPlayPauseImage)
	{
		FullscreenPlayPauseImage = Cast<UImage>(GetWidgetFromName(TEXT("FullscreenPlayPauseImage")));
	}

	if (FullscreenStopButton)
	{
		FullscreenStopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
		FullscreenStopButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (FullscreenResetButton)
	{
		FullscreenResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
		FullscreenResetButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (FullscreenTopDownCameraButton)
	{
		FullscreenTopDownCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked);
		FullscreenTopDownCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked);
	}

	if (FullscreenOrbitCameraButton)
	{
		FullscreenOrbitCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked);
		FullscreenOrbitCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked);
	}

	if (FullscreenFreeCameraButton)
	{
		FullscreenFreeCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked);
		FullscreenFreeCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked);
	}

	if (FullscreenVehicleFrontCameraButton)
	{
		FullscreenVehicleFrontCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked);
		FullscreenVehicleFrontCameraButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked);
	}

	ResolveFullscreenLayerToggleWidgets();

	if (FullscreenMapToggleButton)
	{
		FullscreenMapToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked);
		FullscreenMapToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked);
	}

	if (FullscreenPointCloudToggleButton)
	{
		FullscreenPointCloudToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked);
		FullscreenPointCloudToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked);
	}

	if (FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked);
		FullscreenRayToggleButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked);
	}

	if (ExitFullscreenButton)
	{
		ExitFullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked);
		ExitFullscreenButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked);
	}
}

void UProjectEpisodeReplayViewerWidget::SetExternalReplayInterestRegionStrip(
	UProjectEpisodeReplayInterestRegionStripWidget* InterestRegionStrip)
{
	if (ExternalReplayInterestRegionStrip.Get() == InterestRegionStrip
		&& ReplayInterestRegionStrip.Get() == InterestRegionStrip)
	{
		return;
	}

	UnbindReplayInterestRegionStrip();
	ExternalReplayInterestRegionStrip = InterestRegionStrip;
	ReplayInterestRegionStrip = nullptr;
	ResolveReplayInterestRegionWidgets();
	BindReplayInterestRegionStrip();
	RebuildReplayInterestRegions();
	UpdateReplayInterestRegionSelection(false);
}

void UProjectEpisodeReplayViewerWidget::NativeDestruct()
{
	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (PlayButton)
	{
		PlayButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePauseClicked);
	}

	if (StopButton)
	{
		StopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (CameraModeButton)
	{
		CameraModeButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked);
	}

	if (FullscreenButton)
	{
		FullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
	}

	UnbindReplayTimelineSlider(ReplayTimelineSlider);
	UnbindReplayTimelineSlider(ReplayCompactTimelineSlider);

	if (FullscreenPlayPauseButton)
	{
		FullscreenPlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
	}

	if (FullscreenStopButton)
	{
		FullscreenStopButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleStopClicked);
	}

	if (FullscreenResetButton)
	{
		FullscreenResetButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleResetClicked);
	}

	if (FullscreenTopDownCameraButton)
	{
		FullscreenTopDownCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked);
	}

	if (FullscreenOrbitCameraButton)
	{
		FullscreenOrbitCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked);
	}

	if (FullscreenFreeCameraButton)
	{
		FullscreenFreeCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked);
	}

	if (FullscreenVehicleFrontCameraButton)
	{
		FullscreenVehicleFrontCameraButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked);
	}

	if (FullscreenMapToggleButton)
	{
		FullscreenMapToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked);
	}

	if (FullscreenPointCloudToggleButton)
	{
		FullscreenPointCloudToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked);
	}

	if (FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked);
	}

	if (ExitFullscreenButton)
	{
		ExitFullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked);
	}

	UnbindReplayInterestRegionStrip();
	ResetReplay();
	ExternalReplayInterestRegionStrip = nullptr;
	ReplayInterestRegionStrip = nullptr;
	Super::NativeDestruct();
}

void UProjectEpisodeReplayViewerWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateReplayTimelineUi();

	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!CanUseReplayCameraInput() || !ReplaySubsystem)
	{
		ClearReplayMovementInput();
		ClearReplayLookInput();
		return;
	}

	if (ReplaySubsystem->GetReplayCameraMode() != EScenarioReplayCameraMode::Free)
	{
		ClearReplayMovementInput();
		if (ReplaySubsystem->GetReplayCameraMode() != EScenarioReplayCameraMode::Orbit)
		{
			ClearReplayLookInput();
		}
		return;
	}

	ReplaySubsystem->AddFreeCameraMovement(
		BuildFreeCameraInput(),
		InDeltaTime);
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	RequestReplayInputFocus();

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& CanUseReplayCameraLook())
	{
		bFreeCameraLookHeld = true;
		return FReply::Handled();
	}

	return FReply::Handled();
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		ClearReplayLookInput();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bFreeCameraLookHeld && CanUseReplayCameraLook())
	{
		if (UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem())
		{
			if (ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::Orbit)
			{
				ReplaySubsystem->AddOrbitCameraLook(InMouseEvent.GetCursorDelta());
			}
			else
			{
				ReplaySubsystem->AddFreeCameraLook(InMouseEvent.GetCursorDelta());
			}
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!CanUseReplayCameraInput() || !ReplaySubsystem)
	{
		ClearReplayMovementInput();
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();
	const EScenarioReplayCameraMode CameraMode =
		ReplaySubsystem->GetReplayCameraMode();
	const bool bFreeCameraMode = CameraMode == EScenarioReplayCameraMode::Free;
	const bool bTopDownCameraMode = CameraMode == EScenarioReplayCameraMode::TopDown;

	if (Key == EKeys::W)
	{
		if (bFreeCameraMode)
		{
			bMoveForwardHeld = true;
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::S)
	{
		if (bFreeCameraMode)
		{
			bMoveBackwardHeld = true;
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::A)
	{
		if (bFreeCameraMode)
		{
			bMoveLeftHeld = true;
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::D)
	{
		if (bFreeCameraMode)
		{
			bMoveRightHeld = true;
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::E)
	{
		if (bFreeCameraMode)
		{
			bMoveUpHeld = true;
			return FReply::Handled();
		}
		if (bTopDownCameraMode)
		{
			ReplaySubsystem->AddTopDownZoom(1.0f);
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::Q)
	{
		if (bFreeCameraMode)
		{
			bMoveDownHeld = true;
			return FReply::Handled();
		}
		if (bTopDownCameraMode)
		{
			ReplaySubsystem->AddTopDownZoom(-1.0f);
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnKeyUp(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::W)
	{
		bMoveForwardHeld = false;
		return FReply::Handled();
	}
	if (Key == EKeys::S)
	{
		bMoveBackwardHeld = false;
		return FReply::Handled();
	}
	if (Key == EKeys::A)
	{
		bMoveLeftHeld = false;
		return FReply::Handled();
	}
	if (Key == EKeys::D)
	{
		bMoveRightHeld = false;
		return FReply::Handled();
	}
	if (Key == EKeys::E)
	{
		bMoveUpHeld = false;
		return FReply::Handled();
	}
	if (Key == EKeys::Q)
	{
		bMoveDownHeld = false;
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UProjectEpisodeReplayViewerWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!CanUseReplayCameraInput() || !ReplaySubsystem)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	if (ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::TopDown)
	{
		ReplaySubsystem->AddTopDownZoom(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	if (ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::Orbit)
	{
		ReplaySubsystem->AddOrbitCameraZoom(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UProjectEpisodeReplayViewerWidget::NativeOnFocusLost(
	const FFocusEvent& InFocusEvent)
{
	ClearReplayMovementInput();
	ClearReplayLookInput();
	Super::NativeOnFocusLost(InFocusEvent);
}

void UProjectEpisodeReplayViewerWidget::HandlePlayClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	ReplaySubsystem->Play();
	SetDiagnosticsText(TEXT("Replay playing."));
	UpdateReplayTimelineUi();
	RequestReplayInputFocus();
}

void UProjectEpisodeReplayViewerWidget::HandlePauseClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	ReplaySubsystem->Pause();
	ClearReplayMovementInput();
	ClearReplayLookInput();
	SetDiagnosticsText(TEXT("Replay paused."));
	UpdateReplayTimelineUi();
	RequestReplayInputFocus();
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
		ClearReplayMovementInput();
		ClearReplayLookInput();
		SetDiagnosticsText(TEXT("Replay paused."));
		UpdateReplayTimelineUi();
		RequestReplayInputFocus();
		return;
	}

	ReplaySubsystem->Play();
	SetDiagnosticsText(TEXT("Replay playing."));
	UpdateReplayTimelineUi();
	RequestReplayInputFocus();
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
	ClearReplayMovementInput();
	ClearReplayLookInput();
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
	ClearReplayMovementInput();
	ClearReplayLookInput();
	SetDiagnosticsText(TEXT("Replay reset to first frame."));
}

void UProjectEpisodeReplayViewerWidget::HandleCameraModeClicked()
{
	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	ApplyReplayCameraMode(
		GetNextReplayCameraMode(ReplaySubsystem->GetReplayCameraMode()));
}

void UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked()
{
	SetReplayFullscreen(true);
}

void UProjectEpisodeReplayViewerWidget::HandleExitFullscreenClicked()
{
	SetReplayFullscreen(false);
}

void UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged(float Value)
{
	if (bUpdatingReplayTimelineSlider)
	{
		return;
	}

	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem || ReplaySubsystem->GetDurationSeconds() <= 0.0)
	{
		return;
	}

	const double NormalizedValue = FMath::Clamp(
		static_cast<double>(Value),
		0.0,
		1.0);
	const double RequestedTimeSeconds =
		ReplaySubsystem->GetDurationSeconds() * NormalizedValue;

	double SnapEventTimeSeconds = 0.0;
	int32 SnapEventIndex = INDEX_NONE;
	const bool bShouldSnap = TryFindTimelineSnapEvent(
		RequestedTimeSeconds,
		SnapEventTimeSeconds,
		SnapEventIndex);
	const double SeekTimeSeconds = bShouldSnap
		? SnapEventTimeSeconds
		: RequestedTimeSeconds;

	bTimelineSnappedToEvent = bShouldSnap;
	SnappedEventTimeSeconds = bShouldSnap ? SnapEventTimeSeconds : 0.0;
	SnappedEventIndex = bShouldSnap ? SnapEventIndex : INDEX_NONE;

	ReplaySubsystem->Seek(SeekTimeSeconds);
	if (bShouldSnap)
	{
		SetReplayTimelineSliderValues(static_cast<float>(
			FMath::Clamp(
				SeekTimeSeconds / ReplaySubsystem->GetDurationSeconds(),
				0.0,
				1.0)));
		FocusReplayInterestEvent(SnapEventIndex);
	}
	UpdateReplayTimelineUi();
	RequestReplayInputFocus();
}

void UProjectEpisodeReplayViewerWidget::HandleReplayTimelineMouseCaptureEnd()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		bTimelineSnappedToEvent = false;
		SnappedEventTimeSeconds = 0.0;
		SnappedEventIndex = INDEX_NONE;
		return;
	}

	if (bTimelineSnappedToEvent)
	{
		ReplaySubsystem->Seek(SnappedEventTimeSeconds);
		ReplaySubsystem->Pause();
		ClearReplayMovementInput();
		ClearReplayLookInput();
		SetDiagnosticsText(FString::Printf(
			TEXT("Replay stopped at event %.2fs."),
			SnappedEventTimeSeconds));
		FocusReplayInterestEvent(SnappedEventIndex);
		UpdateReplayTimelineUi();
	}

	bTimelineSnappedToEvent = false;
	SnappedEventTimeSeconds = 0.0;
	SnappedEventIndex = INDEX_NONE;
	RequestReplayInputFocus();
}

void UProjectEpisodeReplayViewerWidget::HandleTopDownCameraClicked()
{
	ApplyReplayCameraMode(EScenarioReplayCameraMode::TopDown);
}

void UProjectEpisodeReplayViewerWidget::HandleOrbitCameraClicked()
{
	ApplyReplayCameraMode(EScenarioReplayCameraMode::Orbit);
}

void UProjectEpisodeReplayViewerWidget::HandleFreeCameraClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (!ReplaySubsystem->HasLoadedReplayFrames())
	{
		SetDiagnosticsText(TEXT("Replay camera is unavailable until replay frames are loaded."));
		return;
	}

	if (ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::Free)
	{
		ClearReplayMovementInput();
		ClearReplayLookInput();
		if (ReplaySubsystem->FocusFreeCameraOnReplayRobot())
		{
			SetDiagnosticsText(TEXT("Replay free camera refocused on robot."));
		}
		UpdateCameraModeText();
		RequestReplayInputFocus();
		return;
	}

	ApplyReplayCameraMode(EScenarioReplayCameraMode::Free);
}

void UProjectEpisodeReplayViewerWidget::HandleVehicleFrontCameraClicked()
{
	ApplyReplayCameraMode(EScenarioReplayCameraMode::VehicleFront);
}

// Toggles replay scenario map visibility in fullscreen mode.
void UProjectEpisodeReplayViewerWidget::HandleFullscreenMapToggleClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	const bool bNewVisible = !ReplaySubsystem->IsReplayMapVisible();
	ReplaySubsystem->SetReplayMapVisible(bNewVisible);
	SetDiagnosticsText(bNewVisible
		? TEXT("Replay map visible.")
		: TEXT("Replay map hidden."));
	RequestReplayInputFocus();
}

// Toggles replay point cloud visibility in fullscreen mode.
void UProjectEpisodeReplayViewerWidget::HandleFullscreenPointCloudToggleClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (!ReplaySubsystem->HasReplayPointCloud())
	{
		SetDiagnosticsText(TEXT("Replay point cloud is unavailable."));
		RequestReplayInputFocus();
		return;
	}

	const bool bNewVisible = !ReplaySubsystem->IsReplayPointCloudVisible();
	ReplaySubsystem->SetReplayPointCloudVisible(bNewVisible);
	SetDiagnosticsText(bNewVisible
		? TEXT("Replay point cloud visible.")
		: TEXT("Replay point cloud hidden."));
	RequestReplayInputFocus();
}

// Toggles replay LiDAR ray visibility in fullscreen mode.
void UProjectEpisodeReplayViewerWidget::HandleFullscreenRayToggleClicked()
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (!ReplaySubsystem->HasReplayLidarRays())
	{
		SetDiagnosticsText(TEXT("Replay LiDAR rays are unavailable."));
		RequestReplayInputFocus();
		return;
	}

	const bool bNewVisible = !ReplaySubsystem->IsReplayLidarRaysVisible();
	ReplaySubsystem->SetReplayLidarRaysVisible(bNewVisible);
	SetDiagnosticsText(bNewVisible
		? TEXT("Replay LiDAR rays visible.")
		: TEXT("Replay LiDAR rays hidden."));
	RequestReplayInputFocus();
}

void UProjectEpisodeReplayViewerWidget::HandleReplayInterestEventSelected(
	UProjectEpisodeReplayInterestRegionStripWidget* InterestStrip,
	double TimeSeconds)
{
	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (!ReplaySubsystem->HasLoadedReplayFrames())
	{
		SetDiagnosticsText(TEXT("Replay event selection is unavailable until replay frames are loaded."));
		return;
	}

	ReplaySubsystem->Seek(TimeSeconds);
	ReplaySubsystem->Pause();
	ClearReplayMovementInput();
	ClearReplayLookInput();
	UpdateReplayTimelineUi();
	UpdateReplayInterestRegionSelection(true);
	SetDiagnosticsText(FString::Printf(
		TEXT("Replay stopped at event %.2fs."),
		TimeSeconds));
	RequestReplayInputFocus();
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
	if (!ReplaySubsystem)
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
	if (ReplayImage)
	{
		ReplayImage->SetBrush(ReplayBrush);
	}
	if (ReplayFullscreenImage)
	{
		ReplayFullscreenImage->SetBrush(ReplayBrush);
	}
}

void UProjectEpisodeReplayViewerWidget::SetReplayFullscreen(
	bool bNewFullscreen)
{
	if (bReplayFullscreen == bNewFullscreen)
	{
		UpdateReplayFullscreenVisibility();
		OnReplayFullscreenChanged.Broadcast(this, bReplayFullscreen);
		RequestReplayInputFocus();
		return;
	}

	bReplayFullscreen = bNewFullscreen;
	ClearReplayMovementInput();
	ClearReplayLookInput();
	UpdateReplayFullscreenVisibility();
	ApplyReplayRenderTarget();
	UpdateCameraModeText();
	UpdateReplayTimelineUi();
	OnReplayFullscreenChanged.Broadcast(this, bReplayFullscreen);
	RequestReplayInputFocus();
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayFullscreenVisibility()
{
	ResolveCompactReplayWidgets();

	if (bReplayFullscreen)
	{
		ApplyReplayFullscreenLayout();
	}

	if (ReplayFullscreenLayer)
	{
		ReplayFullscreenLayer->SetVisibility(
			bReplayFullscreen
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	if (ReplayControlBar)
	{
		ReplayControlBar->SetVisibility(
			bReplayFullscreen
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}

	const ESlateVisibility CompactTelemetryVisibility =
		bReplayFullscreen
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible;
	for (UWidget* CompactTelemetryWidget : {
		ReplayCompactDriveTelemetryRow.Get(),
		ReplayCompactSpeedPill.Get(),
		ReplayCompactTargetSpeedPill.Get(),
		ReplayCompactSteeringPill.Get(),
		ReplayCompactBrakePill.Get() })
	{
		if (CompactTelemetryWidget)
		{
			CompactTelemetryWidget->SetVisibility(CompactTelemetryVisibility);
		}
	}

	if (ExitFullscreenButton)
	{
		ExitFullscreenButton->SetVisibility(
			bReplayFullscreen
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}

void UProjectEpisodeReplayViewerWidget::ApplyReplayFullscreenLayout()
{
	ApplyReplayFillSlot(ReplayFullscreenLayer.Get(), ReplayFullscreenLayerZOrder);
	InvalidateLayoutAndVolatility();
}

// Finds fullscreen layer toggle buttons by name when they are not exposed as WBP variables.
void UProjectEpisodeReplayViewerWidget::ResolveFullscreenLayerToggleWidgets()
{
	if (!FullscreenMapToggleButton)
	{
		FullscreenMapToggleButton = Cast<UButton>(GetWidgetFromName(TEXT("FullscreenMapToggleButton")));
	}

	if (!FullscreenPointCloudToggleButton)
	{
		FullscreenPointCloudToggleButton =
			Cast<UButton>(GetWidgetFromName(TEXT("FullscreenPointCloudToggleButton")));
	}

	if (!FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton =
			Cast<UButton>(GetWidgetFromName(TEXT("FullscreenRayToggleButton")));
	}

	if (!ExitFullscreenButton)
	{
		static constexpr const TCHAR* ExitFullscreenButtonNames[] = {
			TEXT("ExitFullscreenButton"),
			TEXT("FullscreenExitButton"),
			TEXT("ReplayExitFullscreenButton"),
			TEXT("ReplayFullscreenExitButton")
		};

		for (const TCHAR* ButtonName : ExitFullscreenButtonNames)
		{
			ExitFullscreenButton = Cast<UButton>(GetWidgetFromName(ButtonName));
			if (ExitFullscreenButton)
			{
				break;
			}
		}
	}
}

void UProjectEpisodeReplayViewerWidget::ResolveCompactReplayWidgets()
{
	if (!ReplayControlBar)
	{
		ReplayControlBar = Cast<UBorder>(GetWidgetFromName(TEXT("ReplayControlBar")));
	}

	if (!PlayPauseImage)
	{
		PlayPauseImage = Cast<UImage>(GetWidgetFromName(TEXT("PlayPauseImage")));
	}

	if (!CameraModeImage)
	{
		CameraModeImage = Cast<UImage>(GetWidgetFromName(TEXT("CameraModeImage")));
	}

	if (!ReplayCompactTimelineSlider)
	{
		ReplayCompactTimelineSlider =
			Cast<USlider>(GetWidgetFromName(TEXT("ReplayCompactTimelineSlider")));
	}

	if (!ReplayCompactTimelineMarkerCanvas)
	{
		ReplayCompactTimelineMarkerCanvas =
			Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ReplayCompactTimelineMarkerCanvas")));
	}
}

void UProjectEpisodeReplayViewerWidget::ResolveReplayInterestRegionWidgets()
{
	if (ExternalReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip = ExternalReplayInterestRegionStrip;
		return;
	}

	if (ReplayInterestRegionStrip)
	{
		return;
	}

	ReplayInterestRegionStrip =
		Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
			GetWidgetFromName(TEXT("ReplayInterestRegionStrip")));

	if (!ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip =
			Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
				GetWidgetFromName(TEXT("InterestRegionStrip")));
	}

	if (!ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip =
			Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
				GetWidgetFromName(TEXT("WBP_ReplayInterestRegionStrip")));
	}
}

void UProjectEpisodeReplayViewerWidget::BindReplayInterestRegionStrip()
{
	if (!ReplayInterestRegionStrip)
	{
		return;
	}

	ReplayInterestRegionStrip->OnInterestEventSelected.RemoveAll(this);
	ReplayInterestRegionStrip->OnInterestEventSelected.AddUObject(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayInterestEventSelected);
}

void UProjectEpisodeReplayViewerWidget::UnbindReplayInterestRegionStrip()
{
	if (ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip->OnInterestEventSelected.RemoveAll(this);
	}
}

void UProjectEpisodeReplayViewerWidget::BindReplayTimelineSlider(
	USlider* TimelineSlider)
{
	if (!TimelineSlider)
	{
		return;
	}

	TimelineSlider->OnValueChanged.RemoveDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
	TimelineSlider->OnValueChanged.AddDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
	TimelineSlider->OnMouseCaptureEnd.RemoveDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineMouseCaptureEnd);
	TimelineSlider->OnMouseCaptureEnd.AddDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineMouseCaptureEnd);
}

void UProjectEpisodeReplayViewerWidget::UnbindReplayTimelineSlider(
	USlider* TimelineSlider)
{
	if (!TimelineSlider)
	{
		return;
	}

	TimelineSlider->OnValueChanged.RemoveDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
	TimelineSlider->OnMouseCaptureEnd.RemoveDynamic(
		this,
		&UProjectEpisodeReplayViewerWidget::HandleReplayTimelineMouseCaptureEnd);
}

void UProjectEpisodeReplayViewerWidget::ApplyReplayCameraMode(
	EScenarioReplayCameraMode NewMode)
{
	ClearReplayMovementInput();
	ClearReplayLookInput();

	UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioReplaySubsystem is unavailable."));
		return;
	}

	if (!ReplaySubsystem->HasLoadedReplayFrames())
	{
		SetDiagnosticsText(TEXT("Replay camera is unavailable until replay frames are loaded."));
		return;
	}

	ReplaySubsystem->SetReplayCameraMode(NewMode);
	UpdateCameraModeText();
	SetDiagnosticsText(FString::Printf(
		TEXT("Replay camera: %s."),
		*GetReplayCameraModeLabel(NewMode).ToString()));
	RequestReplayInputFocus();
}

EScenarioReplayCameraMode UProjectEpisodeReplayViewerWidget::GetNextReplayCameraMode(
	EScenarioReplayCameraMode CurrentMode) const
{
	switch (CurrentMode)
	{
	case EScenarioReplayCameraMode::TopDown:
		return EScenarioReplayCameraMode::Orbit;

	case EScenarioReplayCameraMode::Orbit:
		return EScenarioReplayCameraMode::VehicleFront;

	case EScenarioReplayCameraMode::VehicleFront:
		return EScenarioReplayCameraMode::Free;

	case EScenarioReplayCameraMode::Free:
	default:
		return EScenarioReplayCameraMode::TopDown;
	}
}

FText UProjectEpisodeReplayViewerWidget::GetReplayCameraModeLabel(
	EScenarioReplayCameraMode CameraMode) const
{
	switch (CameraMode)
	{
	case EScenarioReplayCameraMode::Orbit:
		return FText::FromString(TEXT("Orbit"));

	case EScenarioReplayCameraMode::Free:
		return FText::FromString(TEXT("Free"));

	case EScenarioReplayCameraMode::VehicleFront:
		return FText::FromString(TEXT("Vehicle Front"));

	case EScenarioReplayCameraMode::TopDown:
	default:
		return FText::FromString(TEXT("Top View"));
	}
}

UTexture2D* UProjectEpisodeReplayViewerWidget::GetReplayCameraModeIcon(
	EScenarioReplayCameraMode CameraMode) const
{
	switch (CameraMode)
	{
	case EScenarioReplayCameraMode::Orbit:
		return ReplayThirdPersonCameraIconTexture.Get()
			? ReplayThirdPersonCameraIconTexture.Get()
			: ReplayTopDownCameraIconTexture.Get();

	case EScenarioReplayCameraMode::Free:
		return ReplayFreeCameraIconTexture.Get()
			? ReplayFreeCameraIconTexture.Get()
			: ReplayTopDownCameraIconTexture.Get();

	case EScenarioReplayCameraMode::VehicleFront:
		return ReplayFirstPersonCameraIconTexture.Get()
			? ReplayFirstPersonCameraIconTexture.Get()
			: ReplayTopDownCameraIconTexture.Get();

	case EScenarioReplayCameraMode::TopDown:
	default:
		return ReplayTopDownCameraIconTexture.Get();
	}
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayCameraModeIcon(
	EScenarioReplayCameraMode CameraMode)
{
	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	const bool bHasReplayFrames =
		ReplaySubsystem
			? ReplaySubsystem->HasLoadedReplayFrames()
			: false;
	ApplyReplayIconBrush(
		CameraModeImage.Get(),
		GetReplayCameraModeIcon(CameraMode),
		ReplayCameraIconSize,
		bHasReplayFrames);
}

void UProjectEpisodeReplayViewerWidget::UpdateCameraModeText()
{
	if (!CameraModeText && !FullscreenCameraModeText && !CameraModeImage)
	{
		return;
	}

	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	const EScenarioReplayCameraMode CameraMode =
		ReplaySubsystem
			? ReplaySubsystem->GetReplayCameraMode()
			: EScenarioReplayCameraMode::TopDown;
	const FText CameraText = FText::Format(
		FText::FromString(TEXT("Camera: {0}")),
		GetReplayCameraModeLabel(CameraMode));
	if (CameraModeText)
	{
		CameraModeText->SetText(CameraText);
	}
	if (FullscreenCameraModeText)
	{
		FullscreenCameraModeText->SetText(CameraText);
	}
	UpdateReplayCameraModeIcon(CameraMode);
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayTimelineUi()
{
	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	const double CurrentTimeSeconds =
		ReplaySubsystem
			? ReplaySubsystem->GetCurrentReplayTimeSeconds()
			: 0.0;
	const double DurationSeconds =
		ReplaySubsystem
			? ReplaySubsystem->GetDurationSeconds()
			: 0.0;
	const bool bHasReplayFrames =
		ReplaySubsystem
			? ReplaySubsystem->HasLoadedReplayFrames()
			: false;
	const bool bReplayPlaying =
		ReplaySubsystem
			&& ReplaySubsystem->GetPlaybackState() == EScenarioReplayPlaybackState::Playing;
	const double RobotSpeedKmh =
		ReplaySubsystem
			? ReplaySubsystem->GetCurrentRobotSpeedKmh()
			: 0.0;
	const double RobotTargetSpeedKmh =
		ReplaySubsystem
			? ReplaySubsystem->GetCurrentRobotTargetSpeedKmh()
			: 0.0;
	const double RobotSteering =
		ReplaySubsystem
			? FMath::Clamp(ReplaySubsystem->GetCurrentRobotSteering(), -1.0, 1.0)
			: 0.0;
	const double RobotBrake =
		ReplaySubsystem
			? FMath::Clamp(ReplaySubsystem->GetCurrentRobotBrake(), 0.0, 1.0)
			: 0.0;

	if (ReplayTimeText)
	{
		ReplayTimeText->SetText(FText::Format(
			FText::FromString(TEXT("Time: {0} / {1}")),
			FormatReplayTime(CurrentTimeSeconds),
			FormatReplayTime(DurationSeconds)));
	}

	if (ReplayFrameText)
	{
		const int32 CurrentFrameIndex =
			ReplaySubsystem
				? ReplaySubsystem->GetCurrentFrameIndex()
				: INDEX_NONE;
		const int32 FrameCount =
			ReplaySubsystem
				? ReplaySubsystem->GetFrameCount()
				: 0;
		ReplayFrameText->SetText(FText::FromString(FString::Printf(
			TEXT("Frame: %d / %d"),
			CurrentFrameIndex >= 0 ? CurrentFrameIndex + 1 : 0,
			FrameCount)));
	}

	if (ReplaySpeedText)
	{
		const double PlaybackSpeed =
			ReplaySubsystem
				? ReplaySubsystem->GetPlaybackSpeed()
				: 1.0;
		ReplaySpeedText->SetText(FText::FromString(FString::Printf(
			TEXT("Speed: %.1f km/h | %.2fx"),
			RobotSpeedKmh,
			PlaybackSpeed)));
	}

	if (ReplaySpeedValueText)
	{
		ReplaySpeedValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f km/h"),
			RobotSpeedKmh)));
	}

	if (ReplayTargetSpeedValueText)
	{
		ReplayTargetSpeedValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f km/h"),
			RobotTargetSpeedKmh)));
	}

	if (ReplaySteeringValueText)
	{
		ReplaySteeringValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%+.2f"),
			RobotSteering)));
	}

	if (ReplayBrakeValueText)
	{
		ReplayBrakeValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"),
			RobotBrake * 100.0)));
	}

	if (ReplayCompactSpeedValueText)
	{
		ReplayCompactSpeedValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f km/h"),
			RobotSpeedKmh)));
	}

	if (ReplayCompactThrottleValueText)
	{
		const double RobotThrottle =
			ReplaySubsystem
				? FMath::Clamp(ReplaySubsystem->GetCurrentRobotThrottle(), 0.0, 1.0)
				: 0.0;
		ReplayCompactThrottleValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"),
			RobotThrottle * 100.0)));
	}

	if (ReplayCompactTargetSpeedValueText)
	{
		ReplayCompactTargetSpeedValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f km/h"),
			RobotTargetSpeedKmh)));
	}

	if (ReplayCompactSteeringValueText)
	{
		ReplayCompactSteeringValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%+.2f"),
			RobotSteering)));
	}

	if (ReplayCompactBrakeValueText)
	{
		ReplayCompactBrakeValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"),
			RobotBrake * 100.0)));
	}

	if (ReplayPlaybackRateText)
	{
		const double PlaybackSpeed =
			ReplaySubsystem
				? ReplaySubsystem->GetPlaybackSpeed()
				: 1.0;
		ReplayPlaybackRateText->SetText(FText::FromString(FString::Printf(
			TEXT("%.2fx"),
			PlaybackSpeed)));
	}

	if (ReplayPositionText)
	{
		const FVector RobotPositionM =
			ReplaySubsystem
				? ReplaySubsystem->GetCurrentRobotPositionCm() * 0.01
				: FVector::ZeroVector;
		ReplayPositionText->SetText(FText::FromString(FString::Printf(
			TEXT("Pos: X %.2fm Y %.2fm Z %.2fm"),
			RobotPositionM.X,
			RobotPositionM.Y,
			RobotPositionM.Z)));
	}

	const FVector RobotPositionM =
		ReplaySubsystem
			? ReplaySubsystem->GetCurrentRobotPositionCm() * 0.01
			: FVector::ZeroVector;
	if (ReplayPositionXText)
	{
		ReplayPositionXText->SetText(FText::FromString(FString::Printf(
			TEXT("X %.2fm"),
			RobotPositionM.X)));
	}
	if (ReplayPositionYText)
	{
		ReplayPositionYText->SetText(FText::FromString(FString::Printf(
			TEXT("Y %.2fm"),
			RobotPositionM.Y)));
	}
	if (ReplayPositionZText)
	{
		ReplayPositionZText->SetText(FText::FromString(FString::Printf(
			TEXT("Z %.2fm"),
			RobotPositionM.Z)));
	}

	SetReplayTimelineSliderValues(
		ReplaySubsystem
			? static_cast<float>(ReplaySubsystem->GetPlaybackProgress())
			: 0.0f);
	UpdateReplayInterestRegionSelection(false);

	if (PlayPauseButton)
	{
		PlayPauseButton->SetIsEnabled(bHasReplayFrames);
	}
	if (PlayButton)
	{
		PlayButton->SetIsEnabled(bHasReplayFrames && !bReplayPlaying);
	}
	if (PauseButton)
	{
		PauseButton->SetIsEnabled(bHasReplayFrames && bReplayPlaying);
	}
	if (ResetButton)
	{
		ResetButton->SetIsEnabled(bHasReplayFrames);
	}
	if (ReplayTimelineSlider)
	{
		ReplayTimelineSlider->SetIsEnabled(bHasReplayFrames);
	}
	if (ReplayCompactTimelineSlider)
	{
		ReplayCompactTimelineSlider->SetIsEnabled(bHasReplayFrames);
	}
	if (FullscreenPlayPauseButton)
	{
		FullscreenPlayPauseButton->SetIsEnabled(bHasReplayFrames);
	}
	if (FullscreenResetButton)
	{
		FullscreenResetButton->SetIsEnabled(bHasReplayFrames);
	}
	UpdateReplayPlaybackIcon(bReplayPlaying, bHasReplayFrames);

	if (FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton->SetIsEnabled(
			ReplaySubsystem && ReplaySubsystem->HasReplayLidarRays());
	}
}

void UProjectEpisodeReplayViewerWidget::SetReplayTimelineSliderValues(
	float NormalizedValue)
{
	const float SafeValue = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
	bUpdatingReplayTimelineSlider = true;
	if (ReplayTimelineSlider)
	{
		ReplayTimelineSlider->SetValue(SafeValue);
	}
	if (ReplayCompactTimelineSlider)
	{
		ReplayCompactTimelineSlider->SetValue(SafeValue);
	}
	bUpdatingReplayTimelineSlider = false;
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayPlaybackIcon(
	bool bReplayPlaying,
	bool bHasReplayFrames)
{
	UTexture2D* DesiredIcon = bReplayPlaying
		? ReplayPauseIconTexture.Get()
		: ReplayPlayIconTexture.Get();
	if (!DesiredIcon)
	{
		return;
	}

	ApplyReplayIconBrush(
		PlayPauseImage.Get(),
		DesiredIcon,
		ReplayPlaybackIconSize,
		bHasReplayFrames);
	ApplyReplayIconBrush(
		FullscreenPlayPauseImage.Get(),
		DesiredIcon,
		ReplayPlaybackIconSize,
		bHasReplayFrames);
}

void UProjectEpisodeReplayViewerWidget::RebuildReplayEventMarkers()
{
	ClearReplayEventMarkers();

	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem
		|| ReplaySubsystem->GetDurationSeconds() <= 0.0)
	{
		return;
	}

	AddReplayEventMarkersToCanvas(ReplayTimelineMarkerCanvas, *ReplaySubsystem);
	AddReplayEventMarkersToCanvas(ReplayCompactTimelineMarkerCanvas, *ReplaySubsystem);
}

void UProjectEpisodeReplayViewerWidget::AddReplayEventMarkersToCanvas(
	UCanvasPanel* MarkerCanvas,
	const UScenarioReplaySubsystem& ReplaySubsystem)
{
	if (!MarkerCanvas || ReplaySubsystem.GetDurationSeconds() <= 0.0)
	{
		return;
	}

	const double DurationSeconds = ReplaySubsystem.GetDurationSeconds();
	for (const FScenarioReplayEventMarker& Marker : ReplaySubsystem.GetReplayEventMarkers())
	{
		if (Marker.TimeSeconds < 0.0 || Marker.TimeSeconds > DurationSeconds)
		{
			continue;
		}

		UBorder* MarkerWidget = NewObject<UBorder>(this);
		if (!MarkerWidget)
		{
			continue;
		}

		const double NormalizedTime = FMath::Clamp(
			Marker.TimeSeconds / DurationSeconds,
			0.0,
			1.0);
		MarkerWidget->SetBrushColor(GetReplayEventMarkerColor(Marker.EventType));
		MarkerWidget->SetToolTipText(BuildReplayEventMarkerTooltip(Marker));
		MarkerWidget->SetVisibility(ESlateVisibility::Visible);

		if (UCanvasPanelSlot* MarkerSlot =
			MarkerCanvas->AddChildToCanvas(MarkerWidget))
		{
			MarkerSlot->SetAnchors(FAnchors(
				static_cast<float>(NormalizedTime),
				0.5f,
				static_cast<float>(NormalizedTime),
				0.5f));
			MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MarkerSlot->SetPosition(FVector2D::ZeroVector);
			MarkerSlot->SetSize(ReplayEventMarkerSize);
			MarkerSlot->SetZOrder(1);
		}
	}
}

void UProjectEpisodeReplayViewerWidget::ClearReplayEventMarkers()
{
	ClearReplayMarkerCanvas(ReplayTimelineMarkerCanvas.Get());
	ClearReplayMarkerCanvas(ReplayCompactTimelineMarkerCanvas.Get());

	bTimelineSnappedToEvent = false;
	SnappedEventTimeSeconds = 0.0;
	SnappedEventIndex = INDEX_NONE;
}

void UProjectEpisodeReplayViewerWidget::RebuildReplayInterestRegions()
{
	ResolveReplayInterestRegionWidgets();
	if (!ReplayInterestRegionStrip)
	{
		return;
	}

	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem
		|| ReplaySubsystem->GetReplayEventMarkers().IsEmpty())
	{
		ReplayInterestRegionStrip->ClearEventMarkers();
		return;
	}

	ReplayInterestRegionStrip->SetEventMarkers(
		ReplaySubsystem->GetReplayEventMarkers(),
		ReplaySubsystem->GetCurrentReplayTimeSeconds());
}

void UProjectEpisodeReplayViewerWidget::ClearReplayInterestRegions()
{
	if (ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip->ClearEventMarkers();
	}
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayInterestRegionSelection(
	bool bScrollSelectedIntoView)
{
	if (!ReplayInterestRegionStrip)
	{
		return;
	}

	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem || !ReplaySubsystem->HasLoadedReplayFrames())
	{
		ReplayInterestRegionStrip->SetCurrentTime(0.0, false);
		return;
	}

	ReplayInterestRegionStrip->SetCurrentTime(
		ReplaySubsystem->GetCurrentReplayTimeSeconds(),
		bScrollSelectedIntoView);
}

void UProjectEpisodeReplayViewerWidget::FocusReplayInterestEvent(int32 EventIndex)
{
	if (ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip->FocusEventByIndex(EventIndex);
	}
}

bool UProjectEpisodeReplayViewerWidget::TryFindTimelineSnapEvent(
	double TimeSeconds,
	double& OutEventTimeSeconds,
	int32& OutEventIndex) const
{
	OutEventTimeSeconds = 0.0;
	OutEventIndex = INDEX_NONE;

	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	if (!ReplaySubsystem || TimelineEventSnapThresholdSeconds <= 0.0)
	{
		return false;
	}

	double BestDistanceSeconds = TimelineEventSnapThresholdSeconds;
	for (const FScenarioReplayEventMarker& Marker : ReplaySubsystem->GetReplayEventMarkers())
	{
		const double DistanceSeconds = FMath::Abs(Marker.TimeSeconds - TimeSeconds);
		if (DistanceSeconds <= BestDistanceSeconds)
		{
			BestDistanceSeconds = DistanceSeconds;
			OutEventTimeSeconds = Marker.TimeSeconds;
			OutEventIndex = Marker.EventIndex;
		}
	}

	return OutEventIndex != INDEX_NONE;
}

void UProjectEpisodeReplayViewerWidget::UpdateReplayEpisodeNumberText()
{
	SetReplayChromeText(ReplayEpisodeNumber.Get(), FormatReplayEpisodeNumberText(LoadedEpisodeDirectory));
}

FText UProjectEpisodeReplayViewerWidget::FormatReplayTime(
	double TimeSeconds) const
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

void UProjectEpisodeReplayViewerWidget::RequestReplayInputFocus()
{
	SetIsFocusable(true);
	const TSharedPtr<SWidget> slateWidget = GetCachedWidget();
	if (!slateWidget.IsValid() || !slateWidget->SupportsKeyboardFocus())
	{
		return;
	}
	SetKeyboardFocus();
}

bool UProjectEpisodeReplayViewerWidget::CanUseReplayCameraInput() const
{
	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	return ReplaySubsystem
		&& ReplaySubsystem->IsReplayCameraInputAllowed();
}

bool UProjectEpisodeReplayViewerWidget::CanUseReplayCameraLook() const
{
	const UScenarioReplaySubsystem* ReplaySubsystem = GetReplaySubsystem();
	return ReplaySubsystem
		&& ReplaySubsystem->IsReplayCameraInputAllowed()
		&& (ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::Free
			|| ReplaySubsystem->GetReplayCameraMode() == EScenarioReplayCameraMode::Orbit);
}

FVector UProjectEpisodeReplayViewerWidget::BuildFreeCameraInput() const
{
	FVector Input = FVector::ZeroVector;
	Input.X += bMoveForwardHeld ? 1.0 : 0.0;
	Input.X -= bMoveBackwardHeld ? 1.0 : 0.0;
	Input.Y += bMoveRightHeld ? 1.0 : 0.0;
	Input.Y -= bMoveLeftHeld ? 1.0 : 0.0;
	Input.Z += bMoveUpHeld ? 1.0 : 0.0;
	Input.Z -= bMoveDownHeld ? 1.0 : 0.0;
	return Input;
}

void UProjectEpisodeReplayViewerWidget::ClearReplayMovementInput()
{
	bMoveForwardHeld = false;
	bMoveBackwardHeld = false;
	bMoveLeftHeld = false;
	bMoveRightHeld = false;
	bMoveUpHeld = false;
	bMoveDownHeld = false;
}

void UProjectEpisodeReplayViewerWidget::ClearReplayLookInput()
{
	bFreeCameraLookHeld = false;
}

void UProjectEpisodeReplayViewerWidget::SetDiagnosticsText(const FString& Message)
{
	LastDiagnosticsText = Message;
}
