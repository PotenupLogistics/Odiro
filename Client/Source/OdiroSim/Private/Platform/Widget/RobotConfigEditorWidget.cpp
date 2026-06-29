#include "Platform/Widget/RobotConfigEditorWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "UI/BaseSliderWidget.h"

void URobotConfigEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReloadProfileButton)
	{
		ReloadProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleReloadProfileClicked);
		ReloadProfileButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleReloadProfileClicked);
	}
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
	BindProfileSlider(DriveSteeringGainSlider.Get(), this);
	BindProfileSlider(DriveMassSlider.Get(), this);
	BindProfileSlider(LidarRangeSlider.Get(), this);
	BindProfileSlider(LidarFrontAngleSlider.Get(), this);
	BindProfileSlider(LidarAngleStepSlider.Get(), this);
	BindProfileSlider(LidarScanRateSlider.Get(), this);

	LoadProfileFromViewModel();
	ShowAllProfileSections();
}

void URobotConfigEditorWidget::NativeDestruct()
{
	if (ReloadProfileButton)
	{
		ReloadProfileButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleReloadProfileClicked);
	}
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
	UnbindProfileSlider(DriveSteeringGainSlider.Get(), this);
	UnbindProfileSlider(DriveMassSlider.Get(), this);
	UnbindProfileSlider(LidarRangeSlider.Get(), this);
	UnbindProfileSlider(LidarFrontAngleSlider.Get(), this);
	UnbindProfileSlider(LidarAngleStepSlider.Get(), this);
	UnbindProfileSlider(LidarScanRateSlider.Get(), this);

	Super::NativeDestruct();
}

void URobotConfigEditorWidget::HandleReloadProfileClicked()
{
	LoadProfileFromViewModel();
}

void URobotConfigEditorWidget::HandleResetProfileClicked()
{
	if (!ResolveViewModel())
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		return;
	}

	ApplyViewModelToFields();
	SetProfileStatus(TEXT("Inputs restored to the last loaded profile values."));
}

void URobotConfigEditorWidget::HandleSaveProfileClicked()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		return;
	}

	if (!ReadFieldsIntoViewModel())
	{
		return;
	}

	if (!viewModel->SaveRobotProfile())
	{
		SetProfileStatus(viewModel->GetDiagnosticsText());
		return;
	}

	SetProfilePathText(viewModel->GetProfilePath());
	SetProfileStatus(viewModel->GetDiagnosticsText());
}

void URobotConfigEditorWidget::HandleProfileSliderChanged(UWidget* widget, const float value)
{
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
	else if (widget == LidarFrontAngleSlider.Get())
	{
		SetInputText(LidarFrontAngleInput.Get(), value);
	}
	else if (widget == LidarAngleStepSlider.Get())
	{
		SetInputText(LidarAngleStepInput.Get(), value);
	}
	else if (widget == LidarScanRateSlider.Get())
	{
		SetInputText(LidarScanRateInput.Get(), value);
	}
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
		return false;
	}

	const bool bLoaded = viewModel->LoadFromActiveProject();
	SetProfilePathText(viewModel->GetProfilePath());
	if (!bLoaded)
	{
		SetProfileStatus(viewModel->GetDiagnosticsText());
		return false;
	}

	ApplyViewModelToFields();
	SetProfileStatus(TEXT("Profile loaded."));
	return true;
}

bool URobotConfigEditorWidget::ReadFieldsIntoViewModel()
{
	URobotProfileViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetProfileStatus(TEXT("Robot profile ViewModel is not available."));
		return false;
	}

	float bodyLengthM = 0.0f;
	float bodyWidthM = 0.0f;
	float bodyHeightM = 0.0f;
	float bodyWheelBaseM = 0.0f;
	float bodyTurningRadiusM = 0.0f;
	float driveMaxSpeedKmh = 0.0f;
	float driveSteeringRatePerS = 0.0f;
	float driveMassKg = 0.0f;
	float lidarScanRangeM = 0.0f;
	float lidarFrontHalfAngleDegree = 0.0f;
	float lidarAngleStepDegree = 0.0f;
	float lidarScanRateHz = 0.0f;
	if (!TryReadFloatField(BodyLengthInput.Get(), TEXT("Body Length"), bodyLengthM)
		|| !TryReadFloatField(BodyWidthInput.Get(), TEXT("Body Width"), bodyWidthM)
		|| !TryReadFloatField(BodyHeightInput.Get(), TEXT("Body Height"), bodyHeightM)
		|| !TryReadFloatField(BodyWheelBaseInput.Get(), TEXT("Wheel Base"), bodyWheelBaseM)
		|| !TryReadFloatField(BodyTurningRadiusInput.Get(), TEXT("Turning Radius"), bodyTurningRadiusM)
		|| !TryReadFloatField(DriveMaxSpeedInput.Get(), TEXT("Max Speed"), driveMaxSpeedKmh)
		|| !TryReadFloatField(DriveSteeringGainInput.Get(), TEXT("Steering Rate"), driveSteeringRatePerS)
		|| !TryReadFloatField(DriveMassInput.Get(), TEXT("Mass"), driveMassKg)
		|| !TryReadFloatField(LidarRangeInput.Get(), TEXT("LiDAR Scan Range"), lidarScanRangeM)
		|| !TryReadFloatField(LidarFrontAngleInput.Get(), TEXT("LiDAR Front Angle"), lidarFrontHalfAngleDegree)
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
	viewModel->SetDriveSteeringRatePerS(driveSteeringRatePerS);
	viewModel->SetDriveMassKg(driveMassKg);
	viewModel->SetLidarScanRangeM(lidarScanRangeM);
	viewModel->SetLidarFrontHalfAngleDegree(lidarFrontHalfAngleDegree);
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
		return false;
	}

	const FString rawText = input->GetText().ToString().TrimStartAndEnd();
	if (!LexTryParseString(outValue, *rawText))
	{
		SetProfileStatus(FString::Printf(TEXT("%s must be a number."), *label));
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
		DriveSteeringGainInput.Get(),
		DriveSteeringGainSlider.Get(),
		viewModel->GetDriveSteeringRatePerS());
	SetLinkedSliderFieldValue(DriveMassInput.Get(), DriveMassSlider.Get(), viewModel->GetDriveMassKg());
	SetLinkedSliderFieldValue(LidarRangeInput.Get(), LidarRangeSlider.Get(), viewModel->GetLidarScanRangeM());
	SetLinkedSliderFieldValue(
		LidarFrontAngleInput.Get(),
		LidarFrontAngleSlider.Get(),
		viewModel->GetLidarFrontHalfAngleDegree());
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

FString URobotConfigEditorWidget::FormatProfileFloat(const float value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
