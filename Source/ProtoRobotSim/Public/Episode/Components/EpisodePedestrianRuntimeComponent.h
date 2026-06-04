#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/EpisodePedestrianPlanTypes.h"
#include "EpisodePedestrianRuntimeComponent.generated.h"

UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UEpisodePedestrianRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEpisodePedestrianRuntimeComponent();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString InstanceId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString PlanId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString PlanHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm/s"))
	double SpeedCmPerSecond = 120.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double VerticalOffsetCm = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double InitialDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double CurrentDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double TotalDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	TArray<FEpisodePedestrianPlanPoint> PlanPoints;

	void ConfigurePlan(
		const FString& inInstanceId,
		const FEpisodePedestrianPlan& plan,
		double fallbackSpeedCmPerSecond,
		double initialDistanceCm,
		bool bStartAutomatically);

	UFUNCTION(BlueprintCallable, Category = "Episode|PedestrianRuntime")
	void StartFollowing();

	UFUNCTION(BlueprintCallable, Category = "Episode|PedestrianRuntime")
	void StopFollowing();

	UFUNCTION(BlueprintPure, Category = "Episode|PedestrianRuntime")
	bool HasPlan() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

private:
	FVector GetLocationAtDistance(double distanceCm) const;
	FVector GetDirectionAtDistance(double distanceCm) const;
	void MoveOwnerToCurrentDistance(double deltaSeconds = 0.0);
};
