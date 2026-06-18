#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Subsystems/WorldSubsystem.h"
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

// 컴파일된 Episode simulation setup spec을 현재 월드에 스폰하고, 런타임 actor 생명주기를 관리하는 subsystem.
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

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void ClearScenario();

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	bool SetupScenarioWorld(const FScenarioSimulationSetupSpec& setupSpec);

	UFUNCTION(BlueprintPure, Category = "Scenario")
	AActor* FindRuntimeActor(const FString& instanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	FScenarioRuntimeContext BuildRuntimeContext(const FScenarioSimulationSetupSpec& setupSpec) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioSplinePath* SpawnSplinePath(const FString& pathId, const TArray<FVector>& points, bool bClosedLoop);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioSplinePath* FindSplinePath(const FString& pathId) const;

	// Spawns one sampled runtime Corridor surface actor.
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioCorridorRuntimeActor* SpawnCorridor(const FScenarioRuntimeCorridorSpec& corridorSpec);

	// Spawns all sampled runtime Corridor surface actors for a setup.
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void SpawnCorridors(const TArray<FScenarioRuntimeCorridorSpec>& corridorSpecs);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	AScenarioGroundRegion* SpawnGroundRegion(const FScenarioGroundRegionSpec& regionSpec);

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void SpawnGroundRegions(const TArray<FScenarioGroundRegionSpec>& regionSpecs);

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
	// Builds grid bounds from spawned runtime surfaces that the user actually sees in the world.
	bool TryBuildRuntimeSurfaceXYBounds(
		FBox2D& outXYBounds,
		double& outCenterZ) const;
	ADeliveryBot_GridBoundsActor* SpawnDeliveryBotGridBoundsActor(const FBox2D& xyBounds, double centerZ);
	void ApplyXYBoundsToGridBoundsActor(
		ADeliveryBot_GridBoundsActor* gridBoundsActor,
		const FBox2D& xyBounds,
		double centerZ) const;
	static void ExpandXYBoundsWithGroundRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		FBox2D& inOutXYBounds);
	// Expands XY bounds from a spawned actor's component bounds.
	static void ExpandXYBoundsWithActor(
		const AActor* actor,
		FBox2D& inOutXYBounds,
		double& inOutZSum,
		int32& inOutValidActorCount);
	// Ensures DeliveryBot start and goal anchors are inside the generated navigation grid bounds.
	static void ExpandXYBoundsWithDeliveryBotRoute(
		const FScenarioPlaceableInstanceSpec& placeableSpec,
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
