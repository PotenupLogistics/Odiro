#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Curves/RichCurve.h"

namespace
{
	constexpr float KMH_TO_CMS = 27.777778f;
}

// 드라이브 컴포넌트의 기본 Tick 설정을 초기화한다.
UDeliveryBot_DriveComponent::UDeliveryBot_DriveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 이동 명령을 현재 차량 상태에 맞는 기어, 스로틀, 브레이크, 조향 입력으로 변환한다.
void UDeliveryBot_DriveComponent::ApplyMoveCommand(UChaosVehicleMovementComponent* vehicleMovement,	
	const FDeliveryBotMoveCommandInfo& moveCommandInfo,	float deltaTime)
{
	if (!IsValid(vehicleMovement))
		return;

	const float safeDeltaTime = FMath::Max(deltaTime, 0.f);
	const int32 targetGear = GetTargetGear(moveCommandInfo);
	const float targetMaxSpeedKmh = GetTargetMaxSpeedKmh(moveCommandInfo);

	if (ShouldBrakeBeforeGearSwitch(vehicleMovement, targetGear))
	{
		CurrentTargetSpeedKmh = 0.f;

		ApplyDriveInput(
			vehicleMovement,
			0.f,
			0.f,
			DriveConfigInfo.GearSwitchBrakeInput,
			false,
			targetMaxSpeedKmh,
			safeDeltaTime);

		return;
	}

	vehicleMovement->SetTargetGear(targetGear, true);

	const float requestedTargetSpeedKmh = moveCommandInfo.bBrake
		? 0.f
		: FMath::Clamp(moveCommandInfo.TargetSpeedKmh, 0.f, targetMaxSpeedKmh);

	const float speedInterpRateKmhPerSecond =
		GetTargetAccelerationRateKmhPerSecond(moveCommandInfo, requestedTargetSpeedKmh);

	CurrentTargetSpeedKmh = FMath::FInterpConstantTo(
		CurrentTargetSpeedKmh,
		requestedTargetSpeedKmh,
		safeDeltaTime,
		speedInterpRateKmhPerSecond);

	const float currentSpeedKmh = FMath::Abs(GetCmPerSecondToKmh(vehicleMovement->GetForwardSpeed()));
	const float speedErrorKmh = CurrentTargetSpeedKmh - currentSpeedKmh;
	const float speedControlRangeKmh = FMath::Max(DriveConfigInfo.SlowdownSpeedRangeKmh, 0.1f);

	const float throttle = FMath::Clamp(speedErrorKmh / speedControlRangeKmh, 0.f, 1.f);

	float brake = FMath::Clamp(moveCommandInfo.Brake, 0.f, 1.f);

	if (moveCommandInfo.bBrake)
	{
		brake = FMath::Max(brake, DriveConfigInfo.StopBrakeInput);
	}
	else if (speedErrorKmh < -DriveConfigInfo.SpeedLimitToleranceKmh)
	{
		const float overspeedBrake = FMath::Clamp(-speedErrorKmh / speedControlRangeKmh, 0.f, 1.f);
		brake = FMath::Max(brake, FMath::Min(overspeedBrake, DriveConfigInfo.SpeedLimitBrake));
	}

	ApplyDriveInput(
		vehicleMovement,
		throttle,
		moveCommandInfo.Steering,
		brake,
		DriveConfigInfo.bUseHandbrakeWhenBrake && moveCommandInfo.bBrake,
		targetMaxSpeedKmh,
		safeDeltaTime);
}

