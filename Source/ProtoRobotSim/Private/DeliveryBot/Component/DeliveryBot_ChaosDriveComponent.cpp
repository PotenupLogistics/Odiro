#include "DeliveryBot/Component/DeliveryBot_ChaosDriveComponent.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Curves/RichCurve.h"

UDeliveryBot_ChaosDriveComponent::UDeliveryBot_ChaosDriveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeliveryBot_ChaosDriveComponent::SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const
{
	if (!IsValid(wheeledMovement))
	{
		return;
	}

	wheeledMovement->bMechanicalSimEnabled = true;

	wheeledMovement->EngineSetup.MaxTorque = DriveConfigInfo.MaxTorque;
	wheeledMovement->EngineSetup.MaxRPM = DriveConfigInfo.MaxRPM;
	wheeledMovement->EngineSetup.EngineIdleRPM = DriveConfigInfo.EngineIdleRPM;
	wheeledMovement->EngineSetup.EngineBrakeEffect = DriveConfigInfo.EngineBrakeEffect;
	wheeledMovement->EngineSetup.EngineRevUpMOI = DriveConfigInfo.EngineRevUpMOI;
	wheeledMovement->EngineSetup.EngineRevDownRate = DriveConfigInfo.EngineRevDownRate;

	SetupTorqueCurve(wheeledMovement);
}

void UDeliveryBot_ChaosDriveComponent::ApplyDriveInput(
	UChaosVehicleMovementComponent* vehicleMovement,
	float throttle,
	float steering,
	float brake,
	bool bHandbrake) const
{
	if (!IsValid(vehicleMovement))
	{
		return;
	}

	const float limitedThrottle{ GetLimitedThrottle(vehicleMovement, throttle) };
	float finalBrake{ FMath::Clamp(brake, 0.f, 1.f) };

	const float currentSpeedCmS{ FMath::Abs(vehicleMovement->GetForwardSpeed()) };
	const float speedLimitCmS{ GetMaxSpeedCmPerSecond() };
	const float speedToleranceCmS{ GetKmhToCmPerSecond(DriveConfigInfo.SpeedLimitToleranceKmh) };

	if (currentSpeedCmS > speedLimitCmS + speedToleranceCmS)
	{
		finalBrake = FMath::Max(finalBrake, DriveConfigInfo.SpeedLimitBrake);
	}

	vehicleMovement->SetThrottleInput(limitedThrottle);
	vehicleMovement->SetSteeringInput(FMath::Clamp(steering, -1.f, 1.f));
	vehicleMovement->SetBrakeInput(finalBrake);
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