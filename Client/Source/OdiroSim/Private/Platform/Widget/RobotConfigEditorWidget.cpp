#include "Platform/Widget/RobotConfigEditorWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/Preview/RobotPreviewSubsystem.h"
#include "Platform/RobotProfileSettings.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "Styling/SlateBrush.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseCheckBoxWidget.h"
#include "UI/BaseDropdownWidget.h"
#include "UI/BaseSliderComboWidget.h"

namespace
{
	// Maps WBP combo-box text to preview-only LiDAR display density.
	ERobotPreviewLidarDisplayDensity ResolveRobotPreviewLidarDensity(const FString& RawDensity)
	{
		const FString NormalizedDensity = RawDensity.TrimStartAndEnd().ToLower();
		if (NormalizedDensity == TEXT("간략") || NormalizedDensity == TEXT("sparse"))
		{
			return ERobotPreviewLidarDisplayDensity::Sparse;
		}
		if (NormalizedDensity == TEXT("정밀") || NormalizedDensity == TEXT("dense"))
		{
			return ERobotPreviewLidarDisplayDensity::Dense;
		}
		return ERobotPreviewLidarDisplayDensity::Standard;
	}

	// Builds a base dropdown item whose id intentionally matches the saved profile string.
	FBaseDropdownItem MakeDropdownItem(const TCHAR* itemId)
	{
		FBaseDropdownItem Item;
		Item.Id = FName(itemId);
		Item.Label = FText::FromString(FString(itemId));
		return Item;
	}
}

void URobotConfigEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeDropdowns();

	if (ResetProfileButton)
	{
		ResetProfileButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
		ResetProfileButton->OnBaseClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
	}
	if (SaveProfileButton)
	{
		SaveProfileButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
		SaveProfileButton->OnBaseClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
	}
	if (RotateLeftButton)
	{
		RotateLeftButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
		RotateLeftButton->OnBaseClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
	}
	if (ResetPreviewRotationButton)
	{
		ResetPreviewRotationButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
		ResetPreviewRotationButton->OnBaseClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
	}
	if (RotateRightButton)
	{
		RotateRightButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
		RotateRightButton->OnBaseClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
	}
	if (DrawLidarRaysButton)
	{
		DrawLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
		DrawLidarRaysButton->OnBaseClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysButton)
	{
		ToggleLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
		ToggleLidarRaysButton->OnBaseClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ClearLidarRaysButton)
	{
		ClearLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
		ClearLidarRaysButton->OnBaseClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
	}
	if (ShowLidarRaysCheckBox)
	{
		ShowLidarRaysCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
		ShowLidarRaysCheckBox->OnCheckStateChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (ShowLidarRangeCheckBox)
	{
		ShowLidarRangeCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
		ShowLidarRangeCheckBox->OnCheckStateChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (ShowLidarPointsCheckBox)
	{
		ShowLidarPointsCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
		ShowLidarPointsCheckBox->OnCheckStateChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (LidarPreviewDensityComboBox)
	{
		LidarPreviewDensityComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewDensitySelectionChanged);
		LidarPreviewDensityComboBox->OnSelectionChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewDensitySelectionChanged);
	}
	BindProfileSliderCombo(BodyLengthSliderCombo.Get(), this);
	BindProfileSliderCombo(BodyWidthSliderCombo.Get(), this);
	BindProfileSliderCombo(BodyHeightSliderCombo.Get(), this);
	BindProfileSliderCombo(BodyWheelBaseSliderCombo.Get(), this);
	BindProfileSliderCombo(BodyTurningRadiusSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveMaxSpeedSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveReverseSpeedSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveAccelerationSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveDecelerationSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveSteeringGainSliderCombo.Get(), this);
	BindProfileSliderCombo(DriveMassSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarRangeSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarSensorHeightSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarSensorForwardOffsetSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarSensorRightOffsetSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarFrontAngleSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarStopDistanceSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarObstacleWarningDistanceSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarSlowDownDistanceSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarAngleStepSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarVerticalMinSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarVerticalMaxSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarVerticalStepSliderCombo.Get(), this);
	BindProfileSliderCombo(LidarScanRateSliderCombo.Get(), this);
	if (LidarModeComboBox)
	{
		LidarModeComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarModeSelectionChanged);
		LidarModeComboBox->OnSelectionChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarModeSelectionChanged);
	}
	if (LidarDrawDebugCheckBox)
	{
		LidarDrawDebugCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarDrawDebugChanged);
		LidarDrawDebugCheckBox->OnCheckStateChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarDrawDebugChanged);
	}

	LoadProfileFromViewModel();
	ShowAllProfileSections();
	SyncLidarPreviewControlState();
}

void URobotConfigEditorWidget::NativeDestruct()
{
	if (ResetProfileButton)
	{
		ResetProfileButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
	}
	if (SaveProfileButton)
	{
		SaveProfileButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
	}
	if (RotateLeftButton)
	{
		RotateLeftButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
	}
	if (ResetPreviewRotationButton)
	{
		ResetPreviewRotationButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
	}
	if (RotateRightButton)
	{
		RotateRightButton->OnBaseClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
	}
	if (DrawLidarRaysButton)
	{
		DrawLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysButton)
	{
		ToggleLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ClearLidarRaysButton)
	{
		ClearLidarRaysButton->OnBaseClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
	}
	if (ShowLidarRaysCheckBox)
	{
		ShowLidarRaysCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (ShowLidarRangeCheckBox)
	{
		ShowLidarRangeCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (ShowLidarPointsCheckBox)
	{
		ShowLidarPointsCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewOptionChanged);
	}
	if (LidarPreviewDensityComboBox)
	{
		LidarPreviewDensityComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewDensitySelectionChanged);
	}
	UnbindProfileSliderCombo(BodyLengthSliderCombo.Get(), this);
	UnbindProfileSliderCombo(BodyWidthSliderCombo.Get(), this);
	UnbindProfileSliderCombo(BodyHeightSliderCombo.Get(), this);
	UnbindProfileSliderCombo(BodyWheelBaseSliderCombo.Get(), this);
	UnbindProfileSliderCombo(BodyTurningRadiusSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveMaxSpeedSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveReverseSpeedSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveAccelerationSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveDecelerationSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveSteeringGainSliderCombo.Get(), this);
	UnbindProfileSliderCombo(DriveMassSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarRangeSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarSensorHeightSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarSensorForwardOffsetSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarSensorRightOffsetSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarFrontAngleSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarStopDistanceSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarObstacleWarningDistanceSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarSlowDownDistanceSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarAngleStepSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarVerticalMinSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarVerticalMaxSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarVerticalStepSliderCombo.Get(), this);
	UnbindProfileSliderCombo(LidarScanRateSliderCombo.Get(), this);
	if (LidarModeComboBox)
	{
		LidarModeComboBox->OnSelectionChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarModeSelectionChanged);
	}
	if (LidarDrawDebugCheckBox)
	{
		LidarDrawDebugCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarDrawDebugChanged);
	}

	bRobotPreviewActive = false;
	ClearRobotPreviewOrbitInput();
	StopRobotPreview();

	Super::NativeDestruct();
}

