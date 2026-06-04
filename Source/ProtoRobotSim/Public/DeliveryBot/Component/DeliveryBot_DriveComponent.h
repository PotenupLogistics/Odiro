#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "DeliveryBot_DriveComponent.generated.h"

class UChaosVehicleMovementComponent;
class UChaosWheeledVehicleMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_DriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_DriveComponent();
	void ApplyMoveCommand(UChaosVehicleMovementComponent* vehicleMovement,	const FDeliveryBotMoveCommandInfo& moveCommandInfo,	float deltaTime);
	
	void SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void InitializeChaosDrive(	UChaosWheeledVehicleMovementComponent* wheeledMovement,	const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void SetDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotDriveConfigInfo GetDriveConfigInfo() const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	float GetMaxSpeedCmPerSecond() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotDriveConfigInfo DriveConfigInfo{};

	float CurrentThrottleInput{ 0.f };
	float CurrentBrakeInput{ 0.f };
	float CurrentSteeringInput{ 0.f };
	float CurrentTargetSpeedKmh{ 0.f };

private:
	void ApplyDriveInput(
		UChaosVehicleMovementComponent* vehicleMovement,
		float throttle,
		float steering,
		float brake,
		bool bHandbrake,
		float speedLimitKmh,
		float deltaTime);
	
	void SetupTorqueCurve(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;
	float GetCmPerSecondToKmh(float speedCmS) const;
	float GetLimitedThrottle(const UChaosVehicleMovementComponent* vehicleMovement, float targetThrottle, float maxSpeedKmh) const;
	float GetKmhToCmPerSecond(float speedKmh) const;
	
	int32 GetTargetGear(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;

	float GetTargetMaxSpeedKmh(const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;

	float GetTargetAccelerationRateKmhPerSecond(const FDeliveryBotMoveCommandInfo& moveCommandInfo,	float requestedTargetSpeedKmh) const;

	bool ShouldBrakeBeforeGearSwitch(const UChaosVehicleMovementComponent* vehicleMovement,	int32 targetGear) const;
	
	FDeliveryBotDriveConfigInfo NormalizeDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo) const;
	
	
	
	
};
