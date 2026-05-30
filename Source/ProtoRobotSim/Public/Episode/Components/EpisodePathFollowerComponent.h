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
	double SpeedCmPerSecond = 140.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bOrientToSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise")
	bool bUseSeededPathNoise = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise")
	int32 PathNoiseSeed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise", meta = (ClampMin = "0.0"))
	double LateralNoiseAmplitudeCm = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise", meta = (ClampMin = "1.0"))
	double LateralNoiseWavelengthCm = 400.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	double SpeedNoiseStrength = 0.15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise", meta = (ClampMin = "1.0"))
	double SpeedNoiseWavelengthCm = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Path Noise", meta = (ClampMin = "0.0"))
	double PathNoiseEndpointFadeDistanceCm = 150.0;

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
	void FreezeOwnedSplineTransform();
	void InitializePathNoise();
	double GetPathNoiseFade(double SplineLength) const;
	double GetPathNoiseSpeedScale(double SplineLength) const;
	FVector ApplyPathNoise(double DistanceCm, double SplineLength, const FVector& BaseLocation) const;
	void MoveOwnerToCurrentDistance(double DeltaSeconds = 0.0);

	double LateralNoisePhase = 0.0;
	double SpeedNoisePhase = 0.0;
};
