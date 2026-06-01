#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeSpecTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeSimulationSubsystem.generated.h"

class ADeliveryBot_ChaosActor;
class AEpisodeGroundRegion;
class AEpisodePedestrian;
class AEpisodeSplinePath;
class AEpisodeStaticObstacle;
class UPrimitiveComponent;
class UEpisodePlaceableComponent;

// 컴파일된 Episode WorldSpec을 현재 월드에 스폰하고, 런타임 actor 생명주기를 관리하는 subsystem.
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
	TSubclassOf<ADeliveryBot_ChaosActor> RobotActorClass;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ClearEpisode();

	UFUNCTION(BlueprintCallable, Category = "Episode")
	bool SpawnEpisodeWorld(const FEpisodeWorldSpec& WorldSpec);

	UFUNCTION(BlueprintCallable, Category = "Episode|Json")
	bool SpawnEpisodeWorldFromJsonFile(const FString& JsonFilePath);

	UFUNCTION(BlueprintCallable, Category = "Episode|Debug")
	bool SpawnSampleEpisodeWorldFromJson();

	UFUNCTION(BlueprintPure, Category = "Episode")
	AActor* FindRuntimeActor(const FString& InstanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeSplinePath* SpawnSplinePath(const FString& PathId, const TArray<FVector>& Points, bool bClosedLoop);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeSplinePath* FindSplinePath(const FString& PathId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeGroundRegion* SpawnGroundRegion(const FEpisodeGroundRegionSpec& RegionSpec);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void SpawnGroundRegions(const TArray<FEpisodeGroundRegionSpec>& RegionSpecs);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodeGroundRegion* FindGroundRegion(const FString& RegionId) const;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodePedestrian* SpawnPedestrianOnPath(
		TSubclassOf<AEpisodePedestrian> InPedestrianClass,
		const FTransform& SpawnTransform,
		AEpisodeSplinePath* SplinePath,
		double SpeedCmPerSecond,
		double InitialDistanceCm,
		bool bStartFollowing);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodePedestrian* SpawnPedestrianOnPathId(
		TSubclassOf<AEpisodePedestrian> InPedestrianClass,
		const FTransform& SpawnTransform,
		const FString& PathId,
		double SpeedCmPerSecond,
		double InitialDistanceCm,
		bool bStartFollowing);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeGroundRegion>> RuntimeGroundRegions;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeSplinePath>> RuntimePaths;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AActor>> RuntimeActorsById;

	AActor* SpawnPlaceable(const FEpisodePlaceableInstanceSpec& PlaceableSpec);
	AEpisodeStaticObstacle* SpawnStaticObstacle(const FEpisodePlaceableInstanceSpec& PlaceableSpec);
	AActor* SpawnRobotActor(const FEpisodePlaceableInstanceSpec& PlaceableSpec, const FDeliveryBotRobotSpawnInfo& RobotSpawnInfo);
	AActor* SpawnDynamicActor(const FEpisodeDynamicActorSpec& DynamicActorSpec);
	AEpisodePedestrian* SpawnPedestrian(const FEpisodeDynamicActorSpec& DynamicActorSpec);

	void RegisterRuntimeActor(
		const FString& InstanceId,
		const FString& AssetId,
		EEpisodeActorCategory Category,
		EEpisodeMobilityMode MobilityMode,
		AActor* Actor);

	void ConfigurePlaceableComponent(
		UEpisodePlaceableComponent* PlaceableComponent,
		const FString& InstanceId,
		const FString& AssetId,
		EEpisodeActorCategory Category,
		EEpisodeMobilityMode MobilityMode) const;

	static double GetFloatProperty(
		const TMap<FString, FEpisodeParamValue>& Properties,
		const FString& Key,
		double DefaultValue);

	static bool GetBoolProperty(
		const TMap<FString, FEpisodeParamValue>& Properties,
		const FString& Key,
		bool DefaultValue);

	static bool GetVectorProperty(
		const TMap<FString, FEpisodeParamValue>& Properties,
		const FString& Key,
		FVector& OutValue);

	static void SetActorReceivesDecals(AActor* Actor, bool bReceivesDecals);
};