void URobotConfigEditorWidget::ActivateRobotPreview()
{
	if (bRobotPreviewActive)
	{
		SyncRobotPreviewViewportFrame();
		RefreshRobotPreviewFromFields();
		return;
	}

	bRobotPreviewActive = true;
	StartRobotPreview();
}

void URobotConfigEditorWidget::DeactivateRobotPreview()
{
	if (!bRobotPreviewActive)
	{
		ClearRobotPreviewOrbitInput();
		StopRobotPreview();
		return;
	}

	bRobotPreviewActive = false;
	ClearRobotPreviewOrbitInput();
	StopRobotPreview();
}

FReply URobotConfigEditorWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bRobotPreviewActive)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& IsPointerOverRobotPreviewInputArea(InMouseEvent.GetScreenSpacePosition())
		&& ResolveRobotPreviewSubsystem())
	{
		bRobotPreviewOrbitHeld = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URobotConfigEditorWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bRobotPreviewActive)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bRobotPreviewOrbitHeld)
	{
		ClearRobotPreviewOrbitInput();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URobotConfigEditorWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bRobotPreviewActive)
	{
		SyncRobotPreviewViewportFrame();
	}
}

FReply URobotConfigEditorWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bRobotPreviewActive)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	if (bRobotPreviewOrbitHeld)
	{
		if (URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem())
		{
			PreviewSubsystem->AddCameraOrbit(InMouseEvent.GetCursorDelta());
			SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
			return FReply::Handled();
		}

		ClearRobotPreviewOrbitInput();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply URobotConfigEditorWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bRobotPreviewActive)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	if (IsPointerOverRobotPreviewInputArea(InMouseEvent.GetScreenSpacePosition()))
	{
		if (URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem())
		{
			PreviewSubsystem->AddCameraZoom(InMouseEvent.GetWheelDelta());
			SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void URobotConfigEditorWidget::HandleResetProfileClicked(UBaseButtonWidget* button)
{
	(void)button;

	if (!ResolveViewModel())
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		SetProfileStateError(TEXT("상태 오류"));
		return;
	}

	ApplyViewModelToFields();
	RefreshRobotPreviewFromFields();
	SetProfileStatus(TEXT("수정 취소됨"));
	SetProfileStateSaved(TEXT("변경 취소됨"));
}

void URobotConfigEditorWidget::HandleSaveProfileClicked(UBaseButtonWidget* button)
{
	(void)button;

	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		SetProfileStateError(TEXT("저장 실패"));
		return;
	}

	if (!ReadFieldsIntoViewModel())
	{
		return;
	}

	if (!viewModel->SaveRobotProfile())
	{
		SetProfileStatus(viewModel->GetDiagnosticsText());
		SetProfileStateError(TEXT("저장 실패"));
		return;
	}

	SetProfilePathText(viewModel->GetProfilePath());
	SetProfileStatus(TEXT("저장 완료"));
	SetProfileStateSaved(TEXT("최근 저장 방금 전"));
	RefreshRobotPreviewFromFields();
}

