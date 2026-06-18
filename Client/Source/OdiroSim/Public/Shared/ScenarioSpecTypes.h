#pragma once

#include "CoreMinimal.h"
#include "ScenarioCoreTypes.h"
#include "ScenarioConfigTypes.h"
#include "Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "ScenarioSpecTypes.generated.h"

UENUM(BlueprintType)
enum class EScenarioPathType : uint8
{
	Waypoints,
	Spline
};

UENUM(BlueprintType)
enum class EScenarioGroundRegionType : uint8
{
	Walkable,
	Penalty,
	Blocked
};

UENUM(BlueprintType)
enum class EScenarioGroundShapeType : uint8
{
	Rectangle,
	ConvexPolygon
};

// 런타임 지면 영역 명세. 단위는 centimeter.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioGroundRegionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioGroundRegionType RegionType = EScenarioGroundRegionType::Walkable;

	// Corridor surface catalog id used to keep editor preview and runtime visuals aligned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString SurfaceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioGroundShapeType ShapeType = EScenarioGroundShapeType::Rectangle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector2D Size = FVector2D(100.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double YawDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double TraversabilityScore = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PenaltyKind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double PenaltyCost = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double ViolationAfterSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString CollisionTag;
};

// 로봇을 배치시키고 초기화시키기 위한 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioDeliveryBotSpawnSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|DeliveryBot")
	FDeliveryBotSetupInfo SetupInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|DeliveryBot")
	bool bSpawnOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|DeliveryBot")
	bool bHasStartLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|DeliveryBot")
	bool bHasGoalLocation = false;
};

// 월드에 배치되는 정적 객체 하나를 표현하는 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioPlaceableInstanceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioActorCategory Category = EScenarioActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioDeliveryBotSpawnSpec DeliveryBot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FScenarioParamValue> Properties;
};

// 보행자나 이동체처럼 실행 중 움직이는 actor 하나를 표현하는 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioDynamicActorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioActorCategory Category = EScenarioActorCategory::Pedestrian;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FTransform InitialTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FScenarioParamValue> Properties;
};

// spline 또는 waypoint 기반 이동 경로를 표현하는 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioPathSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioPathType PathType = EScenarioPathType::Spline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FVector> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool bClosedLoop = false;
};

// 특정 시각이나 조건에서 발생할 이벤트를 표현하는 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioEventSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString TargetInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double TriggerTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FScenarioParamValue> Properties;
};

// Runner가 SimulationSubsystem에게 에피소드 Setup을 지시하기 위해 필요한 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSimulationSetupSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString SpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioSeedLedger Seeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioGroundRegionSpec> GroundRegions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPlaceableInstanceSpec> Placeables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioDynamicActorSpec> DynamicActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPathSpec> Paths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioEventSpec> Events;
};

// Runner가 읽어서 월드에 actor를 생성할 수 있는 최종 에피소드 명세.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioWorldSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioRunConfig RunConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioEvaluationConfig EvaluationConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioSeedLedger Seeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioGroundRegionSpec> GroundRegions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPlaceableInstanceSpec> Placeables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioDynamicActorSpec> DynamicActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPathSpec> Paths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioEventSpec> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString SpecHash;
};
