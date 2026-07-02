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

private:
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
