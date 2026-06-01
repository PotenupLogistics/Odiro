#include "DeliveryBot/Component/DeliveryBot_ChaosDriveComponent.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Curves/RichCurve.h"

UDeliveryBot_ChaosDriveComponent::UDeliveryBot_ChaosDriveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeliveryBot_ChaosDriveComponent::ApplyMoveCommand(
	UChaosVehicleMovementComponent* vehicleMovement,
	const FDeliveryBotMoveCommandInfo& moveCommandInfo,
	float deltaTime)
{
	if (!IsValid(vehicleMovement))
		return;

	const float safeDeltaTime{ FMath::Max(deltaTime, 0.f) };

	if (moveCommandInfo.bBrake)
	{
		vehicleMovement->SetTargetGear(1, true);

		const float brakeInput{
			FMath::Clamp(
				moveCommandInfo.Brake > 0.f
					? FMath::Min(moveCommandInfo.Brake, DriveConfigInfo.StopBrakeInput)
					: DriveConfigInfo.StopBrakeInput,
				0.f,
				1.f
			)
		};

		ApplyDriveInput(
			vehicleMovement,
			0.f,
			moveCommandInfo.Steering,
			brakeInput,
			DriveConfigInfo.bUseHandbrakeWhenBrake,
			safeDeltaTime
		);
		return;
	}

	const float currentSpeedKmh{
		FMath::Abs(GetCmPerSecondToKmh(vehicleMovement->GetForwardSpeed()))
	};

	const float targetSpeedKmh{
		moveCommandInfo.bBrake
			? 0.f
			: FMath::Clamp(moveCommandInfo.TargetSpeedKmh, 0.f, DriveConfigInfo.MaxSpeedKmh)
	};

	const float speedErrorKmh{ targetSpeedKmh - currentSpeedKmh };
	const float speedControlRangeKmh{ FMath::Max(DriveConfigInfo.SlowdownSpeedRangeKmh, 0.1f) };

	const float throttle{
		FMath::Clamp(speedErrorKmh / speedControlRangeKmh, 0.f, 1.f)
	};

	float brake{ FMath::Clamp(moveCommandInfo.Brake, 0.f, 1.f) };

	if (moveCommandInfo.bBrake)
	{
		brake = FMath::Max(brake, 1.f);
	}
	else if (speedErrorKmh < -DriveConfigInfo.SpeedLimitToleranceKmh)
	{
		const float overspeedBrake{
			FMath::Clamp(-speedErrorKmh / speedControlRangeKmh, 0.f, 1.f)
		};

		brake = FMath::Max(brake, FMath::Min(overspeedBrake, DriveConfigInfo.SpeedLimitBrake));
	}

	ApplyDriveInput(
		vehicleMovement,
		throttle,
		moveCommandInfo.Steering,
		brake,
		false,
		safeDeltaTime
	);
}

void UDeliveryBot_ChaosDriveComponent::SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const
{
	if (!IsValid(wheeledMovement))
	{
		return;
	}

	wheeledMovement->bMechanicalSimEnabled = true;
	wheeledMovement->TransmissionSetup.bUseAutoReverse = false;

	wheeledMovement->EngineSetup.MaxTorque = DriveConfigInfo.MaxTorque;
	wheeledMovement->EngineSetup.MaxRPM = DriveConfigInfo.MaxRPM;
	wheeledMovement->EngineSetup.EngineIdleRPM = DriveConfigInfo.EngineIdleRPM;
	wheeledMovement->EngineSetup.EngineBrakeEffect = DriveConfigInfo.EngineBrakeEffect;
	wheeledMovement->EngineSetup.EngineRevUpMOI = DriveConfigInfo.EngineRevUpMOI;
	wheeledMovement->EngineSetup.EngineRevDownRate = DriveConfigInfo.EngineRevDownRate;

	SetupTorqueCurve(wheeledMovement);
}

void UDeliveryBot_ChaosDriveComponent::InitializeChaosDrive(
	UChaosWheeledVehicleMovementComponent* wheeledMovement,
	const FDeliveryBotChaosDriveConfigInfo& driveConfigInfo)
{
	DriveConfigInfo = driveConfigInfo;

	DriveConfigInfo.MaxSpeedKmh = FMath::Max(DriveConfigInfo.MaxSpeedKmh, 0.f);
	DriveConfigInfo.SlowdownSpeedRangeKmh = FMath::Max(DriveConfigInfo.SlowdownSpeedRangeKmh, 0.1f);
	DriveConfigInfo.SpeedLimitToleranceKmh = FMath::Max(DriveConfigInfo.SpeedLimitToleranceKmh, 0.f);
	DriveConfigInfo.SpeedLimitBrake = FMath::Clamp(DriveConfigInfo.SpeedLimitBrake, 0.f, 1.f);
	DriveConfigInfo.StopBrakeInput = FMath::Clamp(DriveConfigInfo.StopBrakeInput, 0.f, 1.f);
	DriveConfigInfo.ThrottleInputRatePerSecond = FMath::Max(DriveConfigInfo.ThrottleInputRatePerSecond, 0.f);
	DriveConfigInfo.BrakeInputRatePerSecond = FMath::Max(DriveConfigInfo.BrakeInputRatePerSecond, 0.f);
	DriveConfigInfo.SteeringInputRatePerSecond = FMath::Max(DriveConfigInfo.SteeringInputRatePerSecond, 0.f);
	DriveConfigInfo.MaxTorque = FMath::Max(DriveConfigInfo.MaxTorque, 0.f);
	DriveConfigInfo.MaxRPM = FMath::Max(DriveConfigInfo.MaxRPM, 1.f);
	DriveConfigInfo.EngineIdleRPM = FMath::Max(DriveConfigInfo.EngineIdleRPM, 0.f);
	DriveConfigInfo.EngineBrakeEffect = FMath::Max(DriveConfigInfo.EngineBrakeEffect, 0.f);
	DriveConfigInfo.EngineRevUpMOI = FMath::Max(DriveConfigInfo.EngineRevUpMOI, 0.f);
	DriveConfigInfo.EngineRevDownRate = FMath::Max(DriveConfigInfo.EngineRevDownRate, 0.f);

	SetupVehicleMovement(wheeledMovement);
}

