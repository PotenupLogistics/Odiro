#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBotChaosDriveConfigInfo.h"
#include "DeliveryBot_ChaosDriveComponent.generated.h"

class UChaosVehicleMovementComponent;
class UChaosWheeledVehicleMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_ChaosDriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_ChaosDriveComponent();
	void ApplyMoveCommand(
		UChaosVehicleMovementComponent* vehicleMovement,
		const FDeliveryBotMoveCommandInfo& moveCommandInfo,
		float deltaTime);
	
	void SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void InitializeChaosDrive(
		UChaosWheeledVehicleMovementComponent* wheeledMovement,
		const FDeliveryBotChaosDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void SetDriveConfigInfo(const FDeliveryBotChaosDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotChaosDriveConfigInfo GetDriveConfigInfo() const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	float GetMaxSpeedCmPerSecond() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotChaosDriveConfigInfo DriveConfigInfo{};

	float CurrentThrottleInput{ 0.f };
	float CurrentBrakeInput{ 0.f };
	float CurrentSteeringInput{ 0.f };

private:
	void ApplyDriveInput(
		UChaosVehicleMovementComponent* vehicleMovement,
		float throttle,
		float steering,
		float brake,
		bool bHandbrake,
		float deltaTime);

	void SetupTorqueCurve(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;
	float GetCmPerSecondToKmh(float speedCmS) const;
	float GetLimitedThrottle(
		const UChaosVehicleMovementComponent* vehicleMovement,
		float targetThrottle) const;

	float GetKmhToCmPerSecond(float speedKmh) const;
};