void URobotConfigEditorWidget::HandleProfileSliderComboChanged(UWidget* widget, const float value)
{
	(void)widget;
	(void)value;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarModeSelectionChanged(
	UWidget* widget,
	const FName selectedId)
{
	(void)widget;
	(void)selectedId;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarDrawDebugChanged(UWidget* widget, const ECheckBoxState checkState)
{
	(void)widget;
	(void)checkState;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarPreviewOptionChanged(UWidget* widget, const ECheckBoxState checkState)
{
	(void)widget;
	(void)checkState;
	ApplyRobotPreviewDisplayOptions();
}

void URobotConfigEditorWidget::HandleLidarPreviewDensitySelectionChanged(
	UWidget* widget,
	const FName selectedId)
{
	(void)widget;
	(void)selectedId;
	ApplyRobotPreviewDisplayOptions();
}

void URobotConfigEditorWidget::HandleRotatePreviewLeftClicked(UBaseButtonWidget* button)
{
	(void)button;

	if (!bRobotPreviewActive)
	{
		return;
	}

	if (URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem())
	{
		PreviewSubsystem->AddRobotYawDegrees(-15.0f);
		SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
	}
}

void URobotConfigEditorWidget::HandleResetPreviewRotationClicked(UBaseButtonWidget* button)
{
	(void)button;

	if (!bRobotPreviewActive)
	{
		return;
	}

	if (URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem())
	{
		PreviewSubsystem->ResetRobotYaw();
		SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
	}
}

void URobotConfigEditorWidget::HandleRotatePreviewRightClicked(UBaseButtonWidget* button)
{
	(void)button;

	if (!bRobotPreviewActive)
	{
		return;
	}

	if (URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem())
	{
		PreviewSubsystem->AddRobotYawDegrees(15.0f);
		SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
	}
}

void URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked(UBaseButtonWidget* button)
{
	(void)button;

	URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	const bool bShouldShow = !PreviewSubsystem || !PreviewSubsystem->AreLidarPreviewRaysVisible();
	SetLidarPreviewRaysVisible(bShouldShow);
}

void URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked(UBaseButtonWidget* button)
{
	(void)button;
	SetLidarPreviewRaysVisible(false);
}

URobotProfileViewModel* URobotConfigEditorWidget::ResolveViewModel()
{
	if (RobotProfileViewModel)
	{
		return RobotProfileViewModel;
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	RobotProfileViewModel = platformUiSubsystem ? platformUiSubsystem->GetRobotProfileViewModel() : nullptr;
	return RobotProfileViewModel;
}

URobotPreviewSubsystem* URobotConfigEditorWidget::ResolveRobotPreviewSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URobotPreviewSubsystem>() : nullptr;
}

bool URobotConfigEditorWidget::LoadProfileFromViewModel()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		SetProfileStateError(TEXT("불러오기 실패"));
		return false;
	}

	const bool bLoaded = viewModel->LoadFromActiveProject();
	SetProfilePathText(viewModel->GetProfilePath());
	if (!bLoaded)
	{
		SetProfileStatus(viewModel->GetDiagnosticsText());
		SetProfileStateError(TEXT("불러오기 실패"));
		return false;
	}

	ApplyViewModelToFields();
	SetProfileStatus(TEXT("불러오기 완료"));
	SetProfileStateSaved(TEXT("최근 불러오기 완료"));
	return true;
}

bool URobotConfigEditorWidget::ReadFieldsIntoViewModel()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		SetProfileStateError(TEXT("상태 오류"));
		return false;
	}

	float bodyLengthM = 0.0f;
	float bodyWidthM = 0.0f;
	float bodyHeightM = 0.0f;
	float bodyWheelBaseM = 0.0f;
	float bodyTurningRadiusM = 0.0f;
	float driveMaxSpeedKmh = 0.0f;
	float driveMaxReverseSpeedKmh = 0.0f;
	float driveAccelerationRateKmhPerSecond = 0.0f;
	float driveDecelerationRateKmhPerSecond = 0.0f;
	float driveSteeringRatePerS = 0.0f;
	float driveMassKg = 0.0f;
	float lidarScanRangeM = 0.0f;
	float lidarSensorHeightM = 0.0f;
	float lidarSensorForwardOffsetM = viewModel->GetLidarSensorForwardOffsetM();
	float lidarSensorRightOffsetM = viewModel->GetLidarSensorRightOffsetM();
	float lidarFrontHalfAngleDegree = 0.0f;
	float lidarStopDistanceM = 0.0f;
	float lidarObstacleWarningDistanceM = viewModel->GetLidarObstacleWarningDistanceM();
	float lidarSlowDownDistanceM = 0.0f;
	float lidarAngleStepDegree = 0.0f;
	float lidarVerticalMinDegree = viewModel->GetLidarVerticalMinDegree();
	float lidarVerticalMaxDegree = viewModel->GetLidarVerticalMaxDegree();
	float lidarVerticalStepDegree = viewModel->GetLidarVerticalStepDegree();
	float lidarScanRateHz = 0.0f;
	if (!TryReadFloatField(BodyLengthSliderCombo.Get(), TEXT("Body Length"), bodyLengthM)
		|| !TryReadFloatField(BodyWidthSliderCombo.Get(), TEXT("Body Width"), bodyWidthM)
		|| !TryReadFloatField(BodyHeightSliderCombo.Get(), TEXT("Body Height"), bodyHeightM)
		|| !TryReadFloatField(BodyWheelBaseSliderCombo.Get(), TEXT("Wheel Base"), bodyWheelBaseM)
		|| !TryReadFloatField(BodyTurningRadiusSliderCombo.Get(), TEXT("Turning Radius"), bodyTurningRadiusM)
		|| !TryReadFloatField(DriveMaxSpeedSliderCombo.Get(), TEXT("Max Speed"), driveMaxSpeedKmh)
		|| !TryReadFloatField(
			DriveReverseSpeedSliderCombo.Get(),
			TEXT("Max Reverse Speed"),
			driveMaxReverseSpeedKmh)
		|| !TryReadFloatField(
			DriveAccelerationSliderCombo.Get(),
			TEXT("Acceleration Rate"),
			driveAccelerationRateKmhPerSecond)
		|| !TryReadFloatField(
			DriveDecelerationSliderCombo.Get(),
			TEXT("Deceleration Rate"),
			driveDecelerationRateKmhPerSecond)
		|| !TryReadFloatField(DriveSteeringGainSliderCombo.Get(), TEXT("Steering Rate"), driveSteeringRatePerS)
		|| !TryReadFloatField(DriveMassSliderCombo.Get(), TEXT("Mass"), driveMassKg)
		|| !TryReadFloatField(LidarRangeSliderCombo.Get(), TEXT("LiDAR Scan Range"), lidarScanRangeM)
		|| !TryReadFloatField(
			LidarSensorHeightSliderCombo.Get(),
			TEXT("LiDAR Sensor Height"),
			lidarSensorHeightM)
		|| !TryReadFloatField(
			LidarFrontAngleSliderCombo.Get(),
			TEXT("LiDAR Front Angle"),
			lidarFrontHalfAngleDegree)
		|| !TryReadFloatField(
			LidarStopDistanceSliderCombo.Get(),
			TEXT("LiDAR Stop Distance"),
			lidarStopDistanceM)
		|| !TryReadFloatField(
			LidarSlowDownDistanceSliderCombo.Get(),
			TEXT("LiDAR Slowdown Distance"),
			lidarSlowDownDistanceM)
		|| !TryReadFloatField(LidarAngleStepSliderCombo.Get(), TEXT("LiDAR Angle Step"), lidarAngleStepDegree)
		|| !TryReadFloatField(LidarScanRateSliderCombo.Get(), TEXT("LiDAR Scan Rate"), lidarScanRateHz))
	{
		return false;
	}
	if (!TryReadOptionalFloatField(
			LidarSensorForwardOffsetSliderCombo.Get(),
			TEXT("LiDAR Sensor Forward Offset"),
			lidarSensorForwardOffsetM)
		|| !TryReadOptionalFloatField(
			LidarSensorRightOffsetSliderCombo.Get(),
			TEXT("LiDAR Sensor Right Offset"),
			lidarSensorRightOffsetM)
		|| !TryReadOptionalFloatField(
			LidarObstacleWarningDistanceSliderCombo.Get(),
			TEXT("LiDAR Obstacle Warning Distance"),
			lidarObstacleWarningDistanceM)
		|| !TryReadOptionalFloatField(
			LidarVerticalMinSliderCombo.Get(),
			TEXT("LiDAR Vertical Min"),
			lidarVerticalMinDegree)
		|| !TryReadOptionalFloatField(
			LidarVerticalMaxSliderCombo.Get(),
			TEXT("LiDAR Vertical Max"),
			lidarVerticalMaxDegree)
		|| !TryReadOptionalFloatField(
			LidarVerticalStepSliderCombo.Get(),
			TEXT("LiDAR Vertical Step"),
			lidarVerticalStepDegree))
	{
		return false;
	}

	viewModel->SetBodyLengthM(bodyLengthM);
	viewModel->SetBodyWidthM(bodyWidthM);
	viewModel->SetBodyHeightM(bodyHeightM);
	viewModel->SetBodyWheelBaseM(bodyWheelBaseM);
	viewModel->SetBodyTurningRadiusM(bodyTurningRadiusM);
	viewModel->SetDriveMaxSpeedKmh(driveMaxSpeedKmh);
	viewModel->SetDriveMaxReverseSpeedKmh(driveMaxReverseSpeedKmh);
	viewModel->SetDriveAccelerationRateKmhPerSecond(driveAccelerationRateKmhPerSecond);
	viewModel->SetDriveDecelerationRateKmhPerSecond(driveDecelerationRateKmhPerSecond);
	viewModel->SetDriveSteeringRatePerS(driveSteeringRatePerS);
	viewModel->SetDriveMassKg(driveMassKg);
	if (LidarModeComboBox)
	{
		viewModel->SetLidarMode(GetDropdownSelection(LidarModeComboBox.Get()));
	}
	if (LidarDrawDebugCheckBox)
	{
		viewModel->SetLidarDrawDebug(IsBaseCheckBoxChecked(LidarDrawDebugCheckBox.Get()));
	}
	viewModel->SetLidarScanRangeM(lidarScanRangeM);
	viewModel->SetLidarSensorHeightM(lidarSensorHeightM);
	viewModel->SetLidarSensorForwardOffsetM(lidarSensorForwardOffsetM);
	viewModel->SetLidarSensorRightOffsetM(lidarSensorRightOffsetM);
	viewModel->SetLidarFrontHalfAngleDegree(lidarFrontHalfAngleDegree);
	viewModel->SetLidarStopDistanceM(lidarStopDistanceM);
	viewModel->SetLidarObstacleWarningDistanceM(lidarObstacleWarningDistanceM);
	viewModel->SetLidarSlowDownDistanceM(lidarSlowDownDistanceM);
	viewModel->SetLidarAngleStepDegree(lidarAngleStepDegree);
	viewModel->SetLidarVerticalMinDegree(lidarVerticalMinDegree);
	viewModel->SetLidarVerticalMaxDegree(lidarVerticalMaxDegree);
	viewModel->SetLidarVerticalStepDegree(lidarVerticalStepDegree);
	viewModel->SetLidarScanRateHz(lidarScanRateHz);
	return true;
}

