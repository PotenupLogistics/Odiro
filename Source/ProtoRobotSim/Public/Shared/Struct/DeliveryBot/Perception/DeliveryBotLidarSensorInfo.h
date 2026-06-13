#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotLidarSensorInfo.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotLidarModeType : uint8
{
	OneD,
	TwoD,
	ThreeD,
	OneDAndTwoD,
	TwoDAndThreeD,
	All
};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarSensorConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawNearMissDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScanRangeM{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleStepDegree{ 2.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorHeightM{ 0.07f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FrontHalfAngleDegree{ 20.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStoreMissedRays{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopDistanceM{ 1.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NearMissDistanceM{ 2.f };
	
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
	
};

USTRUCT(BlueprintType)
struct FDeliveryBotLidarRayInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHit{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RayIndex{ INDEX_NONE };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RayYawDegree{ 0.f };

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
