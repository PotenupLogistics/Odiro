#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "DeliveryBot_LidarSensorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ODIROSIM_API UDeliveryBot_LidarSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_LidarSensorComponent();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Lidar")
	void InitializeLidar(const FDeliveryBotLidarSensorConfigInfo& lidarSensorConfigInfo);

public:
	FDeliveryBotLidarScanInfo ScanLidar() const;
	FDeliveryBotLidarScanInfo ScanLidar1D() const;
	FDeliveryBotLidarScanInfo ScanLidar2D() const; // 레이쏘는 역할
	FDeliveryBotLidarScanInfo ScanLidar3D() const;
	
	
	// 전방에 물체 있으면 멈춤
	bool ShouldStopByFrontObject(const FDeliveryBotLidarScanInfo& scanInfo,	FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const;
	
	TArray<FDeliveryBotLidarDetectedObjectInfo> BuildDetectedObjects(
	const FDeliveryBotLidarScanInfo& scanInfo) const;

	bool FindNearestFrontObject(
		const FDeliveryBotLidarScanInfo& scanInfo,
		FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar")
	FDeliveryBotLidarSensorConfigInfo LidarSensorConfigInfo{};

private:
	// 레이와 닿은 물체 판단
	bool TraceLidarRay(const FVector& startLocationCm,	const FVector& endLocationCm, FHitResult& outHitResult) const;


	bool ShouldIgnoreActor(const AActor* actor) const;

	FDeliveryBotLidarRayInfo MakeRayInfo(
		int32 rayIndex,
		float rayYawDegree,
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		const FHitResult* hitResult) const;

	void DrawDebugLidarRay(
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		const FHitResult* hitResult) const;

	void DrawDebugObstacleWarningRange(const FVector& sensorLocationCm) const;
	
	
	
	float GetSignedYawDegree(float yawDegree) const;
	bool IsFrontYaw(float yawDegree) const;
};
