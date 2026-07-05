#include "Platform/Widget/RobotConfigEditorWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
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
#include "UI/BaseSliderWidget.h"

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
}

void URobotConfigEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResetProfileButton)
	{
		ResetProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
		ResetProfileButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
	}
	if (SaveProfileButton)
	{
		SaveProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
		SaveProfileButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
	}
	if (RotateLeftButton)
	{
		RotateLeftButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
		RotateLeftButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
	}
	if (ResetPreviewRotationButton)
	{
		ResetPreviewRotationButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
		ResetPreviewRotationButton->OnClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
	}
	if (RotateRightButton)
	{
		RotateRightButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
		RotateRightButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
	}
	if (DrawLidarRaysButton)
	{
		DrawLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
		DrawLidarRaysButton->OnClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysButton)
	{
		ToggleLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
		ToggleLidarRaysButton->OnClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ClearLidarRaysButton)
	{
		ClearLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
		ClearLidarRaysButton->OnClicked.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysCheckBox)
	{
		ToggleLidarRaysCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewToggleChanged);
		ToggleLidarRaysCheckBox->OnCheckStateChanged.AddDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewToggleChanged);
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
	BindProfileSlider(BodyLengthSlider.Get(), this);
	BindProfileSlider(BodyWidthSlider.Get(), this);
	BindProfileSlider(BodyHeightSlider.Get(), this);
	BindProfileSlider(BodyWheelBaseSlider.Get(), this);
	BindProfileSlider(BodyTurningRadiusSlider.Get(), this);
	BindProfileSlider(DriveMaxSpeedSlider.Get(), this);
	BindProfileSlider(DriveReverseSpeedSlider.Get(), this);
	BindProfileSlider(DriveAccelerationSlider.Get(), this);
	BindProfileSlider(DriveDecelerationSlider.Get(), this);
	BindProfileSlider(DriveSteeringGainSlider.Get(), this);
	BindProfileSlider(DriveMassSlider.Get(), this);
	BindProfileSlider(LidarRangeSlider.Get(), this);
	BindProfileSlider(LidarSensorHeightSlider.Get(), this);
	BindProfileSlider(LidarSensorForwardOffsetSlider.Get(), this);
	BindProfileSlider(LidarSensorRightOffsetSlider.Get(), this);
	BindProfileSlider(LidarFrontAngleSlider.Get(), this);
	BindProfileSlider(LidarStopDistanceSlider.Get(), this);
	BindProfileSlider(LidarObstacleWarningDistanceSlider.Get(), this);
	BindProfileSlider(LidarSlowDownDistanceSlider.Get(), this);
	BindProfileSlider(LidarAngleStepSlider.Get(), this);
	BindProfileSlider(LidarVerticalMinSlider.Get(), this);
	BindProfileSlider(LidarVerticalMaxSlider.Get(), this);
	BindProfileSlider(LidarVerticalStepSlider.Get(), this);
	BindProfileSlider(LidarScanRateSlider.Get(), this);
	BindProfileInput(BodyLengthInput.Get(), this);
	BindProfileInput(BodyWidthInput.Get(), this);
	BindProfileInput(BodyHeightInput.Get(), this);
	BindProfileInput(BodyWheelBaseInput.Get(), this);
	BindProfileInput(BodyTurningRadiusInput.Get(), this);
	BindProfileInput(DriveMaxSpeedInput.Get(), this);
	BindProfileInput(DriveReverseSpeedInput.Get(), this);
	BindProfileInput(DriveAccelerationInput.Get(), this);
	BindProfileInput(DriveDecelerationInput.Get(), this);
	BindProfileInput(DriveSteeringGainInput.Get(), this);
	BindProfileInput(DriveMassInput.Get(), this);
	BindProfileInput(LidarRangeInput.Get(), this);
	BindProfileInput(LidarSensorHeightInput.Get(), this);
	BindProfileInput(LidarSensorForwardOffsetInput.Get(), this);
	BindProfileInput(LidarSensorRightOffsetInput.Get(), this);
	BindProfileInput(LidarFrontAngleInput.Get(), this);
	BindProfileInput(LidarStopDistanceInput.Get(), this);
	BindProfileInput(LidarObstacleWarningDistanceInput.Get(), this);
	BindProfileInput(LidarSlowDownDistanceInput.Get(), this);
	BindProfileInput(LidarAngleStepInput.Get(), this);
	BindProfileInput(LidarVerticalMinInput.Get(), this);
	BindProfileInput(LidarVerticalMaxInput.Get(), this);
	BindProfileInput(LidarVerticalStepInput.Get(), this);
	BindProfileInput(LidarScanRateInput.Get(), this);
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
		ResetProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleResetProfileClicked);
	}
	if (SaveProfileButton)
	{
		SaveProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleSaveProfileClicked);
	}
	if (RotateLeftButton)
	{
		RotateLeftButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewLeftClicked);
	}
	if (ResetPreviewRotationButton)
	{
		ResetPreviewRotationButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleResetPreviewRotationClicked);
	}
	if (RotateRightButton)
	{
		RotateRightButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleRotatePreviewRightClicked);
	}
	if (DrawLidarRaysButton)
	{
		DrawLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysButton)
	{
		ToggleLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked);
	}
	if (ClearLidarRaysButton)
	{
		ClearLidarRaysButton->OnClicked.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked);
	}
	if (ToggleLidarRaysCheckBox)
	{
		ToggleLidarRaysCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&URobotConfigEditorWidget::HandleLidarPreviewToggleChanged);
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
	UnbindProfileSlider(BodyLengthSlider.Get(), this);
	UnbindProfileSlider(BodyWidthSlider.Get(), this);
	UnbindProfileSlider(BodyHeightSlider.Get(), this);
	UnbindProfileSlider(BodyWheelBaseSlider.Get(), this);
	UnbindProfileSlider(BodyTurningRadiusSlider.Get(), this);
	UnbindProfileSlider(DriveMaxSpeedSlider.Get(), this);
	UnbindProfileSlider(DriveReverseSpeedSlider.Get(), this);
	UnbindProfileSlider(DriveAccelerationSlider.Get(), this);
	UnbindProfileSlider(DriveDecelerationSlider.Get(), this);
	UnbindProfileSlider(DriveSteeringGainSlider.Get(), this);
	UnbindProfileSlider(DriveMassSlider.Get(), this);
	UnbindProfileSlider(LidarRangeSlider.Get(), this);
	UnbindProfileSlider(LidarSensorHeightSlider.Get(), this);
	UnbindProfileSlider(LidarSensorForwardOffsetSlider.Get(), this);
	UnbindProfileSlider(LidarSensorRightOffsetSlider.Get(), this);
	UnbindProfileSlider(LidarFrontAngleSlider.Get(), this);
	UnbindProfileSlider(LidarStopDistanceSlider.Get(), this);
	UnbindProfileSlider(LidarObstacleWarningDistanceSlider.Get(), this);
	UnbindProfileSlider(LidarSlowDownDistanceSlider.Get(), this);
	UnbindProfileSlider(LidarAngleStepSlider.Get(), this);
	UnbindProfileSlider(LidarVerticalMinSlider.Get(), this);
	UnbindProfileSlider(LidarVerticalMaxSlider.Get(), this);
	UnbindProfileSlider(LidarVerticalStepSlider.Get(), this);
	UnbindProfileSlider(LidarScanRateSlider.Get(), this);
	UnbindProfileInput(BodyLengthInput.Get(), this);
	UnbindProfileInput(BodyWidthInput.Get(), this);
	UnbindProfileInput(BodyHeightInput.Get(), this);
	UnbindProfileInput(BodyWheelBaseInput.Get(), this);
	UnbindProfileInput(BodyTurningRadiusInput.Get(), this);
	UnbindProfileInput(DriveMaxSpeedInput.Get(), this);
	UnbindProfileInput(DriveReverseSpeedInput.Get(), this);
	UnbindProfileInput(DriveAccelerationInput.Get(), this);
	UnbindProfileInput(DriveDecelerationInput.Get(), this);
	UnbindProfileInput(DriveSteeringGainInput.Get(), this);
	UnbindProfileInput(DriveMassInput.Get(), this);
	UnbindProfileInput(LidarRangeInput.Get(), this);
	UnbindProfileInput(LidarSensorHeightInput.Get(), this);
	UnbindProfileInput(LidarSensorForwardOffsetInput.Get(), this);
	UnbindProfileInput(LidarSensorRightOffsetInput.Get(), this);
	UnbindProfileInput(LidarFrontAngleInput.Get(), this);
	UnbindProfileInput(LidarStopDistanceInput.Get(), this);
	UnbindProfileInput(LidarObstacleWarningDistanceInput.Get(), this);
	UnbindProfileInput(LidarSlowDownDistanceInput.Get(), this);
	UnbindProfileInput(LidarAngleStepInput.Get(), this);
	UnbindProfileInput(LidarVerticalMinInput.Get(), this);
	UnbindProfileInput(LidarVerticalMaxInput.Get(), this);
	UnbindProfileInput(LidarVerticalStepInput.Get(), this);
	UnbindProfileInput(LidarScanRateInput.Get(), this);
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