bool URobotConfigEditorWidget::TryReadFloatField(
	UBaseSliderComboWidget* sliderCombo,
	const FString& label,
	float& outValue)
{
	if (!sliderCombo)
	{
		SetProfileStatus(FString::Printf(TEXT("%s input widget is missing."), *label));
		SetProfileStateError(TEXT("입력값 오류"));
		return false;
	}

	outValue = sliderCombo->GetValue();
	if (!FMath::IsFinite(outValue))
	{
		SetProfileStatus(FString::Printf(TEXT("%s must be a number."), *label));
		SetProfileStateError(TEXT("입력값 오류"));
		return false;
	}

	return true;
}

bool URobotConfigEditorWidget::TryReadOptionalFloatField(
	UBaseSliderComboWidget* sliderCombo,
	const FString& label,
	float& outValue)
{
	return !sliderCombo || TryReadFloatField(sliderCombo, label, outValue);
}

bool URobotConfigEditorWidget::TryReadFieldsIntoPreviewSettings(FRobotProfileSettings& outSettings) const
{
	FRobotProfileSettings previewSettings;
	if (!TryReadPreviewFloatField(BodyLengthSliderCombo.Get(), previewSettings.Body.LengthM)
		|| !TryReadPreviewFloatField(BodyWidthSliderCombo.Get(), previewSettings.Body.WidthM)
		|| !TryReadPreviewFloatField(BodyHeightSliderCombo.Get(), previewSettings.Body.HeightM)
		|| !TryReadPreviewFloatField(BodyWheelBaseSliderCombo.Get(), previewSettings.Body.WheelBaseM)
		|| !TryReadPreviewFloatField(BodyTurningRadiusSliderCombo.Get(), previewSettings.Body.TurningRadiusM)
		|| !TryReadPreviewFloatField(DriveMaxSpeedSliderCombo.Get(), previewSettings.Drive.MaxSpeedKmh)
		|| !TryReadPreviewFloatField(DriveReverseSpeedSliderCombo.Get(), previewSettings.Drive.MaxReverseSpeedKmh)
		|| !TryReadPreviewFloatField(
			DriveAccelerationSliderCombo.Get(),
			previewSettings.Drive.AccelerationRateKmhPerSecond)
		|| !TryReadPreviewFloatField(
			DriveDecelerationSliderCombo.Get(),
			previewSettings.Drive.DecelerationRateKmhPerSecond)
		|| !TryReadPreviewFloatField(DriveSteeringGainSliderCombo.Get(), previewSettings.Drive.SteeringRatePerS)
		|| !TryReadPreviewFloatField(DriveMassSliderCombo.Get(), previewSettings.Drive.MassKg)
		|| !TryReadPreviewFloatField(LidarRangeSliderCombo.Get(), previewSettings.Lidar.ScanRangeM)
		|| !TryReadPreviewFloatField(LidarSensorHeightSliderCombo.Get(), previewSettings.Lidar.SensorHeightM)
		|| !TryReadPreviewFloatField(LidarFrontAngleSliderCombo.Get(), previewSettings.Lidar.FrontHalfAngleDegree)
		|| !TryReadPreviewFloatField(LidarStopDistanceSliderCombo.Get(), previewSettings.Lidar.StopDistanceM)
		|| !TryReadPreviewFloatField(LidarSlowDownDistanceSliderCombo.Get(), previewSettings.Lidar.SlowDownDistanceM)
		|| !TryReadPreviewFloatField(LidarAngleStepSliderCombo.Get(), previewSettings.Lidar.AngleStepDegree)
		|| !TryReadPreviewFloatField(LidarScanRateSliderCombo.Get(), previewSettings.Lidar.ScanRateHz))
	{
		return false;
	}
	if (!TryReadOptionalPreviewFloatField(
			LidarSensorForwardOffsetSliderCombo.Get(),
			previewSettings.Lidar.SensorForwardOffsetM)
		|| !TryReadOptionalPreviewFloatField(
			LidarSensorRightOffsetSliderCombo.Get(),
			previewSettings.Lidar.SensorRightOffsetM)
		|| !TryReadOptionalPreviewFloatField(
			LidarObstacleWarningDistanceSliderCombo.Get(),
			previewSettings.Lidar.ObstacleWarningDistanceM)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalMinSliderCombo.Get(),
			previewSettings.Lidar.VerticalMinDegree)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalMaxSliderCombo.Get(),
			previewSettings.Lidar.VerticalMaxDegree)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalStepSliderCombo.Get(),
			previewSettings.Lidar.VerticalStepDegree))
	{
		return false;
	}

	if (LidarModeComboBox)
	{
		const FString selectedMode = GetDropdownSelection(LidarModeComboBox.Get()).TrimStartAndEnd();
		if (!selectedMode.IsEmpty())
		{
			previewSettings.Lidar.LidarMode = selectedMode;
		}
	}
	if (LidarDrawDebugCheckBox)
	{
		previewSettings.Lidar.bDrawDebug = IsBaseCheckBoxChecked(LidarDrawDebugCheckBox.Get());
	}

	outSettings = previewSettings;
	return true;
}

