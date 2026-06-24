#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Subsystems/WorldSubsystem.h"
#include "Shared/Actors/ScenarioMapBounds.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioSimulationSubsystem.generated.h"

class ADeliveryBot;
class ADeliveryBot_GridBoundsActor;
class AScenarioCorridorRuntimeActor;
class AScenarioGroundRegion;
class AScenarioPedestrian;
class AScenarioSplinePath;
class AScenarioStaticObstacle;
class UPrimitiveComponent;
class UScenarioPlaceableComponent;
struct FScenarioPedestrianPlan;
struct FScenarioPedestrianPlanBuildContext;
struct FDeliveryBotSetupInfo;

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioSimulationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UScenarioSimulationSubsystem();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<AScenarioStaticObstacle> StaticObstacleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<AScenarioPedestrian> PedestrianClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<ADeliveryBot> RobotActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<AActor> GoalPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<AActor> StartPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Classes")
	TSubclassOf<ADeliveryBot_GridBoundsActor> GridBoundsActorClass;

	// 현재 Scenario가 소유한 runtime actor, Grid, Bounds와 보행자 계획을 정리한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void ClearScenario();

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	bool SetupScenarioWorld(const FScenarioSimulationSetupSpec& setupSpec);

	UFUNCTION(BlueprintPure, Category = "Scenario")
	AActor* FindRuntimeActor(const FString& instanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	FScenarioRuntimeContext BuildRuntimeContext(const FScenarioSimulationSetupSpec& setupSpec) const;

	// 전달받은 Surface Actor와 Placeable을 Grid와 동일한 Padding으로 계산한다.
	bool TryResolveScenarioMapBounds(
		const TArray<AActor*>& surfaceActors,
		const TArray<FScenarioPlaceableInstanceSpec>& placeables,
		FScenarioMapBounds& outBounds) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioSplinePath* SpawnSplinePath(const FString& pathId, const TArray<FVector>& points, bool bClosedLoop);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioSplinePath* FindSplinePath(const FString& pathId) const;

	// Spawns one sampled runtime Corridor surface actor.
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioCorridorRuntimeActor* SpawnCorridor(const FScenarioRuntimeCorridorSpec& corridorSpec);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioGroundRegion* SpawnGroundRegion(const FScenarioGroundRegionSpec& regionSpec);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioGroundRegion* FindGroundRegion(const FString& regionId) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioPedestrian* SpawnPedestrianOnPath(
		TSubclassOf<AScenarioPedestrian> inPedestrianClass,
		const FTransform& spawnTransform,
		AScenarioSplinePath* splinePath,
		double speedCmPerSecond,
		double initialDistanceCm,
		bool bStartFollowing);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioPedestrian* SpawnPedestrianOnPathId(
		TSubclassOf<AScenarioPedestrian> inPedestrianClass,
		const FTransform& spawnTransform,
		const FString& pathId,
		double speedCmPerSecond,
		double initialDistanceCm,
		bool bStartFollowing);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioGroundRegion>> RuntimeGroundRegions;

	// Runtime Corridor actors keyed by sampled corridor id.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioCorridorRuntimeActor>> RuntimeCorridors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioSplinePath>> RuntimePaths;

	// RuntimeActors가 SimulationSubsystem이 다루는 모든 Actor들이라면
	// RuntimeActorsById는 instance_id로 찾아야 하는 런타임 액터 lookup map.
	// 예를 들어, RuntimePaths, RuntimeGroundRegions는 instance_id가 없어서 포함되지 않음.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AActor>> RuntimeActorsById;

	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBot_GridBoundsActor> RuntimeGridBoundsActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float DeliveryBotGridBoundsPaddingCm{ 100.f };

	AActor* SpawnPlaceable(const FScenarioPlaceableInstanceSpec& placeableSpec);
	AScenarioStaticObstacle* SpawnStaticObstacle(const FScenarioPlaceableInstanceSpec& placeableSpec);
	bool TryFindStaticObstacleProp(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;
	AActor* SpawnRobotActor(const FScenarioPlaceableInstanceSpec& placeableSpec);
	AActor* SpawnDynamicActor(const FScenarioDynamicActorSpec& dynamicActorSpec);
	AScenarioPedestrian* SpawnPedestrian(const FScenarioDynamicActorSpec& dynamicActorSpec);
	AScenarioPedestrian* SpawnPlannedPedestrian(const FScenarioDynamicActorSpec& dynamicActorSpec);

	bool RebuildDeliveryBotGridFromScenarioSurfaces(const FScenarioSimulationSetupSpec& setupSpec);
	bool TryBuildGroundRegionXYBounds(
		const TArray<FScenarioGroundRegionSpec>& groundRegionSpecs,
		FBox2D& outXYBounds,
		double& outCenterZ) const;
	// Spawn된 runtime surface actor에서 Grid/Preview 공용 XY 영역과 평균 중심 Z를 계산한다.
	bool TryBuildRuntimeSurfaceXYBounds(
		FBox2D& outXYBounds,
		double& outCenterZ) const;
	// 최종 Map Bounds를 사용하는 runtime DeliveryBot GridBoundsActor를 생성한다.
	ADeliveryBot_GridBoundsActor* SpawnDeliveryBotGridBoundsActor(
		const FScenarioMapBounds& mapBounds);
	// 최종 Map Bounds의 중심과 크기를 GridBoundsActor에 적용한다.
	void ApplyMapBoundsToGridBoundsActor(
		ADeliveryBot_GridBoundsActor* gridBoundsActor,
		const FScenarioMapBounds& mapBounds) const;
	static void ExpandXYBoundsWithGroundRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		FBox2D& inOutXYBounds);
	bool ResolveDeliveryBotGridLocation(
		const FString& robotInstanceId,
		const FString& locationLabel,
		FVector& inOutWorldLocation) const;
	bool ValidateDeliveryBotRouteOnGrid(
		const FScenarioPlaceableInstanceSpec& placeableSpec,
		FDeliveryBotSetupInfo& setupInfo,
		bool bHasGoal,
		FVector& inOutGoalLocation) const;

	void RegisterRuntimeActor(
		const FString& instanceId,
		const FString& assetId,
		EScenarioActorCategory category,
		AActor* actor);

	void ConfigurePlaceableComponent(
		UScenarioPlaceableComponent* placeableComponent,
		const FString& instanceId,
		const FString& assetId,
		EScenarioActorCategory category) const;

	static double GetFloatProperty(
		const TMap<FString, FScenarioParamValue>& properties,
		const FString& key,
		double defaultValue);

	static bool GetBoolProperty(
		const TMap<FString, FScenarioParamValue>& properties,
		const FString& key,
		bool defaultValue);

	static FString GetStringProperty(
		const TMap<FString, FScenarioParamValue>& properties,
		const FString& key,
		const FString& defaultValue);

	static bool GetVectorProperty(
		const TMap<FString, FScenarioParamValue>& properties,
		const FString& key,
		FVector& outValue);

	AActor* FindRuntimeActorByCategory(EScenarioActorCategory category) const;

	void BuildPedestrianPlanContext(
		const FScenarioSimulationSetupSpec& setupSpec,
		FScenarioPedestrianPlanBuildContext& outBuildContext) const;

	static void SetActorReceivesDecals(AActor* actor, bool bReceivesDecals);
};
