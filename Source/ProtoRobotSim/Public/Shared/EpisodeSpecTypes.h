#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.h"
#include "EpisodeConfigTypes.h"
#include "Struct/DeliveryBotRobotSpawnInfo.h"
#include "EpisodeSpecTypes.generated.h"

// 언리얼에서 ScenarioSpec 해석 후 에피소드를 생성하기 위한 에피소드 명세.
// 컴파일된 에피소드 명세를 JSON 친화적인 데이터로 정의하는 파일.
// Episode : ScenarioSpec + seed를 컴파일한 single instance
// "이번 한 번의 실행에서 무엇을 어디에 배치하고 어떻게 실행할 것인가"를 결정하기 위한 실행 계약

UENUM(BlueprintType)
enum class EEpisodePathType : uint8
{
	Waypoints,
	Spline
};

UENUM(BlueprintType)
enum class EEpisodeGroundRegionType : uint8
{
	Walkable,
	Penalty,
	Blocked
};

UENUM(BlueprintType)
enum class EEpisodeGroundShapeType : uint8
{
	Rectangle,
	ConvexPolygon
};

// 런타임 지면 영역 명세. 단위는 
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeGroundRegionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeGroundRegionType RegionType = EEpisodeGroundRegionType::Walkable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeGroundShapeType ShapeType = EEpisodeGroundShapeType::Rectangle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector2D Size = FVector2D(100.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double YawDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double TraversabilityScore = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PenaltyKind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double PenaltyCost = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double ViolationAfterSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString CollisionTag;
};

// 월드에 배치되는 정적 객체 하나를 표현하는 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePlaceableInstanceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeActorCategory Category = EEpisodeActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeMobilityMode MobilityMode = EEpisodeMobilityMode::Static;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FEpisodeParamValue> Properties;
};

// 보행자나 이동체처럼 실행 중 움직이는 actor 하나를 표현하는 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeDynamicActorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeActorCategory Category = EEpisodeActorCategory::Pedestrian;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeMobilityMode MobilityMode = EEpisodeMobilityMode::Moving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FTransform InitialTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double SpawnTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FEpisodeParamValue> Properties;
};

// spline 또는 waypoint 기반 이동 경로를 표현하는 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePathSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodePathType PathType = EEpisodePathType::Spline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FVector> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bClosedLoop = false;
};

// 특정 시각이나 조건에서 발생할 이벤트를 표현하는 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeEventSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString TargetInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double TriggerTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FEpisodeParamValue> Properties;
};

// Runner가 읽어서 월드에 actor를 생성할 수 있는 최종 에피소드 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeWorldSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeRunConfig RunConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeEvaluationConfig EvaluationConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeSeedLedger Seeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodeGroundRegionSpec> GroundRegions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodePlaceableInstanceSpec> Placeables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodeDynamicActorSpec> DynamicActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodePathSpec> Paths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodeEventSpec> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString SpecHash;
	
	// 배달봇 전용 ----------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bHasDeliveryBotRobotSpawnInfo{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FDeliveryBotRobotSpawnInfo DeliveryBotRobotSpawnInfo{};
};
