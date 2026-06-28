#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"

namespace
{
	const int32 ReplayFullscreenLayerZOrder = 100;

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
}

UProjectEpisodeReplayViewerWidget::UProjectEpisodeReplayViewerWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

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
	SetReplayFullscreen(false);
	SetVisibility(ESlateVisibility::Visible);
	UpdateCameraModeText();
	UpdateReplayTimelineUi();
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
	if (ReplayImage)
	{
		ReplayImage->SetBrush(FSlateBrush());
	}
	if (ReplayFullscreenImage)
	{
		ReplayFullscreenImage->SetBrush(FSlateBrush());
	}
	SetReplayFullscreen(false);
	SetDiagnosticsText(TEXT("Replay stopped."));
	UpdateCameraModeText();
	UpdateReplayTimelineUi();
}

void UProjectEpisodeReplayViewerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		PlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
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

	if (FullscreenButton)
	{
		FullscreenButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
		FullscreenButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleFullscreenClicked);
	}

	if (ReplayTimelineSlider)
	{
		ReplayTimelineSlider->OnValueChanged.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
		ReplayTimelineSlider->OnValueChanged.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
	}

	if (FullscreenPlayPauseButton)
	{
		FullscreenPlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
		FullscreenPlayPauseButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
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
}

void UProjectEpisodeReplayViewerWidget::NativeDestruct()
{
	if (PlayPauseButton)
	{
		PlayPauseButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandlePlayPauseClicked);
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

	if (ReplayTimelineSlider)
	{
		ReplayTimelineSlider->OnValueChanged.RemoveDynamic(this, &UProjectEpisodeReplayViewerWidget::HandleReplayTimelineValueChanged);
	}

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

	ResetReplay();
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
		return;
	}

	ReplaySubsystem->Play();
	SetDiagnosticsText(TEXT("Replay playing."));
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
	ReplaySubsystem->Seek(
		ReplaySubsystem->GetDurationSeconds() * NormalizedValue);
	UpdateReplayTimelineUi();
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
}

void UProjectEpisodeReplayViewerWidget::ApplyReplayFullscreenLayout()
{
	ApplyReplayFillSlot(ReplayFullscreenLayer.Get(), ReplayFullscreenLayerZOrder);
	InvalidateLayoutAndVolatility();
	ForceLayoutPrepass();
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

void UProjectEpisodeReplayViewerWidget::UpdateCameraModeText()
{
	if (!CameraModeText && !FullscreenCameraModeText)
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
		const double RobotSpeedKmh =
			ReplaySubsystem
				? ReplaySubsystem->GetCurrentRobotSpeedKmh()
				: 0.0;
		const double PlaybackSpeed =
			ReplaySubsystem
				? ReplaySubsystem->GetPlaybackSpeed()
				: 1.0;
		ReplaySpeedText->SetText(FText::FromString(FString::Printf(
			TEXT("Speed: %.1f km/h | %.2fx"),
			RobotSpeedKmh,
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

	if (ReplayTimelineSlider)
	{
		bUpdatingReplayTimelineSlider = true;
		ReplayTimelineSlider->SetValue(
			ReplaySubsystem
				? static_cast<float>(ReplaySubsystem->GetPlaybackProgress())
				: 0.0f);
		bUpdatingReplayTimelineSlider = false;
	}

	if (FullscreenRayToggleButton)
	{
		FullscreenRayToggleButton->SetIsEnabled(
			ReplaySubsystem && ReplaySubsystem->HasReplayLidarRays());
	}
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
