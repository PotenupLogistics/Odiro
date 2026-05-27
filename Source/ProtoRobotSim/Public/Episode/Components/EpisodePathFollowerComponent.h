#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EpisodePathFollowerComponent.generated.h"

// 보행자와 이동체가 EpisodePathSpec을 따라 움직이도록 하기 위한 component 파일임.
// MVP에서는 참조할 path id와 기본 이동 속도만 보관하고 실제 이동 로직은 이후 구현함.
UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UEpisodePathFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEpisodePathFollowerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double SpeedCmPerSecond = 120.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bLoop = false;
};
