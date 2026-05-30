#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EpisodeSplinePath.generated.h"

class USplineComponent;

// 보행자와 이동체가 참조할 수 있는 spline path actor 파일임.
// EpisodePathSpec을 월드에서 시각화하거나 참조하기 위한 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeSplinePath : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeSplinePath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PathId;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ConfigurePath(const FString& InPathId, const TArray<FVector>& Points, bool bClosedLoop);
};
