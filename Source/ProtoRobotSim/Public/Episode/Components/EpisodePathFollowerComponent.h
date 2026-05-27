#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EpisodePathFollowerComponent.generated.h"

class USplineComponent;

// 보행자와 이동체가 EpisodePathSpec을 따라 움직이도록 하기 위한 component 파일임.
UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UEpisodePathFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEpisodePathFollowerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PathId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double SpeedCmPerSecond = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bOrientToSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bUseCharacterMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bFreezeOwnedSplineOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode", meta = (ClampMin = "0.0"))
	double CharacterMovementLookAheadCm = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode", meta = (ClampMin = "0.0"))
	double StopDistanceToleranceCm = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double InitialDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode")
	double CurrentDistanceCm = 0.0;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void SetSplineComponent(USplineComponent* InSplineComponent);

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void StartFollowing();

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void StopFollowing();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ResolveSplineComponent();
	void ConfigureCharacterMovementTickDependency();
	void FreezeOwnedSplineTransform();
	bool TryMoveOwnerWithCharacterMovementInput(double SplineLength);
	void MoveOwnerToCurrentDistance();
};
