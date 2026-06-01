#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotLidarSensorInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotLidarSensorConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScanRangeM{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleStepDegree{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorHeightM{ 0.4f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FrontHalfAngleDegree{ 10.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStoreMissedRays{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopDistanceM{ 1.5f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ECollisionChannel> TraceChannel{ ECC_Visibility };

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

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> DetectedActor{ nullptr };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActorName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ActorTags{};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FrontHitRayCount{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInFront{ false };
};