bool URobotConfigEditorWidget::TryReadPreviewFloatField(UBaseSliderComboWidget* sliderCombo, float& outValue)
{
	if (!sliderCombo)
	{
		return false;
	}

	outValue = sliderCombo->GetValue();
	return FMath::IsFinite(outValue);
}

bool URobotConfigEditorWidget::TryReadOptionalPreviewFloatField(UBaseSliderComboWidget* sliderCombo, float& outValue)
{
	return !sliderCombo || TryReadPreviewFloatField(sliderCombo, outValue);
}

void URobotConfigEditorWidget::ApplyViewModelToFields()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		return;
	}

	TGuardValue<bool> applyingGuard(bApplyingProfileFields, true);
	SetSliderComboFieldValue(BodyLengthSliderCombo.Get(), viewModel->GetBodyLengthM());
	SetSliderComboFieldValue(BodyWidthSliderCombo.Get(), viewModel->GetBodyWidthM());
	SetSliderComboFieldValue(BodyHeightSliderCombo.Get(), viewModel->GetBodyHeightM());
	SetSliderComboFieldValue(BodyWheelBaseSliderCombo.Get(), viewModel->GetBodyWheelBaseM());
	SetSliderComboFieldValue(BodyTurningRadiusSliderCombo.Get(), viewModel->GetBodyTurningRadiusM());
	SetSliderComboFieldValue(DriveMaxSpeedSliderCombo.Get(), viewModel->GetDriveMaxSpeedKmh());
	SetSliderComboFieldValue(DriveReverseSpeedSliderCombo.Get(), viewModel->GetDriveMaxReverseSpeedKmh());
	SetSliderComboFieldValue(
		DriveAccelerationSliderCombo.Get(),
		viewModel->GetDriveAccelerationRateKmhPerSecond());
	SetSliderComboFieldValue(
		DriveDecelerationSliderCombo.Get(),
		viewModel->GetDriveDecelerationRateKmhPerSecond());
	SetSliderComboFieldValue(DriveSteeringGainSliderCombo.Get(), viewModel->GetDriveSteeringRatePerS());
	SetSliderComboFieldValue(DriveMassSliderCombo.Get(), viewModel->GetDriveMassKg());
	SetDropdownSelection(LidarModeComboBox.Get(), viewModel->GetLidarMode());
	SetBaseCheckBoxChecked(LidarDrawDebugCheckBox.Get(), viewModel->GetLidarDrawDebug());
	SetSliderComboFieldValue(LidarRangeSliderCombo.Get(), viewModel->GetLidarScanRangeM());
	SetSliderComboFieldValue(LidarSensorHeightSliderCombo.Get(), viewModel->GetLidarSensorHeightM());
	SetSliderComboFieldValue(
		LidarSensorForwardOffsetSliderCombo.Get(),
		viewModel->GetLidarSensorForwardOffsetM());
	SetSliderComboFieldValue(LidarSensorRightOffsetSliderCombo.Get(), viewModel->GetLidarSensorRightOffsetM());
	SetSliderComboFieldValue(LidarFrontAngleSliderCombo.Get(), viewModel->GetLidarFrontHalfAngleDegree());
	SetSliderComboFieldValue(LidarStopDistanceSliderCombo.Get(), viewModel->GetLidarStopDistanceM());
	SetSliderComboFieldValue(
		LidarObstacleWarningDistanceSliderCombo.Get(),
		viewModel->GetLidarObstacleWarningDistanceM());
	SetSliderComboFieldValue(LidarSlowDownDistanceSliderCombo.Get(), viewModel->GetLidarSlowDownDistanceM());
	SetSliderComboFieldValue(LidarAngleStepSliderCombo.Get(), viewModel->GetLidarAngleStepDegree());
	SetSliderComboFieldValue(LidarVerticalMinSliderCombo.Get(), viewModel->GetLidarVerticalMinDegree());
	SetSliderComboFieldValue(LidarVerticalMaxSliderCombo.Get(), viewModel->GetLidarVerticalMaxDegree());
	SetSliderComboFieldValue(LidarVerticalStepSliderCombo.Get(), viewModel->GetLidarVerticalStepDegree());
	SetSliderComboFieldValue(LidarScanRateSliderCombo.Get(), viewModel->GetLidarScanRateHz());
}

