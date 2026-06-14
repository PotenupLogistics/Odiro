#pragma once

#include "CoreMinimal.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Shared/ScenarioCompileTypes.h"
#include "ScenarioCompiler.generated.h"

class FJsonObject;
class FJsonValue;

// 작성자가 만든 JSON을 런타임 스폰용 FScenarioWorldSpec으로 변환하는 컴파일러.
// 파일 로딩, JSON 파싱, 단위 변환, 기본 검증 Diagnostic 생성을 한곳에서 담당한다.

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioCompiler : public UObject
{
	GENERATED_BODY()

public:
	UScenarioCompiler();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Compiler|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Compiler")
	FScenarioCompileResult CompileScenarioWorldSpecFromJsonFile(const FString& jsonFilePath) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Compiler")
	FScenarioCompileResult CompileScenarioWorldSpecFromJsonString(const FString& jsonString) const;

private:
	// 작성용 JSON은 meter 단위를 사용하고, 런타임 WorldSpec은 Unreal centimeter 단위를 사용한다.
	// 위치와 크기 값은 이 배율을 곱해 변환한다.
	static constexpr double MetersToCentimeters = 100.0;

	// 컴파일 중 발견한 오류나 경고를 결과 객체에 누적한다.
	// 호출자는 Severity로 성공 가능 여부를 판단하고, Code/Message로 UI나 로그에 표시한다.
	static void AddDiagnostic(FScenarioCompileResult& result, EScenarioCompileDiagnosticSeverity severity, const FString& code, const FString& message);

	// 누적된 Diagnostic 중 Error가 하나라도 있는지 확인한다.
	// 최종 bSuccess 값은 이 함수의 결과를 기준으로 결정한다.
	static bool HasErrors(const FScenarioCompileResult& result);

	// JSON bool 값을 FScenarioParamValue로 포장한다.
	// properties에 들어가는 임의 확장 필드를 WorldSpec의 공통 variant 타입으로 맞추기 위한 헬퍼다.
	static FScenarioParamValue MakeBoolParam(bool value);

	// JSON number 값을 FScenarioParamValue의 Float 타입으로 포장한다.
	// MVP에서는 JSON number를 정수/실수로 엄격히 나누지 않고 double 기반 값으로 보존한다.
	static FScenarioParamValue MakeFloatParam(double value);

	// JSON string 값을 FScenarioParamValue로 포장한다.
	// actor archetype, movement model 같은 semantic 문자열을 properties에 남길 때 사용한다.
	static FScenarioParamValue MakeStringParam(const FString& value);

	// 숫자 배열 3개로 해석된 값을 FScenarioParamValue Vector 타입으로 포장한다.
	// 이 함수 자체는 단위 변환을 하지 않으며, 호출 전에 meter/cm 여부를 확정해야 한다.
	static FScenarioParamValue MakeVectorParam(const FVector& value);

	// JSON 필드가 object인지 확인하고 FJsonObject로 꺼낸다.
	// 필드가 없거나 타입이 다르면 false를 반환해 호출자가 기본값이나 Diagnostic을 선택하게 한다.
	static bool TryGetObjectField(const FJsonObject& jsonObject, const FString& fieldName, TSharedPtr<FJsonObject>& outObject);

	// JSON 필드가 array인지 확인하고 FJsonValue 배열로 꺼낸다.
	// paths, regions, actors 같은 반복 항목을 읽는 공통 진입점이다.
	static bool TryGetArrayField(const FJsonObject& jsonObject, const FString& fieldName, TArray<TSharedPtr<FJsonValue>>& outArray);

	// JSON string 필드를 선택적으로 읽는다.
	// 필드가 없으면 false를 반환하고 Diagnostic은 남기지 않는다.
	static bool TryGetStringField(const FJsonObject& jsonObject, const FString& fieldName, FString& outValue);

	// primary 필드를 먼저 읽고, 없으면 대체 이름 필드를 읽는다.
	// prop_id/asset_id, region_type/type처럼 같은 의미의 대체 이름을 받아들이기 위한 호환 헬퍼다.
	static bool TryGetStringField(const FJsonObject& jsonObject, const FString& primaryFieldName, const FString& fallbackFieldName, FString& outValue);

	// 필수 string 필드를 읽고 비어 있으면 Error Diagnostic을 남긴다.
	// Path는 사람이 JSON 위치를 바로 찾을 수 있도록 Message에 포함된다.
	static bool RequireStringField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FScenarioCompileResult& result, FString& outValue);

	// 필수 string 필드를 primary/대체 이름으로 읽고 실패하면 Error Diagnostic을 남긴다.
	// 같은 의미의 대체 이름을 허용할 때 사용한다.
	static bool RequireStringField(const FJsonObject& jsonObject, const FString& primaryFieldName, const FString& fallbackFieldName, const FString& path, FScenarioCompileResult& result, FString& outValue);

	// JSON number 필드를 읽고 없으면 기본값을 반환한다.
	// 단위 해석은 호출자가 담당하며, 이 함수는 값 자체를 변환하지 않는다.
	static double ReadNumberOrDefault(const FJsonObject& jsonObject, const FString& fieldName, double defaultValue);

	// JSON bool 필드를 읽고 없으면 기본값을 반환한다.
	// auto_start, closed_loop, spawn_only처럼 생략 가능한 스위치 값에 사용한다.
	static bool ReadBoolOrDefault(const FJsonObject& jsonObject, const FString& fieldName, bool defaultValue);

	// 지정한 길이의 숫자 배열을 검증하며 읽는다.
	// 좌표, 크기, 회전 배열의 길이와 숫자 타입 오류는 여기서 Diagnostic으로 기록한다.
	static bool ReadNumberArray(const FJsonObject& jsonObject, const FString& fieldName, int32 expectedCount, const FString& path, FScenarioCompileResult& result, TArray<double>& outValues);

	// 숫자 2개 배열을 FVector2D로 읽고 Scale을 곱한다.
	// size_m 같은 평면 크기 입력은 meter에서 centimeter로 변환되어 ground region 크기로 들어간다.
	static bool ReadVector2DField(const FJsonObject& jsonObject, const FString& fieldName, double scale, const FString& path, FScenarioCompileResult& result, FVector2D& outVector);

	// xy_m, center_xy_m, goal_xy_m처럼 2D 입력을 읽어 Z=0인 FVector로 변환한다.
	static bool ReadVector2DAsVectorField(const FJsonObject& jsonObject, const FString& fieldName, double scale, const FString& path, FScenarioCompileResult& result, FVector& outVector);

	// ScenarioSetup 축약 양식의 xy_m/yaw_deg를 FTransform으로 변환한다.
	static bool ReadActorPlacementTransform(const FJsonObject& jsonObject, const FString& path, FScenarioCompileResult& result, FTransform& outTransform);

	// 문자열 ground region 타입을 런타임 enum으로 변환한다.
	// 현재 MVP는 walkable, penalty, blocked만 의미 있는 값으로 인정한다.
	static bool ParseGroundRegionType(const FString& value, EScenarioGroundRegionType& outType);

	// 문자열 shape 타입을 런타임 enum으로 변환한다.
	// convex_polygon은 타입으로는 읽지만 실제 컴파일 단계에서는 MVP 제약상 rectangle만 허용한다.
	static bool ParseGroundShapeType(const FString& value, EScenarioGroundShapeType& outType);

	// 한 컴파일 결과 안에서 semantic Id가 중복되는지 검사한다.
	// 중복되면 actor/Path/region lookup이 모호해지므로 Error Diagnostic을 남긴다.
	static bool AddUniqueId(TSet<FString>& ids, const FString& id, const FString& path, FScenarioCompileResult& result);

	// actor JSON의 properties object를 FScenarioParamValue map으로 복사한다.
	// 숫자 배열은 길이 3일 때만 Vector로 해석하며, 별도 단위 변환은 하지 않는다.
	static void AddJsonProperties(const FJsonObject& sourceObject, TMap<FString, FScenarioParamValue>& outProperties);

	// root JSON의 run/scenario 정보를 RunConfig와 SeedLedger로 옮긴다.
	// base_seed는 WorldSeed로 쓰고, 다른 seed들은 MVP용 고정 offset을 더해 파생시킨다.
	static void CompileRunConfig(const FJsonObject& rootObject, FScenarioCompileResult& result);

	static void CompileEvaluationConfig(const FJsonObject& rootObject, FScenarioCompileResult& result);

	// ground_model.regions 배열을 FScenarioGroundRegionSpec 목록으로 변환한다.
	// center_xy_m과 size_m은 centimeter로 변환하고, penalty/collision 의미 필드는 그대로 보존한다.
	static void CompileGroundRegions(const FJsonObject& rootObject, FScenarioCompileResult& result);

	// paths 배열을 FScenarioPathSpec 목록으로 변환하고 Path Id 목록을 수집한다.
	// points_xy_m 좌표는 meter에서 centimeter로 변환되며, 이후 pedestrian의 path_id 검증에 사용된다.
	static void CompilePaths(const FJsonObject& rootObject, FScenarioCompileResult& result, TSet<FString>& outPathIds);

	// actors.static_obstacles를 정적 배치 actor spec으로 변환한다.
	// prop_id는 AssetId에 그대로 대응시키고, default static obstacle catalog에 존재하는지 검증한다.
	void CompileStaticObstacles(const FJsonObject& actorsObject, FScenarioCompileResult& result, TSet<FString>& instanceIds) const;

	// actors.pedestrians를 동적 actor spec으로 변환한다.
	// speed_mps와 initial_distance_m은 원본 값과 centimeter 변환 값을 properties에 함께 남긴다.
	static void CompilePedestrians(const FJsonObject& actorsObject, FScenarioCompileResult& result, const TSet<FString>& pathIds, TSet<FString>& instanceIds);
	static void CompilePedestrianBehavior(const FJsonObject& pedestrianObject, const FString& path, FScenarioCompileResult& result, FScenarioDynamicActorSpec& dynamicActorSpec);

	// actors.robot setup은 FScenarioDeliveryBotSpawnSpec으로 컴파일되어 저장된다.
	// 배치는 xy_m/yaw_deg, 목적지는 route 블록에서 읽고 로봇 튜닝은 DeliveryBotSetup JSON이 담당한다.
	static void CompileRobotSpawn(const FJsonObject& actorsObject, FScenarioCompileResult& result, TSet<FString>& instanceIds);

	// actors object 전체를 static obstacle, pedestrian, robot 순서로 컴파일한다.
	// instance Id는 actor 종류와 관계없이 하나의 집합에서 중복을 검사한다.
	void CompileActors(const FJsonObject& rootObject, FScenarioCompileResult& result, const TSet<FString>& pathIds) const;

	// 파싱된 root object를 최종 FScenarioWorldSpec으로 조립한다.
	// source JSON 문자열의 hash를 SpecHash로 기록해 동일 입력 추적에 사용한다.
	void CompileRootObject(const FJsonObject& rootObject, const FString& sourceJson, FScenarioCompileResult& result) const;
};

