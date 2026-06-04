#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.h"
#include "Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "ScenarioSpecTypes.generated.h"

// LLM이 언리얼에서 에피소드 세팅을 위해 사용할 수 있는 필드들을 한 WorldConfig에 해당.
// "어떤 상황 분포를 테스트할 것인가"
// JSON 친화적, semantic ID 중심.

// 지면 영역의 접근 가능 여부를 구분하는 타입.
UENUM(BlueprintType)
enum class EScenarioGroundRegionType : uint8
{
	Walkable,
	Penalty,
	Blocked
};

// JSON에서 정의할 수 있는 지면 영역 도형 타입.
UENUM(BlueprintType)
enum class EScenarioGroundShapeType : uint8
{
	Rectangle,
	ConvexPolygon
};

// 지면 영역의 위치와 면적을 표현하는 도형 정보.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioGroundShapeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioGroundShapeType ShapeType = EScenarioGroundShapeType::Rectangle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector CenterMeters = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector2D SizeMeters = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double YawDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FVector> PolygonPointsMeters;
};

// 이동 가능, 패널티, 차단 영역 하나를 표현하는 지면 정보.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioGroundRegionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioGroundRegionType RegionType = EScenarioGroundRegionType::Walkable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioGroundShapeSpec Shape;

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

// 시나리오 전체의 지면 영역 모델.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioGroundModelSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioGroundRegionType DefaultRegionType = EScenarioGroundRegionType::Walkable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioGroundRegionSpec> Regions;
};

// path 기준 종방향/횡방향 좌표로 배치 정보.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioRelativePlacementSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double DistanceMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double LateralOffsetMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double YawOffsetDegrees = 0.0;
};

// 로봇 또는 보행자가 참조할 baseline path 정보.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPathSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FVector> PointsMeters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool bClosedLoop = false;
};

// 정적 장애물 하나의 배치.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioStaticObstacleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool bUseRelativePlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioRelativePlacementSpec RelativePlacement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FEpisodeParamValue> Properties;
};

// 보행자가 원래 가려는 baseline 경로.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianBaselinePathSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString Mode = TEXT("generated_line");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString SpawnAnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString GoalAnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double SpawnDistanceMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double CrossingAngleDegrees = 90.0;
};

// 보행자가 움직임을 시작하는 조건.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianTriggerSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString TriggerType = TEXT("robot_distance_to_crossing_point");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double DistanceMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double DelaySeconds = 0.0;
};

// 보행자의 결정론적 이동 파라미터.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianMovementSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString MovementModel = TEXT("spline_Relative");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double BaseSpeedMetersPerSecond = 1.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double InitialDistanceMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double InitialLateralOffsetMeters = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double MaxSpeedMetersPerSecond = 1.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double AccelerationMetersPerSecondSquared = 2.0;
};

// 보행자 애니메이션과 회전 표시 방식.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianVisualSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString AnimationMode = TEXT("blendspace");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString OrientationMode = TEXT("face_movement_direction");
};

// 보행자가 RunTrace에 남길 이벤트 종류.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianTraceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FString> EmitEvents;
};

// 시나리오에 등장하는 보행자 한 명의 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString ArchetypeId = TEXT("adult_pedestrian");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString BehaviorId = TEXT("crossing_spline");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioPedestrianBaselinePathSpec BaselinePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioPedestrianTriggerSpec Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioPedestrianMovementSpec Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioPedestrianVisualSpec Visual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioPedestrianTraceSpec Trace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FEpisodeParamValue> Properties;
};

// LLM/사용자가 작성하는 최상위 시나리오 명세.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 ScenarioVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString Intent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString MapId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FDeliveryBotSetupInfo Robot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioGroundModelSpec GroundModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPathSpec> Paths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioStaticObstacleSpec> StaticObstacles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioPedestrianSpec> Pedestrians;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 NumSeeds = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FEpisodeParamValue> Metadata;
};
