#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodeRunner.generated.h"

// FEpisodeWorldSpec을 실행하는 에피소드 생명주기 actor 파일임.
// 일단은 WorldSpec 보관만 담당하고 spawn/start/stop 로직은 이후 구현할 예정.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeRunner : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeRunner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeWorldSpec LoadedWorldSpec;
};
