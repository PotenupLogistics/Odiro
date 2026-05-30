#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodeDefinition.generated.h"

// 사용자가 고르는 에피소드 템플릿 데이터 에셋을 정의하는 파일.
// 에피소드 템플릿의 ID, 버전, 기본 파라미터, 사용 가능한 asset pool을 담는 데이터 에셋.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	FName TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	int32 TemplateVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	TMap<FString, FEpisodeParamValue> DefaultParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	TArray<FString> AssetPoolIds;
};
