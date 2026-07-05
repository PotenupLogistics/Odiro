#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "DeliveryBot_DriveComponent.generated.h"

class UChaosVehicleMovementComponent;
class UChaosWheeledVehicleMovementComponent;

USTRUCT(BlueprintType)
struct FDeliveryBotDriveRuntimeSnapshot
{
	GENERATED_BODY()

	// Currently applied throttle input in the 0..1 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float Throttle = 0.0f;

	// Currently applied brake input in the 0..1 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float Brake = 0.0f;

	// Currently applied steering input in the -1..1 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float Steering = 0.0f;

	// Smoothed target speed currently used by drive control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float TargetSpeedKmh = 0.0f;

	// Policy-requested target speed before drive-side interpolation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float RequestedTargetSpeedKmh = 0.0f;

	// Current signed forward speed sampled from Chaos movement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float CurrentForwardSpeedKmh = 0.0f;

	// Difference between smoothed target speed and current absolute speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float SpeedErrorKmh = 0.0f;

	// Brake value requested by policy before drive-side overrides.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float RequestedBrake = 0.0f;

	// Unsmooth throttle target resolved by drive control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float TargetThrottle = 0.0f;

	// Unsmooth brake target resolved by drive control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float TargetBrake = 0.0f;

	// Throttle target after hard speed-limit clipping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float LimitedThrottle = 0.0f;

	// Hard speed limit used by the last drive input application.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	float SpeedLimitKmh = 0.0f;

	// True when the hard speed limit, not target tracking, requested braking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	bool bSpeedLimitBrakeApplied = false;

	// True when the vehicle is over the target speed and coasting instead of braking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	bool bTargetSpeedOverspeed = false;

	// Handbrake state requested by the last drive input application.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	bool bHandbrake = false;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ODIROSIM_API UDeliveryBot_DriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Initializes the drive component with ticking disabled.
	UDeliveryBot_DriveComponent();
	// Applies a high-level movement command to the Chaos vehicle movement component.
	void ApplyMoveCommand(UChaosVehicleMovementComponent* vehicleMovement,	const FDeliveryBotMoveCommandInfo& moveCommandInfo,	float deltaTime);
	// Applies a configured parking stop without forcing handbrake unless requested.
	void ApplyParkingStop(UChaosVehicleMovementComponent* vehicleMovement);
	// Applies a forward-gear stop while waiting for a fresh policy command.
	void ApplyPolicyTimeoutSlowStop(UChaosVehicleMovementComponent* vehicleMovement, float deltaTime);
	
	// Applies stored drive configuration to a Chaos wheeled movement component.
	void SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;

	// Normalizes and applies runtime Chaos drive settings.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void InitializeChaosDrive(	UChaosWheeledVehicleMovementComponent* wheeledMovement,	const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	// Stores normalized drive configuration for future commands.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void SetDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	// Returns the normalized drive configuration currently in use.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotDriveConfigInfo GetDriveConfigInfo() const;

	// Returns the max forward speed in centimeters per second.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	float GetMaxSpeedCmPerSecond() const;

	// Returns the currently applied drive inputs for logging and replay recording.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotDriveRuntimeSnapshot GetRuntimeSnapshot() const;

protected:
	// Normalized drive configuration used by the component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotDriveConfigInfo DriveConfigInfo{};

	// Smoothed throttle input last applied to Chaos movement.
	float CurrentThrottleInput{ 0.f };
	// Smoothed brake input last applied to Chaos movement.
	float CurrentBrakeInput{ 0.f };
	// Smoothed steering input last applied to Chaos movement.
	float CurrentSteeringInput{ 0.f };
	// Smoothed target speed used by the speed controller.
	float CurrentTargetSpeedKmh{ 0.f };
	// Last policy-requested target speed used for drive diagnostics.
	float LastRequestedTargetSpeedKmh{ 0.f };
	// Last signed Chaos forward speed used for drive diagnostics.
	float LastCurrentForwardSpeedKmh{ 0.f };
	// Last target-speed error used for drive diagnostics.
	float LastSpeedErrorKmh{ 0.f };
	// Last policy-requested brake value used for drive diagnostics.
	float LastRequestedBrake{ 0.f };
	// Last unsmoothed throttle target used for drive diagnostics.
	float LastTargetThrottle{ 0.f };
	// Last unsmoothed brake target used for drive diagnostics.
	float LastTargetBrake{ 0.f };
	// Last hard-limit-clipped throttle target used for drive diagnostics.
	float LastLimitedThrottle{ 0.f };
	// Last hard speed limit used for drive diagnostics.
	float LastSpeedLimitKmh{ 0.f };
	// Whether the last drive update applied a hard speed-limit brake.
	bool bLastSpeedLimitBrakeApplied{ false };
	// Whether the last drive update was over target speed and coasting.
	bool bLastTargetSpeedOverspeed{ false };
	// Whether the last drive update requested the handbrake.
	bool bLastHandbrake{ false };

private:
	// Removes backward physics velocity introduced by braking or collision impulses.
	void ClampReverseLinearVelocity(UChaosVehicleMovementComponent* vehicleMovement) const;

	// Smooths and applies low-level drive inputs to Chaos movement.
	void ApplyDriveInput(
		UChaosVehicleMovementComponent* vehicleMovement,
		float throttle,
		float steering,
		float brake,
		bool bHandbrake,
		float speedLimitKmh,
		float deltaTime);
	
	// Rebuilds the engine torque curve from normalized drive settings.
	void SetupTorqueCurve(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;
	// Converts centimeters per second to kilometers per hour.
	float GetCmPerSecondToKmh(float speedCmS) const;
	// Limits throttle as the vehicle approaches the configured speed cap.
	float GetLimitedThrottle(const UChaosVehicleMovementComponent* vehicleMovement, float targetThrottle, float maxSpeedKmh) const;
	// Converts kilometers per hour to centimeters per second.
	float GetKmhToCmPerSecond(float speedKmh) const;
	
	// Resolves high-level move commands to the forward Chaos gear.
	int32 GetTargetGear(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;

	// Resolves the forward max target speed used by drive control.
	float GetTargetMaxSpeedKmh(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;

	// Resolves the forward acceleration or deceleration rate for the requested speed transition.
	float GetTargetAccelerationRateKmhPerSecond(const FDeliveryBotMoveCommandInfo& moveCommandInfo,	float requestedTargetSpeedKmh) const;

	// Returns true when a gear switch must first brake through near-zero speed.
	bool ShouldBrakeBeforeGearSwitch(const UChaosVehicleMovementComponent* vehicleMovement,	int32 targetGear) const;
	
	// Clamps external drive configuration into valid runtime ranges.
	FDeliveryBotDriveConfigInfo NormalizeDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo) const;
	
	
	
	
};