// Chaos Wheeled Vehicle의 엔진과 변속기 기본 설정을 적용한다.
void UDeliveryBot_DriveComponent::SetupVehicleMovement(
	UChaosWheeledVehicleMovementComponent* wheeledMovement) const
{
	if (!IsValid(wheeledMovement))
		return;

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

// 외부 설정값을 보정한 뒤 Chaos 주행 설정을 초기화한다.
void UDeliveryBot_DriveComponent::InitializeChaosDrive(UChaosWheeledVehicleMovementComponent* wheeledMovement,	const FDeliveryBotDriveConfigInfo& driveConfigInfo)
{
	DriveConfigInfo = NormalizeDriveConfigInfo(driveConfigInfo);

	const float maxTargetSpeedKmh = FMath::Max(
		DriveConfigInfo.MaxSpeedKmh,
		DriveConfigInfo.MaxReverseSpeedKmh);

	CurrentTargetSpeedKmh = FMath::Clamp(CurrentTargetSpeedKmh, 0.f, maxTargetSpeedKmh);

	SetupVehicleMovement(wheeledMovement);
}

// 보간된 스로틀, 브레이크, 조향 입력을 Chaos VehicleMovement에 적용한다.
void UDeliveryBot_DriveComponent::ApplyDriveInput(
	UChaosVehicleMovementComponent* vehicleMovement,
	float throttle,
	float steering,
	float brake,
	bool bHandbrake,
	float speedLimitKmh,
	float deltaTime)
{
	if (!IsValid(vehicleMovement))
		return;

	const float limitedThrottle = GetLimitedThrottle(vehicleMovement, throttle, speedLimitKmh);
	float targetBrake = FMath::Clamp(brake, 0.f, 1.f);

	const float currentSpeedCmS = FMath::Abs(vehicleMovement->GetForwardSpeed());
	const float speedLimitCmS = GetKmhToCmPerSecond(speedLimitKmh);
	const float speedToleranceCmS = GetKmhToCmPerSecond(DriveConfigInfo.SpeedLimitToleranceKmh);

	if (currentSpeedCmS > speedLimitCmS + speedToleranceCmS)
	{
		targetBrake = FMath::Max(targetBrake, DriveConfigInfo.SpeedLimitBrake);
	}

	const float targetSteering = FMath::Clamp(steering, -1.f, 1.f);
	const float targetThrottle = targetBrake > KINDA_SMALL_NUMBER ? 0.f : limitedThrottle;

	CurrentThrottleInput = FMath::FInterpConstantTo(
		CurrentThrottleInput,
		targetThrottle,
		deltaTime,
		DriveConfigInfo.ThrottleInputRatePerSecond);

	CurrentBrakeInput = FMath::FInterpConstantTo(
		CurrentBrakeInput,
		targetBrake,
		deltaTime,
		DriveConfigInfo.BrakeInputRatePerSecond);

	CurrentSteeringInput = FMath::FInterpConstantTo(
		CurrentSteeringInput,
		targetSteering,
		deltaTime,
		DriveConfigInfo.SteeringInputRatePerSecond);

	vehicleMovement->SetThrottleInput(CurrentThrottleInput);
	vehicleMovement->SetSteeringInput(CurrentSteeringInput);
	vehicleMovement->SetBrakeInput(CurrentBrakeInput);
	vehicleMovement->SetHandbrakeInput(bHandbrake);
}

// 주행 설정값을 보정해 저장하고 현재 목표 속도를 유효 범위로 맞춘다.
void UDeliveryBot_DriveComponent::SetDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo)
{
	DriveConfigInfo = NormalizeDriveConfigInfo(driveConfigInfo);

	const float maxTargetSpeedKmh = FMath::Max(
		DriveConfigInfo.MaxSpeedKmh,
		DriveConfigInfo.MaxReverseSpeedKmh);

	CurrentTargetSpeedKmh = FMath::Clamp(CurrentTargetSpeedKmh, 0.f, maxTargetSpeedKmh);
}

// 현재 적용 중인 주행 설정값을 반환한다.
FDeliveryBotDriveConfigInfo UDeliveryBot_DriveComponent::GetDriveConfigInfo() const
{
	return DriveConfigInfo;
}

// 최대 전진 속도를 cm/s 단위로 반환한다.
float UDeliveryBot_DriveComponent::GetMaxSpeedCmPerSecond() const
{
	return GetKmhToCmPerSecond(DriveConfigInfo.MaxSpeedKmh);
}

