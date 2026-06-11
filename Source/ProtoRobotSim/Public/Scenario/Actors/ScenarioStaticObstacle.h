#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioStaticObstacle.generated.h"

class UScenarioObstacleCollisionComponent;
class UScenarioPlaceableComponent;
class UScenarioStaticObstaclePropCatalog;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

// 정적 장애물(쓰레기봉투, 가로수, 입간판, 방치 PM 등) actor 파일임.
// 움직이지 않는 에피소드 장애물의 기본 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioStaticObstacle : public AActor
{
	GENERATED_BODY()

public:
	AScenarioStaticObstacle();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UStaticMeshComponent> MeshRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UBoxComponent> CollisionBoundsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UScenarioObstacleCollisionComponent> ObstacleCollisionComponent;
	
	virtual void OnConstruction(const FTransform& transform) override;

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	bool SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> inStaticMeshAsset);

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	void SetStaticMesh(UStaticMesh* inStaticMesh);

	UFUNCTION(BlueprintCallable, Category = "Episode|Mesh")
	bool ApplyConfiguredStaticMesh();

	UFUNCTION(BlueprintCallable, Category = "Episode|Catalog")
	bool ApplyPropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Catalog")
	bool ApplyDefaultPropById(FName inPropId);

	UFUNCTION(BlueprintPure, Category = "Episode|Placement")
	bool GetPlacementBounds(
		FVector& outOrigin,
		FVector& outBoxExtent,
		FVector2D& outHalfSize2D,
		double& outRadius2D) const;

	UFUNCTION(BlueprintPure, Category = "Episode|Placement")
	double GetPlacementRadius2D() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FName PropId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FName SemanticTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	FText PropDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Semantic")
	EScenarioStaticObstaclePropCategory PropCategory = EScenarioStaticObstaclePropCategory::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Mesh", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Placement", meta = (ClampMin = "0.0"))
	FVector FallbackBoxExtent = FVector(50.0, 50.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Placement")
	bool bUseFallbackBoundsWhenMeshMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Collision")
	bool bUseMeshSimpleCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Collision")
	bool bUseFallbackBoxCollision = true;

private:
	bool TryFindConfiguredPropEntry(FName inPropId, FScenarioStaticObstaclePropEntry& outPropEntry) const;
	void ApplyCollisionSettings();
	void ApplyObjectTypeActorTag();
};
