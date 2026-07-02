#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	constexpr float KMH_TO_CMS = 27.777778f;
	// Forward-only drive allows tiny numerical noise but removes real backward drift.
	constexpr float REVERSE_VELOCITY_CLAMP_TOLERANCE_CMS = 1.f;
}

// 드라이브 컴포넌트의 기본 Tick 설정을 초기화한다.
UDeliveryBot_DriveComponent::UDeliveryBot_DriveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 이동 명령을 현재 차량 상태에 맞는 기어, 스로틀, 브레이크, 조향 입력으로 변환한다.
void UDeliveryBot_DriveComponent::ApplyMoveCommand(UChaosVehicleMovementComponent* vehicleMovement,
	const FDeliveryBotMoveCommandInfo& moveCommandInfo, float deltaTime)
{
	if (!IsValid(vehicleMovement))
		return;

	FDeliveryBotMoveCommandInfo effectiveMoveCommandInfo = moveCommandInfo;
	// 정책이나 리플레이가 Reverse를 보내도 구동부에서는 전진 명령으로만 처리한다.
	effectiveMoveCommandInfo.MoveDirectionType = EDeliveryBotMoveDirectionType::Forward;

	const float safeDeltaTime = FMath::Max(deltaTime, 0.f);
	const int32 targetGear = GetTargetGear(effectiveMoveCommandInfo);
	const float targetMaxSpeedKmh = GetTargetMaxSpeedKmh(effectiveMoveCommandInfo);

	vehicleMovement->SetUseAutomaticGears(false);

	const bool bBrakeBeforeGearSwitch = ShouldBrakeBeforeGearSwitch(vehicleMovement, targetGear);
	if (bBrakeBeforeGearSwitch)
	{
		CurrentTargetSpeedKmh = 0.f;
		CurrentThrottleInput = 0.f;

		const float requestedBrake = FMath::Clamp(effectiveMoveCommandInfo.Brake, 0.f, 1.f);
		const bool bForwardCommandRollingBackward = targetGear > 0;
		const float targetBrake = bForwardCommandRollingBackward
			? 1.f
			: FMath::Max(
				DriveConfigInfo.GearSwitchBrakeInput,
				FMath::Max(DriveConfigInfo.StopBrakeInput, requestedBrake));
		const bool bTargetHandbrake =
			(DriveConfigInfo.bUseHandbrakeWhenBrake && effectiveMoveCommandInfo.bBrake)
			|| bForwardCommandRollingBackward;

		if (targetBrake >= 1.f - KINDA_SMALL_NUMBER)
		{
			CurrentBrakeInput = targetBrake;
		}

		ApplyDriveInput(
			vehicleMovement,
			0.f,
			0.f,
			targetBrake,
			bTargetHandbrake,
			targetMaxSpeedKmh,
			safeDeltaTime);

		return;
	}

	vehicleMovement->SetTargetGear(targetGear, true);

	const float requestedTargetSpeedKmh = effectiveMoveCommandInfo.bBrake
		? 0.f
		: FMath::Clamp(effectiveMoveCommandInfo.TargetSpeedKmh, 0.f, targetMaxSpeedKmh);

	const float speedInterpRateKmhPerSecond =
		GetTargetAccelerationRateKmhPerSecond(effectiveMoveCommandInfo, requestedTargetSpeedKmh);

	CurrentTargetSpeedKmh = FMath::FInterpConstantTo(
		CurrentTargetSpeedKmh,
		requestedTargetSpeedKmh,
		safeDeltaTime,
		speedInterpRateKmhPerSecond);

	const float currentSpeedKmh = FMath::Abs(GetCmPerSecondToKmh(vehicleMovement->GetForwardSpeed()));
	const float speedErrorKmh = CurrentTargetSpeedKmh - currentSpeedKmh;
	const float speedControlRangeKmh = FMath::Max(DriveConfigInfo.SlowdownSpeedRangeKmh, 0.1f);

	float throttle = FMath::Clamp(speedErrorKmh / speedControlRangeKmh, 0.f, 1.f);
	float brake = FMath::Clamp(effectiveMoveCommandInfo.Brake, 0.f, 1.f);

	if (effectiveMoveCommandInfo.bBrake)
	{
		brake = FMath::Max(brake, DriveConfigInfo.StopBrakeInput);
		if (brake >= 1.f - KINDA_SMALL_NUMBER)
		{
			CurrentThrottleInput = 0.f;
			CurrentBrakeInput = brake;
		}
	}
	else if (speedErrorKmh < -DriveConfigInfo.SpeedLimitToleranceKmh)
	{
		const float overspeedBrake = FMath::Clamp(-speedErrorKmh / speedControlRangeKmh, 0.f, 1.f);
		brake = FMath::Max(brake, FMath::Min(overspeedBrake, DriveConfigInfo.SpeedLimitBrake));
	}

	const bool bRollingBackward = vehicleMovement->GetForwardSpeed() < -REVERSE_VELOCITY_CLAMP_TOLERANCE_CMS;
	const bool bTargetHandbrake =
		(DriveConfigInfo.bUseHandbrakeWhenBrake && effectiveMoveCommandInfo.bBrake)
		|| bRollingBackward;

	ApplyDriveInput(
		vehicleMovement,
		throttle,
		effectiveMoveCommandInfo.Steering,
		brake,
		bTargetHandbrake,
		targetMaxSpeedKmh,
		safeDeltaTime);
}

