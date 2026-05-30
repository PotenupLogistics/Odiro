#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeSpecTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeSimulationSubsystem.generated.h"

class AEpisodeGroundRegion;
class AEpisodePedestrian;
class AEpisodeSplinePath;

// 한 월드 안에서 에피소드 런타임 actor의 생성, 연결, 초기화를 총괄하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeSimulationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ClearEpisode();

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
		TSubclassOf<AEpisodePedestrian> PedestrianClass,
		const FTransform& SpawnTransform,
		AEpisodeSplinePath* SplinePath,
		double SpeedCmPerSecond,
		double InitialDistanceCm,
		bool bStartFollowing);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	AEpisodePedestrian* SpawnPedestrianOnPathId(
		TSubclassOf<AEpisodePedestrian> PedestrianClass,
		const FTransform& SpawnTransform,
		const FString& PathId,
		double SpeedCmPerSecond,
		double InitialDistanceCm,
		bool bStartFollowing);

	UFUNCTION(BlueprintCallable, Category = "Episode|Debug")
	AEpisodePedestrian* SpawnSimplePedestrianPathTest(
		TSubclassOf<AEpisodePedestrian> PedestrianClass,
		const FVector& StartLocation,
		const FVector& EndLocation,
		double SpeedCmPerSecond);

	UFUNCTION(BlueprintCallable, Category = "Episode|Debug")
	void SpawnDebugGroundRegionTest();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeGroundRegion>> RuntimeGroundRegions;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeSplinePath>> RuntimePaths;
};