/* 
컴파일러가 JSON으로부터 명시적으로 읽는 필드
scenario_id
version
run.base_seed
run.iteration_index
run.time_limit_s

evaluation.goal_acceptance_radius_m
evaluation.tip_over_angle_deg
evaluation.near_miss.distance_m
evaluation.scoring.static_obstacle_collision
evaluation.scoring.blocked_region_collision
evaluation.scoring.penalty_region_violation
evaluation.scoring.pedestrian_near_miss
evaluation.scoring.pedestrian_collision

ground_model.regions[].region_id
ground_model.regions[].region_type / type
ground_model.regions[].shape.type
ground_model.regions[].shape.center_xy_m
ground_model.regions[].shape.size_m
ground_model.regions[].shape.yaw_deg
ground_model.regions[].traversability_score
ground_model.regions[].penalty.kind
ground_model.regions[].penalty.cost
ground_model.regions[].penalty.violation_after_s
ground_model.regions[].collision_tag

paths[].path_id
paths[].points_xy_m
paths[].closed_loop

actors.static_obstacles[].instance_id
actors.static_obstacles[].prop_id / asset_id
actors.static_obstacles[].xy_m
actors.static_obstacles[].yaw_deg
actors.static_obstacles[].properties

actors.pedestrians[].instance_id
actors.pedestrians[].archetype_id
actors.pedestrians[].path_id
actors.pedestrians[].xy_m
actors.pedestrians[].yaw_deg
actors.pedestrians[].movement.model
actors.pedestrians[].movement.speed_mps
actors.pedestrians[].movement.initial_distance_m
actors.pedestrians[].movement.auto_start
actors.pedestrians[].properties

actors.robot.instance_id / actor_id
actors.robot.asset_id / type
actors.robot.spawn_only
actors.robot.xy_m
actors.robot.yaw_deg
actors.robot.route.goal_xy_m
actors.robot.route.auto_start
actors.robot.properties
 */