void UDeliveryBot_ChaosDriveComponent::ApplyDriveInput(
	UChaosVehicleMovementComponent* vehicleMovement,
	float throttle,
	float steering,
	float brake,
	bool bHandbrake,
	float deltaTime)
{
	if (!IsValid(vehicleMovement))
	{
		return;
	}

	const float limitedThrottle{ GetLimitedThrottle(vehicleMovement, throttle) };
	float targetBrake{ FMath::Clamp(brake, 0.f, 1.f) };

	const float currentSpeedCmS{ FMath::Abs(vehicleMovement->GetForwardSpeed()) };
	const float speedLimitCmS{ GetMaxSpeedCmPerSecond() };
	const float speedToleranceCmS{ GetKmhToCmPerSecond(DriveConfigInfo.SpeedLimitToleranceKmh) };

	if (currentSpeedCmS > speedLimitCmS + speedToleranceCmS)
	{
		targetBrake = FMath::Max(targetBrake, DriveConfigInfo.SpeedLimitBrake);
	}

	const float targetSteering{ FMath::Clamp(steering, -1.f, 1.f) };

	if (targetBrake > KINDA_SMALL_NUMBER)
	{
		CurrentThrottleInput = 0.f;
	}
	else
	{
		CurrentThrottleInput = FMath::FInterpConstantTo(
			CurrentThrottleInput,
			limitedThrottle,
			deltaTime,
			DriveConfigInfo.ThrottleInputRatePerSecond
		);
	}

	CurrentBrakeInput = FMath::FInterpConstantTo(
		CurrentBrakeInput,
		targetBrake,
		deltaTime,
		DriveConfigInfo.BrakeInputRatePerSecond
	);

	CurrentSteeringInput = FMath::FInterpConstantTo(
		CurrentSteeringInput,
		targetSteering,
		deltaTime,
		DriveConfigInfo.SteeringInputRatePerSecond
	);

	vehicleMovement->SetThrottleInput(CurrentThrottleInput);
	vehicleMovement->SetSteeringInput(CurrentSteeringInput);
	vehicleMovement->SetBrakeInput(CurrentBrakeInput);
	vehicleMovement->SetHandbrakeInput(bHandbrake);
}

void UDeliveryBot_ChaosDriveComponent::SetDriveConfigInfo(const FDeliveryBotChaosDriveConfigInfo& driveConfigInfo)
{
	DriveConfigInfo = driveConfigInfo;
}

FDeliveryBotChaosDriveConfigInfo UDeliveryBot_ChaosDriveComponent::GetDriveConfigInfo() const
{
	return DriveConfigInfo;
}

float UDeliveryBot_ChaosDriveComponent::GetMaxSpeedCmPerSecond() const
{
	return GetKmhToCmPerSecond(DriveConfigInfo.MaxSpeedKmh);
}

void UDeliveryBot_ChaosDriveComponent::SetupTorqueCurve(UChaosWheeledVehicleMovementComponent* wheeledMovement) const
{
	if (!IsValid(wheeledMovement))
	{
		return;
	}

	FRichCurve* torqueCurve{ wheeledMovement->EngineSetup.TorqueCurve.GetRichCurve() };

	if (torqueCurve == nullptr)
	{
		return;
	}

	const float maxRPM{ FMath::Max(DriveConfigInfo.MaxRPM, 1.f) };

	torqueCurve->Reset();
	torqueCurve->AddKey(0.f, 0.7f);
	torqueCurve->AddKey(maxRPM * 0.125f, 1.f);
	torqueCurve->AddKey(maxRPM * 0.375f, 1.f);
	torqueCurve->AddKey(maxRPM * 0.625f, 0.75f);
	torqueCurve->AddKey(maxRPM * 0.875f, 0.35f);
	torqueCurve->AddKey(maxRPM, 0.1f);
}

float UDeliveryBot_ChaosDriveComponent::GetLimitedThrottle(
	const UChaosVehicleMovementComponent* vehicleMovement,
	float targetThrottle) const
{
	if (!IsValid(vehicleMovement))
	{
		return 0.f;
	}

	const float currentSpeedCmS{ FMath::Abs(vehicleMovement->GetForwardSpeed()) };
	const float maxSpeedCmS{ GetMaxSpeedCmPerSecond() };
	const float slowdownRangeCmS{ FMath::Max(GetKmhToCmPerSecond(DriveConfigInfo.SlowdownSpeedRangeKmh), 1.f) };

	const float throttleScale{
		FMath::Clamp((maxSpeedCmS - currentSpeedCmS) / slowdownRangeCmS, 0.f, 1.f)
	};

	return FMath::Clamp(targetThrottle, 0.f, 1.f) * throttleScale;
}

float UDeliveryBot_ChaosDriveComponent::GetKmhToCmPerSecond(float speedKmh) const
{
	return speedKmh * 27.777778f;
}
float UDeliveryBot_ChaosDriveComponent::GetCmPerSecondToKmh(float speedCmS) const
{
	return speedCmS / 27.777778f;
}
