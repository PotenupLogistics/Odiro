#include "Platform/Widget/RobotConfigEditorWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"

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
	if (BodyTabButton)
	{
		BodyTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleBodyTabClicked);
		BodyTabButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleBodyTabClicked);
	}
	if (DriveTabButton)
	{
		DriveTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleDriveTabClicked);
		DriveTabButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleDriveTabClicked);
	}
	if (LidarTabButton)
	{
		LidarTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleLidarTabClicked);
		LidarTabButton->OnClicked.AddDynamic(this, &URobotConfigEditorWidget::HandleLidarTabClicked);
	}

	LoadProfileFromViewModel();
	ShowProfileSection(ERobotProfileSection::Body);
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
	if (BodyTabButton)
	{
		BodyTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleBodyTabClicked);
	}
	if (DriveTabButton)
	{
		DriveTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleDriveTabClicked);
	}
	if (LidarTabButton)
	{
		LidarTabButton->OnClicked.RemoveDynamic(this, &URobotConfigEditorWidget::HandleLidarTabClicked);
	}

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

void URobotConfigEditorWidget::HandleBodyTabClicked()
{
	ShowProfileSection(ERobotProfileSection::Body);
	SetProfileStatus(TEXT("Body profile fields are active."));
}

void URobotConfigEditorWidget::HandleDriveTabClicked()
{
	ShowProfileSection(ERobotProfileSection::Drive);
	SetProfileStatus(TEXT("Drive profile fields are active."));
}

void URobotConfigEditorWidget::HandleLidarTabClicked()
{
	ShowProfileSection(ERobotProfileSection::Lidar);
	SetProfileStatus(TEXT("LiDAR profile fields are active."));
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

	SetInputText(BodyLengthInput.Get(), viewModel->GetBodyLengthM());
	SetInputText(BodyWidthInput.Get(), viewModel->GetBodyWidthM());
	SetInputText(BodyHeightInput.Get(), viewModel->GetBodyHeightM());
	SetInputText(BodyWheelBaseInput.Get(), viewModel->GetBodyWheelBaseM());
	SetInputText(BodyTurningRadiusInput.Get(), viewModel->GetBodyTurningRadiusM());
	SetInputText(DriveMaxSpeedInput.Get(), viewModel->GetDriveMaxSpeedKmh());
	SetInputText(DriveSteeringGainInput.Get(), viewModel->GetDriveSteeringRatePerS());
	SetInputText(DriveMassInput.Get(), viewModel->GetDriveMassKg());
	SetInputText(LidarRangeInput.Get(), viewModel->GetLidarScanRangeM());
	SetInputText(LidarFrontAngleInput.Get(), viewModel->GetLidarFrontHalfAngleDegree());
	SetInputText(LidarAngleStepInput.Get(), viewModel->GetLidarAngleStepDegree());
	SetInputText(LidarScanRateInput.Get(), viewModel->GetLidarScanRateHz());
}

void URobotConfigEditorWidget::ShowProfileSection(const ERobotProfileSection section) const
{
	if (BodyFieldsBox)
	{
		BodyFieldsBox->SetVisibility(
			section == ERobotProfileSection::Body ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (DriveFieldsBox)
	{
		DriveFieldsBox->SetVisibility(
			section == ERobotProfileSection::Drive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LiDARFieldsBox)
	{
		LiDARFieldsBox->SetVisibility(
			section == ERobotProfileSection::Lidar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

FString URobotConfigEditorWidget::FormatProfileFloat(const float value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