// Applies a stop command that keeps the drivetrain out of reverse unless a later command requests it.
void UDeliveryBot_DriveComponent::ApplyParkingStop(UChaosVehicleMovementComponent* vehicleMovement)
{
	if (!IsValid(vehicleMovement))
		return;

	CurrentThrottleInput = 0.f;
	CurrentBrakeInput = 1.f;
	CurrentSteeringInput = 0.f;
	CurrentTargetSpeedKmh = 0.f;

	vehicleMovement->SetUseAutomaticGears(false);
	vehicleMovement->SetTargetGear(1, true);
	vehicleMovement->SetThrottleInput(0.f);
	vehicleMovement->SetSteeringInput(0.f);
	vehicleMovement->SetBrakeInput(1.f);
	vehicleMovement->SetHandbrakeInput(true);

	ClampReverseLinearVelocity(vehicleMovement);
}

// Applies a forward-gear stop for stale policy commands without engaging the handbrake.
void UDeliveryBot_DriveComponent::ApplyPolicyTimeoutSlowStop(
	UChaosVehicleMovementComponent* vehicleMovement,
	float deltaTime)
{
	if (!IsValid(vehicleMovement))
		return;

	const float safeDeltaTime = FMath::Max(deltaTime, 0.f);
	const int32 targetGear = 1;
	const bool bBrakeBeforeGearSwitch = ShouldBrakeBeforeGearSwitch(vehicleMovement, targetGear);

	CurrentTargetSpeedKmh = FMath::FInterpConstantTo(
		CurrentTargetSpeedKmh,
		0.f,
		safeDeltaTime,
		DriveConfigInfo.DecelerationRateKmhPerSecond);

	vehicleMovement->SetUseAutomaticGears(false);
	if (!bBrakeBeforeGearSwitch)
	{
		vehicleMovement->SetTargetGear(targetGear, true);
	}

	const float targetBrake = 1.f;

	if (targetBrake >= 1.f - KINDA_SMALL_NUMBER)
	{
		CurrentThrottleInput = 0.f;
		CurrentBrakeInput = targetBrake;
	}

	ApplyDriveInput(
		vehicleMovement,
		0.f,
		0.f,
		targetBrake,
		true,
		DriveConfigInfo.MaxSpeedKmh,
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

	if (DriveConfigInfo.bHasMassKg)
	{
		wheeledMovement->Mass = DriveConfigInfo.MassKg;
	}

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
	vehicleMovement->SetHandbrakeInput(
		bHandbrake
		|| targetBrake >= 1.f - KINDA_SMALL_NUMBER
		|| vehicleMovement->GetForwardSpeed() < -REVERSE_VELOCITY_CLAMP_TOLERANCE_CMS);

	ClampReverseLinearVelocity(vehicleMovement);
}

// 후진이 비활성화된 구동부에서 충돌이나 제동 반력으로 생긴 후방 선속도 성분을 제거한다.
void UDeliveryBot_DriveComponent::ClampReverseLinearVelocity(UChaosVehicleMovementComponent* vehicleMovement) const
{
	if (!IsValid(vehicleMovement) || !IsValid(vehicleMovement->UpdatedPrimitive))
		return;

	UPrimitiveComponent* primitive = vehicleMovement->UpdatedPrimitive.Get();
	if (!primitive->IsSimulatingPhysics())
		return;

	FVector forward = primitive->GetForwardVector();
	forward.Z = 0.f;
	if (!forward.Normalize())
		return;

	const FVector currentVelocity = primitive->GetPhysicsLinearVelocity();
	const float signedForwardVelocity = FVector::DotProduct(currentVelocity, forward);
	if (signedForwardVelocity >= -REVERSE_VELOCITY_CLAMP_TOLERANCE_CMS)
		return;

	const FVector clampedVelocity = currentVelocity - (forward * signedForwardVelocity);
	primitive->SetPhysicsLinearVelocity(clampedVelocity, false);
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

// 현재 적용 중인 주행 입력 snapshot을 반환한다.
FDeliveryBotDriveRuntimeSnapshot UDeliveryBot_DriveComponent::GetRuntimeSnapshot() const
{
	FDeliveryBotDriveRuntimeSnapshot Snapshot;
	Snapshot.Throttle = CurrentThrottleInput;
	Snapshot.Brake = CurrentBrakeInput;
	Snapshot.Steering = CurrentSteeringInput;
	Snapshot.TargetSpeedKmh = CurrentTargetSpeedKmh;
	return Snapshot;
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

// 후진 주행은 비활성화되어 모든 고수준 이동 명령은 전진 기어로 해석된다.
int32 UDeliveryBot_DriveComponent::GetTargetGear(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	static_cast<void>(moveCommandInfo);
	return 1;
}

// 후진 주행은 비활성화되어 모든 이동 명령에 전진 속도 제한을 사용한다.
float UDeliveryBot_DriveComponent::GetTargetMaxSpeedKmh(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	static_cast<void>(moveCommandInfo);
	return DriveConfigInfo.MaxSpeedKmh;
}

// 현재 목표 속도와 요청 속도 차이에 따라 가속 또는 감속 변화율을 선택한다.
float UDeliveryBot_DriveComponent::GetTargetAccelerationRateKmhPerSecond(const FDeliveryBotMoveCommandInfo& moveCommandInfo,
	float requestedTargetSpeedKmh) const
{
	if (requestedTargetSpeedKmh <= CurrentTargetSpeedKmh)
		return DriveConfigInfo.DecelerationRateKmhPerSecond;

	static_cast<void>(moveCommandInfo);
	return DriveConfigInfo.AccelerationRateKmhPerSecond;
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
	normalizedInfo.MassKg = FMath::Max(normalizedInfo.MassKg, 0.01f);
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