void URobotConfigEditorWidget::HandleResetProfileClicked()
{
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

void URobotConfigEditorWidget::HandleSaveProfileClicked()
{
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

void URobotConfigEditorWidget::HandleProfileSliderChanged(UWidget* widget, const float value)
{
	bool bHandled = true;
	if (widget == BodyLengthSlider.Get())
	{
		SetInputText(BodyLengthInput.Get(), value);
	}
	else if (widget == BodyWidthSlider.Get())
	{
		SetInputText(BodyWidthInput.Get(), value);
	}
	else if (widget == BodyHeightSlider.Get())
	{
		SetInputText(BodyHeightInput.Get(), value);
	}
	else if (widget == BodyWheelBaseSlider.Get())
	{
		SetInputText(BodyWheelBaseInput.Get(), value);
	}
	else if (widget == BodyTurningRadiusSlider.Get())
	{
		SetInputText(BodyTurningRadiusInput.Get(), value);
	}
	else if (widget == DriveMaxSpeedSlider.Get())
	{
		SetInputText(DriveMaxSpeedInput.Get(), value);
	}
	else if (widget == DriveReverseSpeedSlider.Get())
	{
		SetInputText(DriveReverseSpeedInput.Get(), value);
	}
	else if (widget == DriveAccelerationSlider.Get())
	{
		SetInputText(DriveAccelerationInput.Get(), value);
	}
	else if (widget == DriveDecelerationSlider.Get())
	{
		SetInputText(DriveDecelerationInput.Get(), value);
	}
	else if (widget == DriveSteeringGainSlider.Get())
	{
		SetInputText(DriveSteeringGainInput.Get(), value);
	}
	else if (widget == DriveMassSlider.Get())
	{
		SetInputText(DriveMassInput.Get(), value);
	}
	else if (widget == LidarRangeSlider.Get())
	{
		SetInputText(LidarRangeInput.Get(), value);
	}
	else if (widget == LidarSensorHeightSlider.Get())
	{
		SetInputText(LidarSensorHeightInput.Get(), value);
	}
	else if (widget == LidarSensorForwardOffsetSlider.Get())
	{
		SetInputText(LidarSensorForwardOffsetInput.Get(), value);
	}
	else if (widget == LidarSensorRightOffsetSlider.Get())
	{
		SetInputText(LidarSensorRightOffsetInput.Get(), value);
	}
	else if (widget == LidarFrontAngleSlider.Get())
	{
		SetInputText(LidarFrontAngleInput.Get(), value);
	}
	else if (widget == LidarStopDistanceSlider.Get())
	{
		SetInputText(LidarStopDistanceInput.Get(), value);
	}
	else if (widget == LidarObstacleWarningDistanceSlider.Get())
	{
		SetInputText(LidarObstacleWarningDistanceInput.Get(), value);
	}
	else if (widget == LidarSlowDownDistanceSlider.Get())
	{
		SetInputText(LidarSlowDownDistanceInput.Get(), value);
	}
	else if (widget == LidarAngleStepSlider.Get())
	{
		SetInputText(LidarAngleStepInput.Get(), value);
	}
	else if (widget == LidarVerticalMinSlider.Get())
	{
		SetInputText(LidarVerticalMinInput.Get(), value);
	}
	else if (widget == LidarVerticalMaxSlider.Get())
	{
		SetInputText(LidarVerticalMaxInput.Get(), value);
	}
	else if (widget == LidarVerticalStepSlider.Get())
	{
		SetInputText(LidarVerticalStepInput.Get(), value);
	}
	else if (widget == LidarScanRateSlider.Get())
	{
		SetInputText(LidarScanRateInput.Get(), value);
	}
	else
	{
		bHandled = false;
	}

	if (bHandled)
	{
		MarkProfileDirty();
	}
}

void URobotConfigEditorWidget::HandleProfileInputTextChanged(const FText& text)
{
	(void)text;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleProfileInputTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	(void)text;
	(void)commitMethod;
	SyncProfileSlidersFromValidInputFields();
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarModeSelectionChanged(
	FString selectedItem,
	ESelectInfo::Type selectionType)
{
	(void)selectedItem;
	(void)selectionType;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarDrawDebugChanged(const bool bIsChecked)
{
	(void)bIsChecked;
	MarkProfileDirty();
}

void URobotConfigEditorWidget::HandleLidarPreviewOptionChanged(const bool bIsChecked)
{
	(void)bIsChecked;
	ApplyRobotPreviewDisplayOptions();
}

void URobotConfigEditorWidget::HandleLidarPreviewDensitySelectionChanged(
	FString selectedItem,
	ESelectInfo::Type selectionType)
{
	(void)selectedItem;
	(void)selectionType;
	ApplyRobotPreviewDisplayOptions();
}

void URobotConfigEditorWidget::HandleLidarPreviewToggleChanged(const bool bIsChecked)
{
	if (bSyncingLidarPreviewToggleState)
	{
		return;
	}

	SetLidarPreviewRaysVisible(bIsChecked);
}

void URobotConfigEditorWidget::HandleRotatePreviewLeftClicked()
{
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

void URobotConfigEditorWidget::HandleResetPreviewRotationClicked()
{
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

void URobotConfigEditorWidget::HandleRotatePreviewRightClicked()
{
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

void URobotConfigEditorWidget::HandleDrawLidarPreviewRaysClicked()
{
	URobotPreviewSubsystem* PreviewSubsystem = ResolveRobotPreviewSubsystem();
	const bool bShouldShow = !PreviewSubsystem || !PreviewSubsystem->AreLidarPreviewRaysVisible();
	SetLidarPreviewRaysVisible(bShouldShow);
}

void URobotConfigEditorWidget::HandleClearLidarPreviewRaysClicked()
{
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
	if (!TryReadFloatField(BodyLengthInput.Get(), TEXT("Body Length"), bodyLengthM)
		|| !TryReadFloatField(BodyWidthInput.Get(), TEXT("Body Width"), bodyWidthM)
		|| !TryReadFloatField(BodyHeightInput.Get(), TEXT("Body Height"), bodyHeightM)
		|| !TryReadFloatField(BodyWheelBaseInput.Get(), TEXT("Wheel Base"), bodyWheelBaseM)
		|| !TryReadFloatField(BodyTurningRadiusInput.Get(), TEXT("Turning Radius"), bodyTurningRadiusM)
		|| !TryReadFloatField(DriveMaxSpeedInput.Get(), TEXT("Max Speed"), driveMaxSpeedKmh)
		|| !TryReadFloatField(DriveReverseSpeedInput.Get(), TEXT("Max Reverse Speed"), driveMaxReverseSpeedKmh)
		|| !TryReadFloatField(DriveAccelerationInput.Get(), TEXT("Acceleration Rate"), driveAccelerationRateKmhPerSecond)
		|| !TryReadFloatField(DriveDecelerationInput.Get(), TEXT("Deceleration Rate"), driveDecelerationRateKmhPerSecond)
		|| !TryReadFloatField(DriveSteeringGainInput.Get(), TEXT("Steering Rate"), driveSteeringRatePerS)
		|| !TryReadFloatField(DriveMassInput.Get(), TEXT("Mass"), driveMassKg)
		|| !TryReadFloatField(LidarRangeInput.Get(), TEXT("LiDAR Scan Range"), lidarScanRangeM)
		|| !TryReadFloatField(LidarSensorHeightInput.Get(), TEXT("LiDAR Sensor Height"), lidarSensorHeightM)
		|| !TryReadFloatField(LidarFrontAngleInput.Get(), TEXT("LiDAR Front Angle"), lidarFrontHalfAngleDegree)
		|| !TryReadFloatField(LidarStopDistanceInput.Get(), TEXT("LiDAR Stop Distance"), lidarStopDistanceM)
		|| !TryReadFloatField(LidarSlowDownDistanceInput.Get(), TEXT("LiDAR Slowdown Distance"), lidarSlowDownDistanceM)
		|| !TryReadFloatField(LidarAngleStepInput.Get(), TEXT("LiDAR Angle Step"), lidarAngleStepDegree)
		|| !TryReadFloatField(LidarScanRateInput.Get(), TEXT("LiDAR Scan Rate"), lidarScanRateHz))
	{
		return false;
	}
	if (!TryReadOptionalFloatField(
			LidarSensorForwardOffsetInput.Get(),
			TEXT("LiDAR Sensor Forward Offset"),
			lidarSensorForwardOffsetM)
		|| !TryReadOptionalFloatField(
			LidarSensorRightOffsetInput.Get(),
			TEXT("LiDAR Sensor Right Offset"),
			lidarSensorRightOffsetM)
		|| !TryReadOptionalFloatField(
			LidarObstacleWarningDistanceInput.Get(),
			TEXT("LiDAR Obstacle Warning Distance"),
			lidarObstacleWarningDistanceM)
		|| !TryReadOptionalFloatField(
			LidarVerticalMinInput.Get(),
			TEXT("LiDAR Vertical Min"),
			lidarVerticalMinDegree)
		|| !TryReadOptionalFloatField(
			LidarVerticalMaxInput.Get(),
			TEXT("LiDAR Vertical Max"),
			lidarVerticalMaxDegree)
		|| !TryReadOptionalFloatField(
			LidarVerticalStepInput.Get(),
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
		viewModel->SetLidarMode(LidarModeComboBox->GetSelectedOption());
	}
	if (LidarDrawDebugCheckBox)
	{
		viewModel->SetLidarDrawDebug(LidarDrawDebugCheckBox->IsChecked());
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
	UEditableText* input,
	const FString& label,
	float& outValue)
{
	if (!input)
	{
		SetProfileStatus(FString::Printf(TEXT("%s input widget is missing."), *label));
		SetProfileStateError(TEXT("입력값 오류"));
		return false;
	}

	const FString rawText = input->GetText().ToString().TrimStartAndEnd();
	if (!LexTryParseString(outValue, *rawText))
	{
		SetProfileStatus(FString::Printf(TEXT("%s must be a number."), *label));
		SetProfileStateError(TEXT("입력값 오류"));
		input->SetKeyboardFocus();
		return false;
	}

	return true;
}

bool URobotConfigEditorWidget::TryReadOptionalFloatField(
	UEditableText* input,
	const FString& label,
	float& outValue)
{
	return !input || TryReadFloatField(input, label, outValue);
}

bool URobotConfigEditorWidget::TryReadFieldsIntoPreviewSettings(FRobotProfileSettings& outSettings) const
{
	FRobotProfileSettings previewSettings;
	if (!TryReadPreviewFloatField(BodyLengthInput.Get(), previewSettings.Body.LengthM)
		|| !TryReadPreviewFloatField(BodyWidthInput.Get(), previewSettings.Body.WidthM)
		|| !TryReadPreviewFloatField(BodyHeightInput.Get(), previewSettings.Body.HeightM)
		|| !TryReadPreviewFloatField(BodyWheelBaseInput.Get(), previewSettings.Body.WheelBaseM)
		|| !TryReadPreviewFloatField(BodyTurningRadiusInput.Get(), previewSettings.Body.TurningRadiusM)
		|| !TryReadPreviewFloatField(DriveMaxSpeedInput.Get(), previewSettings.Drive.MaxSpeedKmh)
		|| !TryReadPreviewFloatField(DriveReverseSpeedInput.Get(), previewSettings.Drive.MaxReverseSpeedKmh)
		|| !TryReadPreviewFloatField(
			DriveAccelerationInput.Get(),
			previewSettings.Drive.AccelerationRateKmhPerSecond)
		|| !TryReadPreviewFloatField(
			DriveDecelerationInput.Get(),
			previewSettings.Drive.DecelerationRateKmhPerSecond)
		|| !TryReadPreviewFloatField(DriveSteeringGainInput.Get(), previewSettings.Drive.SteeringRatePerS)
		|| !TryReadPreviewFloatField(DriveMassInput.Get(), previewSettings.Drive.MassKg)
		|| !TryReadPreviewFloatField(LidarRangeInput.Get(), previewSettings.Lidar.ScanRangeM)
		|| !TryReadPreviewFloatField(LidarSensorHeightInput.Get(), previewSettings.Lidar.SensorHeightM)
		|| !TryReadPreviewFloatField(LidarFrontAngleInput.Get(), previewSettings.Lidar.FrontHalfAngleDegree)
		|| !TryReadPreviewFloatField(LidarStopDistanceInput.Get(), previewSettings.Lidar.StopDistanceM)
		|| !TryReadPreviewFloatField(LidarSlowDownDistanceInput.Get(), previewSettings.Lidar.SlowDownDistanceM)
		|| !TryReadPreviewFloatField(LidarAngleStepInput.Get(), previewSettings.Lidar.AngleStepDegree)
		|| !TryReadPreviewFloatField(LidarScanRateInput.Get(), previewSettings.Lidar.ScanRateHz))
	{
		return false;
	}
	if (!TryReadOptionalPreviewFloatField(
			LidarSensorForwardOffsetInput.Get(),
			previewSettings.Lidar.SensorForwardOffsetM)
		|| !TryReadOptionalPreviewFloatField(
			LidarSensorRightOffsetInput.Get(),
			previewSettings.Lidar.SensorRightOffsetM)
		|| !TryReadOptionalPreviewFloatField(
			LidarObstacleWarningDistanceInput.Get(),
			previewSettings.Lidar.ObstacleWarningDistanceM)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalMinInput.Get(),
			previewSettings.Lidar.VerticalMinDegree)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalMaxInput.Get(),
			previewSettings.Lidar.VerticalMaxDegree)
		|| !TryReadOptionalPreviewFloatField(
			LidarVerticalStepInput.Get(),
			previewSettings.Lidar.VerticalStepDegree))
	{
		return false;
	}

	if (LidarModeComboBox)
	{
		const FString selectedMode = LidarModeComboBox->GetSelectedOption().TrimStartAndEnd();
		if (!selectedMode.IsEmpty())
		{
			previewSettings.Lidar.LidarMode = selectedMode;
		}
	}
	if (LidarDrawDebugCheckBox)
	{
		previewSettings.Lidar.bDrawDebug = LidarDrawDebugCheckBox->IsChecked();
	}

	outSettings = previewSettings;
	return true;
}

bool URobotConfigEditorWidget::TryReadPreviewFloatField(UEditableText* input, float& outValue)
{
	if (!input)
	{
		return false;
	}

	const FString rawText = input->GetText().ToString().TrimStartAndEnd();
	return LexTryParseString(outValue, *rawText);
}

bool URobotConfigEditorWidget::TryReadOptionalPreviewFloatField(UEditableText* input, float& outValue)
{
	return !input || TryReadPreviewFloatField(input, outValue);
}

void URobotConfigEditorWidget::ApplyViewModelToFields()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		return;
	}

	TGuardValue<bool> applyingGuard(bApplyingProfileFields, true);
	SetLinkedSliderFieldValue(BodyLengthInput.Get(), BodyLengthSlider.Get(), viewModel->GetBodyLengthM());
	SetLinkedSliderFieldValue(BodyWidthInput.Get(), BodyWidthSlider.Get(), viewModel->GetBodyWidthM());
	SetLinkedSliderFieldValue(BodyHeightInput.Get(), BodyHeightSlider.Get(), viewModel->GetBodyHeightM());
	SetLinkedSliderFieldValue(BodyWheelBaseInput.Get(), BodyWheelBaseSlider.Get(), viewModel->GetBodyWheelBaseM());
	SetLinkedSliderFieldValue(
		BodyTurningRadiusInput.Get(),
		BodyTurningRadiusSlider.Get(),
		viewModel->GetBodyTurningRadiusM());
	SetLinkedSliderFieldValue(DriveMaxSpeedInput.Get(), DriveMaxSpeedSlider.Get(), viewModel->GetDriveMaxSpeedKmh());
	SetLinkedSliderFieldValue(
		DriveReverseSpeedInput.Get(),
		DriveReverseSpeedSlider.Get(),
		viewModel->GetDriveMaxReverseSpeedKmh());
	SetLinkedSliderFieldValue(
		DriveAccelerationInput.Get(),
		DriveAccelerationSlider.Get(),
		viewModel->GetDriveAccelerationRateKmhPerSecond());
	SetLinkedSliderFieldValue(
		DriveDecelerationInput.Get(),
		DriveDecelerationSlider.Get(),
		viewModel->GetDriveDecelerationRateKmhPerSecond());
	SetLinkedSliderFieldValue(
		DriveSteeringGainInput.Get(),
		DriveSteeringGainSlider.Get(),
		viewModel->GetDriveSteeringRatePerS());
	SetLinkedSliderFieldValue(DriveMassInput.Get(), DriveMassSlider.Get(), viewModel->GetDriveMassKg());
	SetComboBoxSelection(LidarModeComboBox.Get(), viewModel->GetLidarMode());
	if (LidarDrawDebugCheckBox)
	{
		LidarDrawDebugCheckBox->SetIsChecked(viewModel->GetLidarDrawDebug());
	}
	SetLinkedSliderFieldValue(LidarRangeInput.Get(), LidarRangeSlider.Get(), viewModel->GetLidarScanRangeM());
	SetLinkedSliderFieldValue(
		LidarSensorHeightInput.Get(),
		LidarSensorHeightSlider.Get(),
		viewModel->GetLidarSensorHeightM());
	SetLinkedSliderFieldValue(
		LidarSensorForwardOffsetInput.Get(),
		LidarSensorForwardOffsetSlider.Get(),
		viewModel->GetLidarSensorForwardOffsetM());
	SetLinkedSliderFieldValue(
		LidarSensorRightOffsetInput.Get(),
		LidarSensorRightOffsetSlider.Get(),
		viewModel->GetLidarSensorRightOffsetM());
	SetLinkedSliderFieldValue(
		LidarFrontAngleInput.Get(),
		LidarFrontAngleSlider.Get(),
		viewModel->GetLidarFrontHalfAngleDegree());
	SetLinkedSliderFieldValue(
		LidarStopDistanceInput.Get(),
		LidarStopDistanceSlider.Get(),
		viewModel->GetLidarStopDistanceM());
	SetLinkedSliderFieldValue(
		LidarObstacleWarningDistanceInput.Get(),
		LidarObstacleWarningDistanceSlider.Get(),
		viewModel->GetLidarObstacleWarningDistanceM());
	SetLinkedSliderFieldValue(
		LidarSlowDownDistanceInput.Get(),
		LidarSlowDownDistanceSlider.Get(),
		viewModel->GetLidarSlowDownDistanceM());
	SetLinkedSliderFieldValue(
		LidarAngleStepInput.Get(),
		LidarAngleStepSlider.Get(),
		viewModel->GetLidarAngleStepDegree());
	SetLinkedSliderFieldValue(
		LidarVerticalMinInput.Get(),
		LidarVerticalMinSlider.Get(),
		viewModel->GetLidarVerticalMinDegree());
	SetLinkedSliderFieldValue(
		LidarVerticalMaxInput.Get(),
		LidarVerticalMaxSlider.Get(),
		viewModel->GetLidarVerticalMaxDegree());
	SetLinkedSliderFieldValue(
		LidarVerticalStepInput.Get(),
		LidarVerticalStepSlider.Get(),
		viewModel->GetLidarVerticalStepDegree());
	SetLinkedSliderFieldValue(LidarScanRateInput.Get(), LidarScanRateSlider.Get(), viewModel->GetLidarScanRateHz());
}

void URobotConfigEditorWidget::BindProfileSlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner)
{
	if (slider && owner)
	{
		slider->OnValueChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderChanged);
		slider->OnValueChanged.AddDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderChanged);
	}
}

void URobotConfigEditorWidget::UnbindProfileSlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner)
{
	if (slider && owner)
	{
		slider->OnValueChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileSliderChanged);
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
	SetRobotPreviewStatus(previewSubsystem->GetStatusText());
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

void URobotConfigEditorWidget::SyncProfileSlidersFromValidInputFields() const
{
	SyncLinkedSliderFromInput(BodyLengthInput.Get(), BodyLengthSlider.Get());
	SyncLinkedSliderFromInput(BodyWidthInput.Get(), BodyWidthSlider.Get());
	SyncLinkedSliderFromInput(BodyHeightInput.Get(), BodyHeightSlider.Get());
	SyncLinkedSliderFromInput(BodyWheelBaseInput.Get(), BodyWheelBaseSlider.Get());
	SyncLinkedSliderFromInput(BodyTurningRadiusInput.Get(), BodyTurningRadiusSlider.Get());
	SyncLinkedSliderFromInput(DriveMaxSpeedInput.Get(), DriveMaxSpeedSlider.Get());
	SyncLinkedSliderFromInput(DriveReverseSpeedInput.Get(), DriveReverseSpeedSlider.Get());
	SyncLinkedSliderFromInput(DriveAccelerationInput.Get(), DriveAccelerationSlider.Get());
	SyncLinkedSliderFromInput(DriveDecelerationInput.Get(), DriveDecelerationSlider.Get());
	SyncLinkedSliderFromInput(DriveSteeringGainInput.Get(), DriveSteeringGainSlider.Get());
	SyncLinkedSliderFromInput(DriveMassInput.Get(), DriveMassSlider.Get());
	SyncLinkedSliderFromInput(LidarRangeInput.Get(), LidarRangeSlider.Get());
	SyncLinkedSliderFromInput(LidarSensorHeightInput.Get(), LidarSensorHeightSlider.Get());
	SyncLinkedSliderFromInput(LidarSensorForwardOffsetInput.Get(), LidarSensorForwardOffsetSlider.Get());
	SyncLinkedSliderFromInput(LidarSensorRightOffsetInput.Get(), LidarSensorRightOffsetSlider.Get());
	SyncLinkedSliderFromInput(LidarFrontAngleInput.Get(), LidarFrontAngleSlider.Get());
	SyncLinkedSliderFromInput(LidarStopDistanceInput.Get(), LidarStopDistanceSlider.Get());
	SyncLinkedSliderFromInput(LidarObstacleWarningDistanceInput.Get(), LidarObstacleWarningDistanceSlider.Get());
	SyncLinkedSliderFromInput(LidarSlowDownDistanceInput.Get(), LidarSlowDownDistanceSlider.Get());
	SyncLinkedSliderFromInput(LidarAngleStepInput.Get(), LidarAngleStepSlider.Get());
	SyncLinkedSliderFromInput(LidarVerticalMinInput.Get(), LidarVerticalMinSlider.Get());
	SyncLinkedSliderFromInput(LidarVerticalMaxInput.Get(), LidarVerticalMaxSlider.Get());
	SyncLinkedSliderFromInput(LidarVerticalStepInput.Get(), LidarVerticalStepSlider.Get());
	SyncLinkedSliderFromInput(LidarScanRateInput.Get(), LidarScanRateSlider.Get());
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
	DisplayOptions.bShowRays = ShowLidarRaysCheckBox ? ShowLidarRaysCheckBox->IsChecked() : true;
	DisplayOptions.bShowRange = ShowLidarRangeCheckBox ? ShowLidarRangeCheckBox->IsChecked() : true;
	DisplayOptions.bShowPoints = ShowLidarPointsCheckBox ? ShowLidarPointsCheckBox->IsChecked() : false;

	if (LidarPreviewDensityComboBox)
	{
		const FString SelectedDensity = LidarPreviewDensityComboBox->GetSelectedOption().TrimStartAndEnd();
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

	if (ToggleLidarRaysCheckBox)
	{
		TGuardValue<bool> Guard(bSyncingLidarPreviewToggleState, true);
		ToggleLidarRaysCheckBox->SetIsChecked(bRaysVisible);
	}
	if (ToggleLidarRaysButton)
	{
		const FLinearColor RayButtonColor = bRaysVisible
			? FLinearColor(0.026f, 0.212f, 0.644f, 1.0f)
			: FLinearColor(0.075f, 0.095f, 0.115f, 1.0f);
		ToggleLidarRaysButton->SetBackgroundColor(RayButtonColor);
	}

	if (LidarPreviewOptionsPanel)
	{
		LidarPreviewOptionsPanel->SetIsEnabled(bRaysVisible);
		LidarPreviewOptionsPanel->SetRenderOpacity(bRaysVisible ? 1.0f : 0.42f);
		return;
	}

	if (ShowLidarRaysCheckBox)
	{
		ShowLidarRaysCheckBox->SetIsEnabled(bRaysVisible);
	}
	if (ShowLidarRangeCheckBox)
	{
		ShowLidarRangeCheckBox->SetIsEnabled(bRaysVisible);
	}
	if (ShowLidarPointsCheckBox)
	{
		ShowLidarPointsCheckBox->SetIsEnabled(bRaysVisible);
	}
	if (LidarPreviewDensityComboBox)
	{
		LidarPreviewDensityComboBox->SetIsEnabled(bRaysVisible);
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

void URobotConfigEditorWidget::BindProfileInput(UEditableText* input, URobotConfigEditorWidget* owner)
{
	if (input && owner)
	{
		input->OnTextChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextChanged);
		input->OnTextChanged.AddDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextChanged);
		input->OnTextCommitted.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextCommitted);
		input->OnTextCommitted.AddDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextCommitted);
	}
}

void URobotConfigEditorWidget::UnbindProfileInput(UEditableText* input, URobotConfigEditorWidget* owner)
{
	if (input && owner)
	{
		input->OnTextChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextChanged);
		input->OnTextCommitted.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextCommitted);
	}
}

