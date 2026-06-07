#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlaceableComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

struct FEpisodeAuthoringMeshCustomDepthState
{
	bool bRenderCustomDepth = false;
	int32 CustomDepthStencilValue = 0;
};

// 에피소드에 배치된 actor의 공통 식별 정보를 담는 component 파일임.
// spawned actor와 JSON 명세의 instance_id, asset_id를 연결하는 component임.
UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UEpisodePlaceableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEpisodePlaceableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeActorCategory Category = EEpisodeActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeMobilityMode MobilityMode = EEpisodeMobilityMode::Static;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor")
	bool bAuthoringSelectable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor")
	TSoftObjectPtr<UMaterialInterface> AuthoringHoverOutlineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor", meta = (ClampMin = "0", ClampMax = "255"))
	int32 AuthoringHoverCustomDepthStencilValue = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|Editor")
	bool bAuthoringHovered = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|Editor")
	bool bAuthoringSelected = false;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetAuthoringHovered(bool bHovered);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetAuthoringSelected(bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	bool IsAuthoringHovered() const { return bAuthoringHovered; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	bool IsAuthoringSelected() const { return bAuthoringSelected; }

private:
	void ApplyAuthoringHoverVisual();
	void ClearAuthoringHoverVisual();
	void CollectOwnerMeshComponents(TArray<UMeshComponent*>& outMeshComponents) const;

	TMap<TWeakObjectPtr<UMeshComponent>, FEpisodeAuthoringMeshCustomDepthState> CachedAuthoringCustomDepthStates;
};
