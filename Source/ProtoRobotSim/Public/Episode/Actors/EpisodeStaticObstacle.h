#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodeStaticObstacle.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePlaceableComponent;
class UStaticMesh;
class UStaticMeshComponent;

// 정적 장애물(쓰레기봉투, 가로수, 입간판, 방치 PM 등) actor 파일임.
// 움직이지 않는 에피소드 장애물의 기본 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeStaticObstacle : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeStaticObstacle();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	bool SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> InStaticMeshAsset);

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	void SetStaticMesh(UStaticMesh* InStaticMesh);

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	bool ApplyConfiguredStaticMesh();

	UFUNCTION(BlueprintCallable, Category = "Episode|Catalog")
	bool ApplyPropEntry(const FEpisodeStaticObstaclePropEntry& PropEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Catalog")
	bool ApplyDefaultPropById(FName InPropId);

	static bool FindDefaultPropEntryById(FName InPropId, FEpisodeStaticObstaclePropEntry& OutPropEntry);
	static const TArray<FEpisodeStaticObstaclePropEntry>& GetDefaultPropEntries();
	static TArray<FName> GetDefaultPropIds();

	UFUNCTION(BlueprintPure, Category = "Episode|Placement")
	bool GetPlacementBounds(
		FVector& OutOrigin,
		FVector& OutBoxExtent,
		FVector2D& OutHalfSize2D,
		double& OutRadius2D) const;

	UFUNCTION(BlueprintPure, Category = "Episode|Placement")
	double GetPlacementRadius2D() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UStaticMeshComponent> MeshRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FName PropId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FName SemanticTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FText PropDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	EEpisodeStaticObstaclePropCategory PropCategory = EEpisodeStaticObstaclePropCategory::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Mesh", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Placement", meta = (ClampMin = "0.0"))
	FVector FallbackBoxExtent = FVector(50.0, 50.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Placement")
	bool bUseFallbackBoundsWhenMeshMissing = true;
};
