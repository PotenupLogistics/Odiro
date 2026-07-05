#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "DeliveryBot_LidarSensorComponent.generated.h"

struct FDeliveryBotLidarRaySample;

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
	
	// Python 정책 입력에 사용할 2D LiDAR scan으로 변환한다.
	FDeliveryBotLidarScanInfo BuildPolicy2DScan(const FDeliveryBotLidarScanInfo& rawScanInfo) const;

	
	// 전방에 물체 있으면 멈춤
	bool ShouldStopByFrontObject(const FDeliveryBotLidarScanInfo& scanInfo,	FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const;
	
	TArray<FDeliveryBotLidarDetectedObjectInfo> BuildDetectedObjects(
			const FDeliveryBotLidarScanInfo& scanInfo) const;

	bool FindNearestFrontObject(
		const FDeliveryBotLidarScanInfo& scanInfo,
		FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const;

private:
	// Resolves the world-space LiDAR origin from the owner transform and configured local offset.
	FVector GetSensorWorldLocationCm(const AActor& owner) const;

	// 레이와 닿은 물체 판단
	bool TraceLidarRay(
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		FHitResult& outHitResult,
		TArray<FHitResult>& outRawHitResults) const;
	
	// Actor tag에 따라 debug ray line만 숨길지 판단한다.
	bool ShouldSuppressDebugRayLine(const AActor* actor) const;
	
	// 1D/2D trace 결과를 LiDAR ray 결과 구조체로 변환한다.
	FDeliveryBotLidarRayInfo MakeRayInfo(
		int32 rayIndex,
		float rayYawDegree,
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		const FHitResult* hitResult,
		EDeliveryBotLidarRayDimensionType rayDimensionType) const;

	// trace 결과를 LiDAR ray 결과 구조체로 변환한다.
	FDeliveryBotLidarRayInfo MakeRayInfo(
		int32 rayIndex,
		float rayYawDegree,
		float rayPitchDegree,
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		const FHitResult* hitResult,
		EDeliveryBotLidarRayDimensionType rayDimensionType) const;

	// yaw/pitch 각도 하나에 대한 LiDAR ray를 쏘고 scan 결과에 추가한다.
	void AppendLidarRay(
		FDeliveryBotLidarScanInfo& scanInfo,
		const FDeliveryBotLidarRaySample& raySample,
		const FVector& sensorLocationCm,
		const FRotator& ownerRotation,
		float scanRangeCm) const;
	
	// 두 LiDAR scan 결과를 하나로 합친다.
	FDeliveryBotLidarScanInfo MergeLidarScans(const FDeliveryBotLidarScanInfo& firstScanInfo,
		const FDeliveryBotLidarScanInfo& secondScanInfo) const;

	void DrawDebugLidarRay(
		const FVector& startLocationCm,
		const FVector& endLocationCm,
		const FHitResult* hitResult,
		const TArray<FHitResult>& rawHitResults,
		bool bDrawMissRayLine) const;

	// Keeps model-specific debug rendering readable without changing captured scan data.
	bool ShouldDrawDebugLidarRay(const FDeliveryBotLidarRaySample& raySample) const;

	// Python과 point cloud로 전달되는 LiDAR hit world location을 누적 점으로 표시한다.
	void DrawAccumulatedHitLocationDebug(const FVector& hitLocationCm, bool bObstacleHit) const;
	
	void DrawDebugObstacleWarningRange(const FVector& sensorLocationCm) const;
	
	float GetSignedYawDegree(float yawDegree) const;
	bool IsFrontYaw(float yawDegree) const;
	float GetDebugDrawLifeTimeSeconds() const;
	// 3D LiDAR scan 결과를 한 줄 로그로 요약한다.
	void LogLidarTraceSummary(const FDeliveryBotLidarScanInfo& scanInfo) const;

protected:
	// LiDAR trace summary 로그를 Unreal 개발자 진단용으로 출력할지 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar|Debug")
	bool bLogTraceSummary{ true };

	// LiDAR HitLocationCm 원본 좌표를 Unreal 월드에 장시간 누적 표시할지 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar|Debug")
	bool bDrawAccumulatedHitLocationDebug{ false };

	// 누적 hit location debug point가 월드에 남아 있는 시간.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar|Debug", meta = (ClampMin = "0.1"))
	float AccumulatedHitLocationDebugLifeTimeSeconds{ 60.f };

	// 누적 hit location debug point의 화면 표시 크기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar|Debug", meta = (ClampMin = "1.0"))
	float AccumulatedHitLocationDebugPointSize{ 5.f };

	// 런타임 LiDAR scan과 Python observation에 사용할 센서 설정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Lidar")
	FDeliveryBotLidarSensorConfigInfo LidarSensorConfigInfo{};


};