// 엔진 RPM별 토크 곡선을 설정한다.
void UDeliveryBot_DriveComponent::SetupTorqueCurve(	UChaosWheeledVehicleMovementComponent* wheeledMovement) const
{
	if (!IsValid(wheeledMovement))
		return;

	FRichCurve* torqueCurve = wheeledMovement->EngineSetup.TorqueCurve.GetRichCurve();

	if (torqueCurve == nullptr)
		return;

	const float maxRPM = FMath::Max(DriveConfigInfo.MaxRPM, 1.f);

	torqueCurve->Reset();
	torqueCurve->AddKey(0.f, 0.7f);
	torqueCurve->AddKey(maxRPM * 0.125f, 1.f);
	torqueCurve->AddKey(maxRPM * 0.375f, 1.f);
	torqueCurve->AddKey(maxRPM * 0.625f, 0.75f);
	torqueCurve->AddKey(maxRPM * 0.875f, 0.35f);
	torqueCurve->AddKey(maxRPM, 0.1f);
}

// 이동 방향에 따라 전진 기어 또는 후진 기어를 결정한다.
int32 UDeliveryBot_DriveComponent::GetTargetGear(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	return moveCommandInfo.MoveDirectionType == EDeliveryBotMoveDirectionType::Reverse ? -1 : 1;
}

// 이동 방향에 따라 사용할 최대 속도(km/h)를 반환한다.
float UDeliveryBot_DriveComponent::GetTargetMaxSpeedKmh(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	return moveCommandInfo.MoveDirectionType == EDeliveryBotMoveDirectionType::Reverse
		? DriveConfigInfo.MaxReverseSpeedKmh
		: DriveConfigInfo.MaxSpeedKmh;
}

// 현재 목표 속도와 요청 속도 차이에 따라 가속 또는 감속 변화율을 선택한다.
float UDeliveryBot_DriveComponent::GetTargetAccelerationRateKmhPerSecond(const FDeliveryBotMoveCommandInfo& moveCommandInfo,
	float requestedTargetSpeedKmh) const
{
	if (requestedTargetSpeedKmh <= CurrentTargetSpeedKmh)
		return DriveConfigInfo.DecelerationRateKmhPerSecond;

	return moveCommandInfo.MoveDirectionType == EDeliveryBotMoveDirectionType::Reverse
		? DriveConfigInfo.ReverseAccelerationRateKmhPerSecond
		: DriveConfigInfo.AccelerationRateKmhPerSecond;
}

// 현재 진행 방향과 목표 기어가 반대일 때 기어 전환 전 제동이 필요한지 판단한다.
bool UDeliveryBot_DriveComponent::ShouldBrakeBeforeGearSwitch(const UChaosVehicleMovementComponent* vehicleMovement,int32 targetGear) const
{
	if (!IsValid(vehicleMovement))
		return false;

	const float signedSpeedKmh = GetCmPerSecondToKmh(vehicleMovement->GetForwardSpeed());

	if (targetGear > 0)
		return signedSpeedKmh < -DriveConfigInfo.GearSwitchStopSpeedKmh;

	return signedSpeedKmh > DriveConfigInfo.GearSwitchStopSpeedKmh;
}

