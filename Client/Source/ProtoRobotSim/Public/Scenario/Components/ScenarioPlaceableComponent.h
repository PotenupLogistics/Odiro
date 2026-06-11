#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioPlaceableComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

UENUM(BlueprintType)
enum class EScenarioPlaceableAuthoringRole : uint8
{
	Generic,
	RobotStartMarker,
	RobotGoalMarker
};

struct FScenarioAuthoringMeshCustomDepthState
{
	bool bRenderCustomDepth = false;
	int32 CustomDepthStencilValue = 0;
};

// 에피소드에 배치된 actor의 공통 식별 정보를 담는 component 파일임.
// spawned actor와 JSON 명세의 instance_id, asset_id를 연결하는 component임.
UCLASS(ClassGroup = (Scenario), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UScenarioPlaceableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScenarioPlaceableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioActorCategory Category = EScenarioActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringSelectable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	EScenarioPlaceableAuthoringRole AuthoringRole = EScenarioPlaceableAuthoringRole::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringRenamable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringDeletable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringAllowLocationEdit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringAllowRotationEdit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	bool bAuthoringAllowScaleEdit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor")
	TSoftObjectPtr<UMaterialInterface> AuthoringHoverOutlineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor", meta = (ClampMin = "0", ClampMax = "255"))
	int32 AuthoringHoverCustomDepthStencilValue = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|Editor")
	bool bAuthoringHovered = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|Editor")
	bool bAuthoringSelected = false;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void SetAuthoringHovered(bool bHovered);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void SetAuthoringSelected(bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	bool IsAuthoringHovered() const { return bAuthoringHovered; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	bool IsAuthoringSelected() const { return bAuthoringSelected; }

private:
	void ApplyAuthoringHoverVisual();
	void ClearAuthoringHoverVisual();
	void CollectOwnerMeshComponents(TArray<UMeshComponent*>& outMeshComponents) const;

	TMap<TWeakObjectPtr<UMeshComponent>, FScenarioAuthoringMeshCustomDepthState> CachedAuthoringCustomDepthStates;
};
