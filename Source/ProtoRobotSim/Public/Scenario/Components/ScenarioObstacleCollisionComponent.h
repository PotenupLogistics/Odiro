#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScenarioObstacleCollisionComponent.generated.h"

// 에피소드 장애물의 물리 충돌과 안전 query 설정을 대표하는 component 파일임.
// 실제 collision primitive들은 actor/blueprint가 갖고, 이 component는 공통 설정을 담는 용도임.
UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UScenarioObstacleCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScenarioObstacleCollisionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bUsePhysicalCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bUseSafetyQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double SafetyRadius = 100.0;
};