void URobotConfigEditorWidget::BindProfileSliderCombo(
	UBaseSliderComboWidget* sliderCombo,
	URobotConfigEditorWidget* owner)
{
	if (sliderCombo && owner)
	{
		sliderCombo->OnValueChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderComboChanged);
		sliderCombo->OnValueChanged.AddDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderComboChanged);
	}
}

void URobotConfigEditorWidget::UnbindProfileSliderCombo(
	UBaseSliderComboWidget* sliderCombo,
	URobotConfigEditorWidget* owner)
{
	if (sliderCombo && owner)
	{
		sliderCombo->OnValueChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderComboChanged);
	}
}

void URobotConfigEditorWidget::ShowAllProfileSections() const
{
	if (BodyFieldsBox)
	{
		BodyFieldsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (DriveFieldsBox)
	{
		DriveFieldsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (LiDARFieldsBox)
	{
		LiDARFieldsBox->SetVisibility(ESlateVisibility::Visible);
	}
}

void URobotConfigEditorWidget::StartRobotPreview()
{
	FRobotProfileSettings previewSettings;
	if (!TryReadFieldsIntoPreviewSettings(previewSettings))
	{
		SetRobotPreviewStatus(TEXT("Preview 입력값 확인 중"));
		return;
	}

	URobotPreviewSubsystem* previewSubsystem = ResolveRobotPreviewSubsystem();
	if (!previewSubsystem)
	{
		SetRobotPreviewStatus(TEXT("Preview subsystem 없음"));
		return;
	}

	if (!previewSubsystem->StartPreview(this, previewSettings))
	{
		SetRobotPreviewStatus(previewSubsystem->GetStatusText());
		return;
	}

	SyncRobotPreviewViewportFrame(true);
	ApplyRobotPreviewRenderTarget();
	ApplyRobotPreviewDisplayOptions();
	// The WBP may hide the legacy toggle button; keep LiDAR preview options active on first open.
	if (!previewSubsystem->DrawLidarPreviewRays())
	{
		SetRobotPreviewStatus(previewSubsystem->GetStatusText());
		SyncLidarPreviewControlState();
		return;
	}

	SetRobotPreviewStatus(previewSubsystem->GetStatusText());
	SyncLidarPreviewControlState();
}

void URobotConfigEditorWidget::StopRobotPreview()
{
	if (RobotPreviewImage)
	{
		RobotPreviewImage->SetBrush(FSlateBrush());
	}
	LastRobotPreviewFrameCenterPixel = FVector2D::ZeroVector;
	LastRobotPreviewViewportSizePixel = FVector2D::ZeroVector;

	if (URobotPreviewSubsystem* previewSubsystem = ResolveRobotPreviewSubsystem())
	{
		previewSubsystem->StopPreview(this);
	}
	SyncLidarPreviewControlState();
}

void URobotConfigEditorWidget::RefreshRobotPreviewFromFields()
{
	if (!bRobotPreviewActive)
	{
		return;
	}

	FRobotProfileSettings previewSettings;
	if (!TryReadFieldsIntoPreviewSettings(previewSettings))
	{
		SetRobotPreviewStatus(TEXT("Preview 입력값 확인 중"));
		return;
	}

	URobotPreviewSubsystem* previewSubsystem = ResolveRobotPreviewSubsystem();
	if (!previewSubsystem)
	{
		SetRobotPreviewStatus(TEXT("Preview subsystem 없음"));
		return;
	}

	SyncRobotPreviewViewportFrame();
	if (!previewSubsystem->ApplyPreviewSettings(previewSettings))
	{
		if (!previewSubsystem->StartPreview(this, previewSettings))
		{
			SetRobotPreviewStatus(previewSubsystem->GetStatusText());
			return;
		}
		SyncRobotPreviewViewportFrame(true);
		ApplyRobotPreviewRenderTarget();
	}

	ApplyRobotPreviewDisplayOptions();
	SetRobotPreviewStatus(previewSubsystem->GetStatusText());
}

void URobotConfigEditorWidget::ApplyRobotPreviewDisplayOptions()
{
	if (!bRobotPreviewActive)
	{
		SyncLidarPreviewControlState();
		return;
	}

	URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	if (!PreviewSubsystem)
	{
		SyncLidarPreviewControlState();
		return;
	}

	FRobotPreviewLidarDisplayOptions DisplayOptions;
	DisplayOptions.bShowRays = ShowLidarRaysCheckBox ? IsBaseCheckBoxChecked(ShowLidarRaysCheckBox.Get()) : true;
	DisplayOptions.bShowRange = ShowLidarRangeCheckBox ? IsBaseCheckBoxChecked(ShowLidarRangeCheckBox.Get()) : true;
	DisplayOptions.bShowPoints = ShowLidarPointsCheckBox ? IsBaseCheckBoxChecked(ShowLidarPointsCheckBox.Get()) : false;

	if (LidarPreviewDensityComboBox)
	{
		const FString SelectedDensity = GetDropdownSelection(LidarPreviewDensityComboBox.Get()).TrimStartAndEnd();
		if (!SelectedDensity.IsEmpty())
		{
			DisplayOptions.Density = ResolveRobotPreviewLidarDensity(SelectedDensity);
		}
	}

	PreviewSubsystem->SetLidarDisplayOptions(DisplayOptions);
	SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
	SyncLidarPreviewControlState();
}

void URobotConfigEditorWidget::SetLidarPreviewRaysVisible(const bool bShouldShow)
{
	if (!bRobotPreviewActive)
	{
		SyncLidarPreviewControlState();
		return;
	}

	URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	if (!PreviewSubsystem)
	{
		SetRobotPreviewStatus(TEXT("Preview subsystem is unavailable"));
		SyncLidarPreviewControlState();
		return;
	}

	if (!bShouldShow)
	{
		PreviewSubsystem->ClearLidarPreviewRays();
		SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
		SyncLidarPreviewControlState();
		return;
	}

	FRobotProfileSettings PreviewSettings;
	if (!TryReadFieldsIntoPreviewSettings(PreviewSettings))
	{
		SetRobotPreviewStatus(TEXT("Preview input is not ready"));
		SyncLidarPreviewControlState();
		return;
	}

	SyncRobotPreviewViewportFrame();
	if (!PreviewSubsystem->ApplyPreviewSettings(PreviewSettings))
	{
		if (!PreviewSubsystem->StartPreview(this, PreviewSettings))
		{
			SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
			SyncLidarPreviewControlState();
			return;
		}
		SyncRobotPreviewViewportFrame(true);
		ApplyRobotPreviewRenderTarget();
	}

	ApplyRobotPreviewDisplayOptions();
	if (!PreviewSubsystem->DrawLidarPreviewRays())
	{
		SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
		SyncLidarPreviewControlState();
		return;
	}
	SetRobotPreviewStatus(PreviewSubsystem->GetStatusText());
	SyncLidarPreviewControlState();
}

void URobotConfigEditorWidget::SyncLidarPreviewControlState()
{
	const URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	const bool bRaysVisible = bRobotPreviewActive
		&& PreviewSubsystem
		&& PreviewSubsystem->AreLidarPreviewRaysVisible();

	if (ToggleLidarRaysButton)
	{
		ToggleLidarRaysButton->SetSelected(bRaysVisible);
	}

	UWidget* LidarPreviewOptionsContainer = LidarPreviewOptionsRow
		? LidarPreviewOptionsRow.Get()
		: LidarPreviewOptionsPanel.Get();
	if (LidarPreviewOptionsContainer)
	{
		LidarPreviewOptionsContainer->SetIsEnabled(bRaysVisible);
		LidarPreviewOptionsContainer->SetRenderOpacity(bRaysVisible ? 1.0f : 0.42f);
		return;
	}

	if (ShowLidarRaysCheckBox)
	{
		ShowLidarRaysCheckBox->SetDisabled(!bRaysVisible);
	}
	if (ShowLidarRangeCheckBox)
	{
		ShowLidarRangeCheckBox->SetDisabled(!bRaysVisible);
	}
	if (ShowLidarPointsCheckBox)
	{
		ShowLidarPointsCheckBox->SetDisabled(!bRaysVisible);
	}
	if (LidarPreviewDensityComboBox)
	{
		LidarPreviewDensityComboBox->SetDisabled(!bRaysVisible);
	}
}

void URobotConfigEditorWidget::ApplyRobotPreviewRenderTarget()
{
	if (!RobotPreviewImage)
	{
		return;
	}

	URobotPreviewSubsystem* previewSubsystem = ResolveRobotPreviewSubsystem();
	if (!previewSubsystem || !previewSubsystem->IsUsingSceneCaptureRenderTarget())
	{
		RobotPreviewImage->SetBrush(FSlateBrush());
		return;
	}

	UTextureRenderTarget2D* renderTarget = previewSubsystem ? previewSubsystem->GetRenderTarget() : nullptr;
	if (!renderTarget)
	{
		RobotPreviewImage->SetBrush(FSlateBrush());
		return;
	}

	FSlateBrush previewBrush;
	previewBrush.SetResourceObject(renderTarget);
	previewBrush.ImageSize = FVector2D(renderTarget->SizeX, renderTarget->SizeY);
	RobotPreviewImage->SetBrush(previewBrush);
}

void URobotConfigEditorWidget::SyncRobotPreviewViewportFrame(const bool bForce)
{
	URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	if (!PreviewSubsystem)
	{
		return;
	}

	UWidget* PreviewFrameWidget = RobotPreviewViewportInputArea
		? RobotPreviewViewportInputArea.Get()
		: RobotPreviewImage.Get();
	if (!PreviewFrameWidget)
	{
		return;
	}

	const FGeometry PreviewFrameGeometry = PreviewFrameWidget->GetCachedGeometry();
	const FVector2D PreviewFrameLocalSize = PreviewFrameGeometry.GetLocalSize();
	if (PreviewFrameLocalSize.X <= UE_SMALL_NUMBER || PreviewFrameLocalSize.Y <= UE_SMALL_NUMBER)
	{
		return;
	}

	FVector2D FrameCenterPixel = FVector2D::ZeroVector;
	FVector2D FrameCenterViewportPosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::LocalToViewport(
		this,
		PreviewFrameGeometry,
		PreviewFrameLocalSize * 0.5f,
		FrameCenterPixel,
		FrameCenterViewportPosition);

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer)
	{
		return;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	OwningPlayer->GetViewportSize(ViewportWidth, ViewportHeight);
	const FVector2D ViewportSizePixel(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
	if (ViewportSizePixel.X <= UE_SMALL_NUMBER || ViewportSizePixel.Y <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (!bForce
		&& FrameCenterPixel.Equals(LastRobotPreviewFrameCenterPixel, 0.5f)
		&& ViewportSizePixel.Equals(LastRobotPreviewViewportSizePixel, 0.5f))
	{
		return;
	}

	LastRobotPreviewFrameCenterPixel = FrameCenterPixel;
	LastRobotPreviewViewportSizePixel = ViewportSizePixel;
	PreviewSubsystem->SetViewportFocusFrame(FrameCenterPixel, ViewportSizePixel);
}

void URobotConfigEditorWidget::SetRobotPreviewStatus(const FString& statusText) const
{
	if (RobotPreviewStatusText)
	{
		RobotPreviewStatusText->SetText(FText::FromString(statusText));
	}
}

bool URobotConfigEditorWidget::IsPointerOverRobotPreviewInputArea(const FVector2D& ScreenSpacePosition) const
{
	if (RobotPreviewViewportInputArea)
	{
		return RobotPreviewViewportInputArea->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition);
	}

	return RobotPreviewImage
		&& RobotPreviewImage->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition);
}

void URobotConfigEditorWidget::ClearRobotPreviewOrbitInput()
{
	bRobotPreviewOrbitHeld = false;
}

void URobotConfigEditorWidget::MarkProfileDirty()
{
	if (bApplyingProfileFields)
	{
		return;
	}

	SetProfileStatus(TEXT("변경사항이 있습니다"));
	SetProfileStateDirty();
	RefreshRobotPreviewFromFields();
}

void URobotConfigEditorWidget::SetProfileStateSaved(const FString& detailText) const
{
	SetProfileStateTexts(
		TEXT("저장됨"),
		detailText,
		TEXT("방금 전"),
		FLinearColor(0.055f, 0.19f, 0.075f, 1.0f),
		FLinearColor(0.78f, 0.95f, 0.78f, 1.0f),
		FLinearColor(0.20f, 0.72f, 0.28f, 1.0f));
}

void URobotConfigEditorWidget::SetProfileStateDirty() const
{
	SetProfileStateTexts(
		TEXT("수정됨"),
		TEXT("저장되지 않음"),
		TEXT("저장 전"),
		FLinearColor(0.23f, 0.18f, 0.055f, 1.0f),
		FLinearColor(1.0f, 0.78f, 0.32f, 1.0f),
		FLinearColor(1.0f, 0.48f, 0.05f, 1.0f));
}

void URobotConfigEditorWidget::SetProfileStateError(const FString& detailText) const
{
	SetProfileStateTexts(
		TEXT("오류"),
		detailText,
		TEXT("확인 필요"),
		FLinearColor(0.24f, 0.07f, 0.07f, 1.0f),
		FLinearColor(1.0f, 0.62f, 0.62f, 1.0f),
		FLinearColor(1.0f, 0.18f, 0.16f, 1.0f));
}

void URobotConfigEditorWidget::SetProfileStateTexts(
	const FString& stateText,
	const FString& detailText,
	const FString& actionTimeText,
	const FLinearColor& badgeColor,
	const FLinearColor& textColor,
	const FLinearColor& actionDotColor) const
{
	if (ProfileStateText)
	{
		ProfileStateText->SetText(FText::FromString(stateText));
		ProfileStateText->SetColorAndOpacity(FSlateColor(textColor));
	}
	if (ProfileSavedAtText)
	{
		ProfileSavedAtText->SetText(FText::FromString(detailText));
	}
	if (ProfileStateBadgeBorder)
	{
		ProfileStateBadgeBorder->SetBrushColor(badgeColor);
	}
	if (ProfileActionStatusDot)
	{
		ProfileActionStatusDot->SetBrushColor(actionDotColor);
	}
	if (ProfileActionTimeText)
	{
		ProfileActionTimeText->SetText(FText::FromString(actionTimeText));
	}
}

void URobotConfigEditorWidget::SetProfileStatus(const FString& statusText) const
{
	if (ProfileStatusText)
	{
		ProfileStatusText->SetText(FText::FromString(statusText));
	}
}

void URobotConfigEditorWidget::SetProfilePathText(const FString& profilePath) const
{
	if (ProfilePathText)
	{
		ProfilePathText->SetText(FText::FromString(profilePath));
	}
}

void URobotConfigEditorWidget::SetSliderComboFieldValue(UBaseSliderComboWidget* sliderCombo, const float value)
{
	if (sliderCombo)
	{
		sliderCombo->SetValue(value);
	}
}

void URobotConfigEditorWidget::InitializeDropdowns() const
{
	if (LidarModeComboBox)
	{
		TArray<FBaseDropdownItem> ModeItems;
		ModeItems.Add(MakeDropdownItem(TEXT("1D")));
		ModeItems.Add(MakeDropdownItem(TEXT("2D")));
		ModeItems.Add(MakeDropdownItem(TEXT("3D")));
		ModeItems.Add(MakeDropdownItem(TEXT("Ouster OS1")));
		LidarModeComboBox->SetItems(ModeItems);
	}

	if (LidarPreviewDensityComboBox)
	{
		TArray<FBaseDropdownItem> DensityItems;
		DensityItems.Add(MakeDropdownItem(TEXT("간략")));
		DensityItems.Add(MakeDropdownItem(TEXT("표준")));
		DensityItems.Add(MakeDropdownItem(TEXT("정밀")));
		LidarPreviewDensityComboBox->SetItems(DensityItems);
	}
}

void URobotConfigEditorWidget::SetDropdownSelection(UBaseDropdownWidget* dropdown, const FString& selectedOption)
{
	const FString TrimmedOption = selectedOption.TrimStartAndEnd();
	if (!dropdown || TrimmedOption.IsEmpty())
	{
		return;
	}

	FName OptionId(*TrimmedOption);
	for (const FBaseDropdownItem& Item : dropdown->GetItems())
	{
		if (Item.Id.ToString().Equals(TrimmedOption, ESearchCase::IgnoreCase))
		{
			OptionId = Item.Id;
			break;
		}
	}

	dropdown->SetSelectedId(OptionId);
}

FString URobotConfigEditorWidget::GetDropdownSelection(const UBaseDropdownWidget* dropdown)
{
	if (!dropdown || dropdown->GetSelectedId().IsNone())
	{
		return FString();
	}

	return dropdown->GetSelectedId().ToString();
}

bool URobotConfigEditorWidget::IsBaseCheckBoxChecked(const UBaseCheckBoxWidget* checkBox)
{
	return checkBox && checkBox->GetCheckState() == ECheckBoxState::Checked;
}

void URobotConfigEditorWidget::SetBaseCheckBoxChecked(UBaseCheckBoxWidget* checkBox, const bool bChecked)
{
	if (checkBox)
	{
		checkBox->SetCheckState(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
