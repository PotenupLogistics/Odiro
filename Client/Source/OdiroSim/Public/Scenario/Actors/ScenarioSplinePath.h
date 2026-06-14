#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScenarioSplinePath.generated.h"

class USplineComponent;

// 보행자와 이동체가 참조할 수 있는 spline path actor 파일임.
// ScenarioPathSpec을 월드에서 시각화하거나 참조하기 위한 actor임.
UCLASS(BlueprintType)
class ODIROSIM_API AScenarioSplinePath : public AActor
{
	GENERATED_BODY()

public:
	AScenarioSplinePath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PathId;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void ConfigurePath(const FString& inPathId, const TArray<FVector>& points, bool bClosedLoop);
};