void URobotConfigEditorWidget::SetInputText(UEditableText* input, const float value)
{
	if (input)
	{
		input->SetText(FText::FromString(FormatProfileFloat(value)));
	}
}

void URobotConfigEditorWidget::SetLinkedSliderFieldValue(
	UEditableText* input,
	UBaseSliderWidget* slider,
	const float value)
{
	SetInputText(input, value);
	if (slider)
	{
		slider->SetValue(value);
	}
}

void URobotConfigEditorWidget::SyncLinkedSliderFromInput(UEditableText* input, UBaseSliderWidget* slider)
{
	if (!input || !slider)
	{
		return;
	}

	float parsedValue = 0.0f;
	const FString rawText = input->GetText().ToString().TrimStartAndEnd();
	if (LexTryParseString(parsedValue, *rawText))
	{
		slider->SetValue(parsedValue);
	}
}

void URobotConfigEditorWidget::SetComboBoxSelection(UComboBoxString* comboBox, const FString& selectedOption)
{
	if (!comboBox || selectedOption.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	FString optionToSelect = selectedOption;
	for (int32 optionIndex = 0; optionIndex < comboBox->GetOptionCount(); ++optionIndex)
	{
		const FString option = comboBox->GetOptionAtIndex(optionIndex);
		if (option.Equals(selectedOption, ESearchCase::IgnoreCase))
		{
			optionToSelect = option;
			break;
		}
	}

	comboBox->SetSelectedOption(optionToSelect);
}

FString URobotConfigEditorWidget::FormatProfileFloat(const float value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
