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
	void ApplyMoveCommand(UChaosVehicleMovementComponent* vehicleMovement, const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;
	
	void SetupVehicleMovement(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;

	void ApplyDriveInput(
		UChaosVehicleMovementComponent* vehicleMovement,
		float throttle,
		float steering,
		float brake,
		bool bHandbrake) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Chaos Drive")
	void SetDriveConfigInfo(const FDeliveryBotChaosDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotChaosDriveConfigInfo GetDriveConfigInfo() const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Chaos Drive")
	float GetMaxSpeedCmPerSecond() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Chaos Drive")
	FDeliveryBotChaosDriveConfigInfo DriveConfigInfo{};

private:
	void SetupTorqueCurve(UChaosWheeledVehicleMovementComponent* wheeledMovement) const;
	float GetCmPerSecondToKmh(float speedCmS) const;
	float GetLimitedThrottle(
		const UChaosVehicleMovementComponent* vehicleMovement,
		float targetThrottle) const;

	float GetKmhToCmPerSecond(float speedKmh) const;
};