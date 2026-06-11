#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Episode/Data/EpisodeStaticObstaclePropCatalog.h"
#include "Subsystems/WorldSubsystem.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodeSimulationSubsystem.generated.h"

class ADeliveryBot;
class ADeliveryBot_GridBoundsActor;
class AEpisodeGroundRegion;
class AEpisodePedestrian;
class AEpisodeSplinePath;
class AEpisodeStaticObstacle;
class UPrimitiveComponent;
class UEpisodePlaceableComponent;
struct FEpisodePedestrianPlan;
struct FEpisodePedestrianPlanBuildContext;
struct FDeliveryBotSetupInfo;

// 컴파일된 Episode simulation setup spec을 현재 월드에 스폰하고, 런타임 actor 생명주기를 관리하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeSimulationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UEpisodeSimulationSubsystem();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<AEpisodeStaticObstacle> StaticObstacleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<AEpisodePedestrian> PedestrianClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<ADeliveryBot> RobotActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<AActor> GoalPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<AActor> StartPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Catalog")
	TSoftObjectPtr<UEpisodeStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
	TSubclassOf<ADeliveryBot_GridBoundsActor> GridBoundsActorClass;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ClearEpisode();

	UFUNCTION(BlueprintCallable, Category = "Episode")
	bool SetupEpisodeWorld(const FEpisodeSimulationSetupSpec& setupSpec);

	UFUNCTION(BlueprintPure, Category = "Episode")
	AActor* FindRuntimeActor(const FString& instanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	FEpisodeRuntimeContext BuildRuntimeContext(const FEpisodeSimulationSetupSpec& setupSpec) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeSplinePath* SpawnSplinePath(const FString& pathId, const TArray<FVector>& points, bool bClosedLoop);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeSplinePath* FindSplinePath(const FString& pathId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeGroundRegion* SpawnGroundRegion(const FEpisodeGroundRegionSpec& regionSpec);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void SpawnGroundRegions(const TArray<FEpisodeGroundRegionSpec>& regionSpecs);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeGroundRegion* FindGroundRegion(const FString& regionId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodePedestrian* SpawnPedestrianOnPath(
		TSubclassOf<AEpisodePedestrian> inPedestrianClass,
		const FTransform& spawnTransform,
		AEpisodeSplinePath* splinePath,
		double speedCmPerSecond,
		double initialDistanceCm,
		bool bStartFollowing);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodePedestrian* SpawnPedestrianOnPathId(
		TSubclassOf<AEpisodePedestrian> inPedestrianClass,
		const FTransform& spawnTransform,
		const FString& pathId,
		double speedCmPerSecond,
		double initialDistanceCm,
		bool bStartFollowing);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeGroundRegion>> RuntimeGroundRegions;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeSplinePath>> RuntimePaths;

	// RuntimeActors가 SimulationSubsystem이 다루는 모든 Actor들이라면
	// RuntimeActorsById는 instance_id로 찾아야 하는 런타임 액터 lookup map.
	// 예를 들어, RuntimePaths, RuntimeGroundRegions는 instance_id가 없어서 포함되지 않음.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AActor>> RuntimeActorsById;

	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBot_GridBoundsActor> RuntimeGridBoundsActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float DeliveryBotGridBoundsPaddingCm{ 100.f };

	AActor* SpawnPlaceable(const FEpisodePlaceableInstanceSpec& placeableSpec);
	AEpisodeStaticObstacle* SpawnStaticObstacle(const FEpisodePlaceableInstanceSpec& placeableSpec);
	bool TryFindStaticObstacleProp(FName propId, FEpisodeStaticObstaclePropEntry& outPropEntry) const;
	AActor* SpawnRobotActor(const FEpisodePlaceableInstanceSpec& placeableSpec);
	AActor* SpawnDynamicActor(const FEpisodeDynamicActorSpec& dynamicActorSpec);
	AEpisodePedestrian* SpawnPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec);
	AEpisodePedestrian* SpawnPlannedPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec);

	bool RebuildDeliveryBotGridFromEpisodeGroundRegions(const FEpisodeSimulationSetupSpec& setupSpec);
	bool TryBuildGroundRegionXYBounds(
		const TArray<FEpisodeGroundRegionSpec>& groundRegionSpecs,
		FBox2D& outXYBounds,
		double& outCenterZ) const;
	ADeliveryBot_GridBoundsActor* SpawnDeliveryBotGridBoundsActor(const FBox2D& xyBounds, double centerZ);
	void ApplyXYBoundsToGridBoundsActor(
		ADeliveryBot_GridBoundsActor* gridBoundsActor,
		const FBox2D& xyBounds,
		double centerZ) const;
	static void ExpandXYBoundsWithGroundRegion(
		const FEpisodeGroundRegionSpec& regionSpec,
		FBox2D& inOutXYBounds);
	bool ValidateDeliveryBotGridLocation(
		const FString& robotInstanceId,
		const FString& locationLabel,
		const FVector& worldLocation) const;
	bool ValidateDeliveryBotRouteOnGrid(
		const FEpisodePlaceableInstanceSpec& placeableSpec,
		const FDeliveryBotSetupInfo& setupInfo,
		bool bHasGoal,
		const FVector& goalLocation) const;

	void RegisterRuntimeActor(
		const FString& instanceId,
		const FString& assetId,
		EEpisodeActorCategory category,
		AActor* actor);

	void ConfigurePlaceableComponent(
		UEpisodePlaceableComponent* placeableComponent,
		const FString& instanceId,
		const FString& assetId,
		EEpisodeActorCategory category) const;

	static double GetFloatProperty(
		const TMap<FString, FEpisodeParamValue>& properties,
		const FString& key,
		double defaultValue);

	static bool GetBoolProperty(
		const TMap<FString, FEpisodeParamValue>& properties,
		const FString& key,
		bool defaultValue);

	static FString GetStringProperty(
		const TMap<FString, FEpisodeParamValue>& properties,
		const FString& key,
		const FString& defaultValue);

	static bool GetVectorProperty(
		const TMap<FString, FEpisodeParamValue>& properties,
		const FString& key,
		FVector& outValue);

	AActor* FindRuntimeActorByCategory(EEpisodeActorCategory category) const;

	void BuildPedestrianPlanContext(
		const FEpisodeSimulationSetupSpec& setupSpec,
		FEpisodePedestrianPlanBuildContext& outBuildContext) const;

	static void SetActorReceivesDecals(AActor* actor, bool bReceivesDecals);
};
