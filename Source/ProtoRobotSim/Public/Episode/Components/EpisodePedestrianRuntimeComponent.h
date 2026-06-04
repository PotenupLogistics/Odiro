#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/EpisodePedestrianPlanTypes.h"
#include "EpisodePedestrianRuntimeComponent.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EEpisodePedestrianRuntimeState : uint8
{
	FollowBaseline,
	YieldSlowdown,
	YieldStop,
	Sidestep,
	Blocked,
	Recover
};

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString BehaviorHash;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString PedestrianScenarioHash;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	TWeakObjectPtr<AActor> RobotActor;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	FString RobotInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime")
	bool bEnableRobotReaction = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianRuntime")
	FEpisodePedestrianBehaviorParams BehaviorParams;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianRuntime")
	EEpisodePedestrianRuntimeState CurrentState = EEpisodePedestrianRuntimeState::FollowBaseline;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ActiveTimeSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ScheduleDelaySeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ForcedWaitSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double BlockedDurationSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "cm"))
	double PathDeviationCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxPathDeviationCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics", meta = (Units = "cm"))
	double MinRobotDistanceCm = -1.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|PedestrianMetrics")
	FString LastConflictRobotInstanceId;

	void ConfigurePlan(
		const FString& inInstanceId,
		const FEpisodePedestrianPlan& plan,
		double fallbackSpeedCmPerSecond,
		double initialDistanceCm,
		bool bStartAutomatically);

	UFUNCTION(BlueprintCallable, Category = "Episode|PedestrianRuntime")
	void SetRobotActor(AActor* inRobotActor, const FString& inRobotInstanceId);

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
	struct FRobotConflict
	{
		TWeakObjectPtr<AActor> RobotActor;
		FString RobotInstanceId;
		FVector RobotLocation = FVector::ZeroVector;
		FVector RobotVelocity = FVector::ZeroVector;
		double RobotRadiusCm = 0.0;
		double TimeToClosestSeconds = 0.0;
		double ClosestDistanceCm = TNumericLimits<double>::Max();
		double PedestrianDistanceAtClosestCm = 0.0;
		double Severity = 0.0;
		bool bHasConflict = false;
		bool bHardConflict = false;
	};

	FVector GetLocationAtDistance(double distanceCm) const;
	FVector GetDirectionAtDistance(double distanceCm) const;
	FVector GetRightAtDistance(double distanceCm) const;
	FVector GetActualLocationAtDistance(double distanceCm, double lateralOffsetCm) const;
	void MoveOwnerToCurrentDistance(double deltaSeconds = 0.0);
	FRobotConflict FindMostSevereRobotConflict() const;
	void UpdateRobotDistanceMetrics();
	double GetActorRadiusCm(const AActor* actor) const;
	double GetConflictWarningDistanceCm(double robotRadiusCm) const;
	double GetConflictStopDistanceCm(double robotRadiusCm) const;
	bool ShouldSidestepForConflict(const FRobotConflict& conflict) const;
	double ComputeSidestepTargetCm(const FRobotConflict& conflict) const;
	double ComputeSpeedScale(const FRobotConflict& conflict) const;
	void UpdateRuntimeState(const FRobotConflict& conflict, double deltaSeconds);
	void SetRuntimeState(EEpisodePedestrianRuntimeState newState, const FRobotConflict& conflict);
	void UpdateLateralOffset(double deltaSeconds);
	void UpdateScheduleDelay();
	void ResetRuntimeMetrics();

	double ActualLateralOffsetCm = 0.0;
	double TargetLateralOffsetCm = 0.0;
	double StoppedByRobotSeconds = 0.0;
	double CachedRobotRadiusCm = 0.0;
};
