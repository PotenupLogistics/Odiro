#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.h"
#include "EpisodeConfigTypes.generated.h"

// 에피소드 실행 설정과 재현성 seed 목록을 정의하는 파일임.
// 사용자가 선택한 템플릿과 파라미터를 한 번의 실행 설정으로 묶은 타입임.
// 여러 Iteration을 중복되지 않은 Seed로 관리하기 위함.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 TemplateVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 GeneratorVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 BaseSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 IterationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FEpisodeParamValue> Parameters;
};

// base seed에서 파생된 세부 seed들을 기록하는 재현성 장부임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeSeedLedger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 WorldSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 LayoutSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 StaticObstacleSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 DynamicActorSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 EventSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 WeatherSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 PolicySeed = 0;
};
