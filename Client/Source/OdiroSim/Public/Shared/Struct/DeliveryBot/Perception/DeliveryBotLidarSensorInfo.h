#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotLidarSensorInfo.generated.h"

// 사용할 Ray 종류
UENUM(BlueprintType)
enum class EDeliveryBotLidarModeType : uint8
{
	OneD,
	TwoD,
	ThreeD,
	OneDAndTwoD,
	TwoDAndThreeD,
	All,
	OusterOS1
};

// LiDAR ray가 어떤 scan 방식에서 생성됐는지 구분한다.(즉, Ray 구분)
UENUM(BlueprintType)
enum class EDeliveryBotLidarRayDimensionType : uint8
{
	OneD,
	TwoD,
	ThreeD
};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarSensorConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawObstacleWarningDebug{ true };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScanRangeM{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleStepDegree{ 2.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorHeightM{ 0.07f };

	// Robot-local forward offset of the LiDAR origin in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorForwardOffsetM{ 0.f };

	// Robot-local right offset of the LiDAR origin in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorRightOffsetM{ 0.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FrontHalfAngleDegree{ 20.f };
	
	// 3D LiDAR의 아래쪽 수직 스캔 각도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalMinDegree{ -10.f };

	// 3D LiDAR의 위쪽 수직 스캔 각도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalMaxDegree{ 10.f };

	// 3D LiDAR의 수직 ray 간격.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalStepDegree{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStoreMissedRays{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopDistanceM{ 1.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleWarningDistanceM{ 2.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlowDownDistanceM{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollisionStopHalfAngleDegree{ 8.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollisionStopDistanceM{ 0.45f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ECollisionChannel> TraceChannel{ ECC_Visibility };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotLidarModeType LidarModeType{ EDeliveryBotLidarModeType::TwoD };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> IgnoreTags{ TEXT("NoCollision") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScanRateHz{ 10.f };


};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarRayInfo
{
	GENERATED_BODY()

public:
	// Python 전송 시 1D/2D/3D 배열 분리에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotLidarRayDimensionType RayDimensionType{ EDeliveryBotLidarRayDimensionType::TwoD }; 
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHit{ false };

	// True when this LiDAR hit should affect policy obstacle decisions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlocksPolicy{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RayIndex{ INDEX_NONE };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RayYawDegree{ 0.f };

	// 로봇 기준 ray의 수직 각도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RayPitchDegree{ 0.f };

	// Vertical scan channel for model-specific 3D LiDAR patterns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChannelIndex{ INDEX_NONE };

	// Horizontal scan column for rotating 3D LiDAR patterns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ColumnIndex{ INDEX_NONE };

	// Per-ray offset from the start of the simulated sensor frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RelativeTimeSeconds{ 0.f };

	// Sensor model that produced this ray when a concrete LiDAR preset is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SensorModel{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceM{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector StartLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector EndLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector HitLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActorName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ActorTags{};

	// Scenario semantic id resolved from the hit actor's placeable component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TargetId{};

	// Scenario semantic tags associated with the hit target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TargetTags{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> HitActor{ nullptr };
};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarScanInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SensorLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotLidarRayInfo> RayInfos{};

	// 이 LiDAR scan이 만들어진 고정 시뮬레이션 시간.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SimulationTimeSeconds{ 0.f };
};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarDetectedObjectInfo
{
	GENERATED_BODY()

public: // 탐지된 액터
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> DetectedActor{ nullptr };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActorName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ActorTags{};

	// Scenario semantic id resolved from the detected actor's placeable component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TargetId{};

	// Scenario semantic tags associated with the detected target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TargetTags{};

	// True when this detected object should affect policy obstacle decisions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlocksPolicy{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBounds{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BoundsOriginCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BoundsExtentCm{ FVector::ZeroVector };

public: // 가장 가까운 액터와의 위치 정보
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ClosestHitLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestDistanceM{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestRayYawDegree{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ClosestFrontHitLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestFrontDistanceM{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestFrontRayYawDegree{ 0.f };

public: // 레이 몇 개 맞았는지(레이의 총 개수와 같은면 어디도 못간다 판단 할 수 있을지도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FrontHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInFront{ false };
};

// 라이더에 부딪힌 각 액터에 대한 정보
USTRUCT(BlueprintType)
struct FDeliveryBotLidarObservedObjectInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActorName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ActorTags{};

	// Scenario semantic id exposed to policy and action-log payloads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TargetId{};

	// Scenario semantic tags exposed to policy and action-log payloads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TargetTags{};

	// True when this observed object should affect policy obstacle decisions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlocksPolicy{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBounds{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BoundsOriginCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BoundsExtentCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ClosestHitLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestDistanceM{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClosestRayYawDegree{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FrontHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInFront{ false };
};
