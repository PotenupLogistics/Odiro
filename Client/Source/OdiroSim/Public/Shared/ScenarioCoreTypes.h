#pragma once

#include "CoreMinimal.h"
#include "ScenarioCoreTypes.generated.h"

class UStaticMesh;
class UTexture2D;

// 에피소드 전반에서 공유하는 가장 작은 공통 타입들을 모아둔 파일임.

UENUM(BlueprintType)
enum class EScenarioParamValueType : uint8
{
	None,
	Bool,
	Integer,
	Float,
	String,
	Vector
};

UENUM(BlueprintType)
enum class EScenarioActorCategory : uint8
{
	DeliveryBot,
	StaticObstacle,
	Pedestrian,
	GroundRegion
};

UENUM(BlueprintType)
enum class EScenarioStaticObstaclePropCategory : uint8
{
	Unknown,
	StreetFurniture,
	TrafficControl,
	DeliveryItem,
	Utility,
	SurfaceObject
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioStaticObstaclePropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FName PropId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FName SemanticTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	EScenarioStaticObstaclePropCategory Category = EScenarioStaticObstaclePropCategory::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Palette")
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Placement", meta = (ClampMin = "0.0"))
	FVector FallbackBoxExtent = FVector(50.0, 50.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUsePhysicalCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseSafetyQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseMeshSimpleCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseFallbackBoxCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision", meta = (ClampMin = "0.0"))
	double SafetyRadius = 100.0;
};

// JSON으로 옮길 수 있는 파라미터 값을 담기 위한 작은 variant 타입임.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioParamValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioParamValueType Type = EScenarioParamValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 IntegerValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double FloatValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector VectorValue = FVector::ZeroVector;
};
