#include "Platform/Widget/RobotConfigEditorWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "UI/BaseSliderWidget.h"

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
	BindProfileSlider(LidarFrontAngleSlider.Get(), this);
	BindProfileSlider(LidarStopDistanceSlider.Get(), this);
	BindProfileSlider(LidarSlowDownDistanceSlider.Get(), this);
	BindProfileSlider(LidarAngleStepSlider.Get(), this);
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
	BindProfileInput(LidarFrontAngleInput.Get(), this);
	BindProfileInput(LidarStopDistanceInput.Get(), this);
	BindProfileInput(LidarSlowDownDistanceInput.Get(), this);
	BindProfileInput(LidarAngleStepInput.Get(), this);
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
	UnbindProfileSlider(LidarFrontAngleSlider.Get(), this);
	UnbindProfileSlider(LidarStopDistanceSlider.Get(), this);
	UnbindProfileSlider(LidarSlowDownDistanceSlider.Get(), this);
	UnbindProfileSlider(LidarAngleStepSlider.Get(), this);
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
	UnbindProfileInput(LidarFrontAngleInput.Get(), this);
	UnbindProfileInput(LidarStopDistanceInput.Get(), this);
	UnbindProfileInput(LidarSlowDownDistanceInput.Get(), this);
	UnbindProfileInput(LidarAngleStepInput.Get(), this);
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

	Super::NativeDestruct();
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
	else if (widget == LidarFrontAngleSlider.Get())
	{
		SetInputText(LidarFrontAngleInput.Get(), value);
	}
	else if (widget == LidarStopDistanceSlider.Get())
	{
		SetInputText(LidarStopDistanceInput.Get(), value);
	}
	else if (widget == LidarSlowDownDistanceSlider.Get())
	{
		SetInputText(LidarSlowDownDistanceInput.Get(), value);
	}
	else if (widget == LidarAngleStepSlider.Get())
	{
		SetInputText(LidarAngleStepInput.Get(), value);
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
	float lidarFrontHalfAngleDegree = 0.0f;
	float lidarStopDistanceM = 0.0f;
	float lidarSlowDownDistanceM = 0.0f;
	float lidarAngleStepDegree = 0.0f;
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
	viewModel->SetLidarFrontHalfAngleDegree(lidarFrontHalfAngleDegree);
	viewModel->SetLidarStopDistanceM(lidarStopDistanceM);
	viewModel->SetLidarSlowDownDistanceM(lidarSlowDownDistanceM);
	viewModel->SetLidarAngleStepDegree(lidarAngleStepDegree);
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
		LidarFrontAngleInput.Get(),
		LidarFrontAngleSlider.Get(),
		viewModel->GetLidarFrontHalfAngleDegree());
	SetLinkedSliderFieldValue(
		LidarStopDistanceInput.Get(),
		LidarStopDistanceSlider.Get(),
		viewModel->GetLidarStopDistanceM());
	SetLinkedSliderFieldValue(
		LidarSlowDownDistanceInput.Get(),
		LidarSlowDownDistanceSlider.Get(),
		viewModel->GetLidarSlowDownDistanceM());
	SetLinkedSliderFieldValue(
		LidarAngleStepInput.Get(),
		LidarAngleStepSlider.Get(),
		viewModel->GetLidarAngleStepDegree());
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

void URobotConfigEditorWidget::MarkProfileDirty()
{
	if (bApplyingProfileFields)
	{
		return;
	}

	SetProfileStatus(TEXT("변경사항이 있습니다"));
	SetProfileStateDirty();
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
	}
}

void URobotConfigEditorWidget::UnbindProfileInput(UEditableText* input, URobotConfigEditorWidget* owner)
{
	if (input && owner)
	{
		input->OnTextChanged.RemoveDynamic(owner, &URobotConfigEditorWidget::HandleProfileInputTextChanged);
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