// 주행 설정값이 음수나 비정상 범위로 들어오지 않도록 보정한다.
FDeliveryBotDriveConfigInfo UDeliveryBot_DriveComponent::NormalizeDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo) const
{
	FDeliveryBotDriveConfigInfo normalizedInfo = driveConfigInfo;

	normalizedInfo.MaxSpeedKmh = FMath::Max(normalizedInfo.MaxSpeedKmh, 0.f);
	normalizedInfo.MaxReverseSpeedKmh = FMath::Max(normalizedInfo.MaxReverseSpeedKmh, 0.f);
	normalizedInfo.ReverseAccelerationRateKmhPerSecond = FMath::Max(normalizedInfo.ReverseAccelerationRateKmhPerSecond, 0.f);
	normalizedInfo.GearSwitchStopSpeedKmh = FMath::Max(normalizedInfo.GearSwitchStopSpeedKmh, 0.f);
	normalizedInfo.GearSwitchBrakeInput = FMath::Clamp(normalizedInfo.GearSwitchBrakeInput, 0.f, 1.f);

	normalizedInfo.SlowdownSpeedRangeKmh = FMath::Max(normalizedInfo.SlowdownSpeedRangeKmh, 0.1f);
	normalizedInfo.SpeedLimitToleranceKmh = FMath::Max(normalizedInfo.SpeedLimitToleranceKmh, 0.f);
	normalizedInfo.SpeedLimitBrake = FMath::Clamp(normalizedInfo.SpeedLimitBrake, 0.f, 1.f);
	normalizedInfo.StopBrakeInput = FMath::Clamp(normalizedInfo.StopBrakeInput, 0.f, 1.f);

	normalizedInfo.ThrottleInputRatePerSecond = FMath::Max(normalizedInfo.ThrottleInputRatePerSecond, 0.f);
	normalizedInfo.BrakeInputRatePerSecond = FMath::Max(normalizedInfo.BrakeInputRatePerSecond, 0.f);
	normalizedInfo.SteeringInputRatePerSecond = FMath::Max(normalizedInfo.SteeringInputRatePerSecond, 0.f);

	normalizedInfo.AccelerationRateKmhPerSecond = FMath::Max(normalizedInfo.AccelerationRateKmhPerSecond, 0.f);
	normalizedInfo.DecelerationRateKmhPerSecond = FMath::Max(normalizedInfo.DecelerationRateKmhPerSecond, 0.f);

	normalizedInfo.MaxTorque = FMath::Max(normalizedInfo.MaxTorque, 0.f);
	normalizedInfo.MaxRPM = FMath::Max(normalizedInfo.MaxRPM, 1.f);
	normalizedInfo.EngineIdleRPM = FMath::Max(normalizedInfo.EngineIdleRPM, 0.f);
	normalizedInfo.EngineBrakeEffect = FMath::Max(normalizedInfo.EngineBrakeEffect, 0.f);
	normalizedInfo.EngineRevUpMOI = FMath::Max(normalizedInfo.EngineRevUpMOI, 0.f);
	normalizedInfo.EngineRevDownRate = FMath::Max(normalizedInfo.EngineRevDownRate, 0.f);

	return normalizedInfo;
}

// 현재 속도가 제한 속도에 가까워질수록 스로틀을 줄인다.
float UDeliveryBot_DriveComponent::GetLimitedThrottle(const UChaosVehicleMovementComponent* vehicleMovement,float targetThrottle,float maxSpeedKmh) const
{
	if (!IsValid(vehicleMovement))
		return 0.f;

	const float currentSpeedCmS = FMath::Abs(vehicleMovement->GetForwardSpeed());
	const float maxSpeedCmS = GetKmhToCmPerSecond(maxSpeedKmh);
	const float slowdownRangeCmS = FMath::Max(GetKmhToCmPerSecond(DriveConfigInfo.SlowdownSpeedRangeKmh), 1.f);

	const float throttleScale = FMath::Clamp((maxSpeedCmS - currentSpeedCmS) / slowdownRangeCmS, 0.f, 1.f);

	return FMath::Clamp(targetThrottle, 0.f, 1.f) * throttleScale;
}

// km/h 단위 속도를 cm/s 단위로 변환한다.
float UDeliveryBot_DriveComponent::GetKmhToCmPerSecond(float speedKmh) const
{
	return speedKmh * KMH_TO_CMS;
}

// cm/s 단위 속도를 km/h 단위로 변환한다.
float UDeliveryBot_DriveComponent::GetCmPerSecondToKmh(float speedCmS) const
{
	return speedCmS / KMH_TO_CMS;
}
