#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Shared/Struct/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBotSetupInfo.h"
#include "Shared/Struct/DeliveryBotMovementInfo.h"
#include "DeliveryBot_ChaosActor.generated.h"


class UDeliveryBot_GridSubsystem;
class UDeliveryBot_LidarSensorComponent;
class UDeliveryBot_ChaosDriveComponent;
class UDeliveryBot_GlobalPathComponent;
class UDeliveryBot_PathFollowComponent;
UCLASS(Blueprintable)
class PROTOROBOTSIM_API ADeliveryBot_ChaosActor : public AWheeledVehiclePawn
{
	GENERATED_BODY()
public:
	ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	void InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo);

private:
	void ApplySetupInfo();
	void BuildGlobalPathAndStartFollow();
	void ApplyPathFollowMoveCommand(float deltaTime);
	void ApplyStopCommand(FDeliveryBotMoveCommandInfo& moveCommandInfo) const;
	bool TryRequestRepathByFrontObject(const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo);
	bool IsInRepathMoveGraceTime() const;
	void AlignRotationToPathStart();
	void UpdateLidarScan();
	void DebugFrontLidarObject() const;
	int32 SetLidarDetectedActorsAsDynamicBlocked(UDeliveryBot_GridSubsystem* gridSubsystem,	AActor* requiredFrontActor) const;
	void ClearLidarDynamicBlockedCells();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_ChaosDriveComponent> ChaosDriveComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_GlobalPathComponent> GlobalPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_PathFollowComponent> PathFollowComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeliveryBot_LidarSensorComponent> LidarSensorComponent;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupInfo SetupInfo{};

	FDeliveryBotLidarScanInfo LastLidarScanInfo{};

	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	bool bUseFrontObstacleRepath{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	float RepathCooldownSeconds{ 2.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	float RepathMoveGraceSeconds{ 2.5f };

	float LastRepathRequestTimeSeconds{ -1000.f };
	float LastSuccessfulRepathTimeSeconds{ -1000.f };
	bool bHasLidarDynamicBlockedCells{ false };
	
};
