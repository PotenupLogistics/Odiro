#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/ScenarioPedestrianPlanTypes.h"
#include "ScenarioPedestrianRuntimeComponent.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EScenarioPedestrianRuntimeState : uint8
{
	FollowBaseline,
	YieldSlowdown,
	YieldStop,
	Sidestep,
	Blocked,
	Recover
};

UCLASS(ClassGroup = (Scenario), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ODIROSIM_API UScenarioPedestrianRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScenarioPedestrianRuntimeComponent();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	FString InstanceId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	FString PlanId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	FString PlanHash;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	FString BehaviorHash;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	FString PedestrianScenarioHash;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	TWeakObjectPtr<AActor> RobotActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime")
	bool bEnableRobotReaction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm/s"))
	double SpeedCmPerSecond = 120.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double VerticalOffsetCm = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double InitialDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double CurrentDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime", meta = (ClampMin = "0.0", Units = "cm"))
	double TotalDistanceCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	TArray<FScenarioPedestrianPlanPoint> PlanPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianRuntime")
	FScenarioPedestrianBehaviorParams BehaviorParams;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianRuntime")
	EScenarioPedestrianRuntimeState CurrentState = EScenarioPedestrianRuntimeState::FollowBaseline;

	// Baseline plan speed. delay 측정과 nominal animation 기준에 쓰는 변하지 않는 속도다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianAnimation", meta = (ClampMin = "0.0", Units = "cm/s"))
	double NominalSpeedCmPerSecond = 120.0;

	// Baseline path를 따라 실제로 진행 중인 속도다. yield/stop/sidestep state의 speed scale이 반영된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianAnimation", meta = (ClampMin = "0.0", Units = "cm/s"))
	double ProgressSpeedCmPerSecond = 120.0;

	// Baseline path 기준 좌우 offset 변화 속도다. sidestep/recover animation bridge에 사용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianAnimation", meta = (Units = "cm/s"))
	double LateralSpeedCmPerSecond = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ActiveTimeSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ScheduleDelaySeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double ForcedWaitSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "s"))
	double BlockedDurationSeconds = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "cm"))
	double PathDeviationCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxPathDeviationCm = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|PedestrianMetrics", meta = (Units = "cm"))
	double MinRobotDistanceCm = -1.0;

	void ConfigurePlan(
		const FString& inInstanceId,
		const FScenarioPedestrianPlan& plan,
		double fallbackSpeedCmPerSecond,
		double initialDistanceCm,
		bool bStartAutomatically);

	UFUNCTION(BlueprintCallable, Category = "Scenario|PedestrianRuntime")
	void SetRobotActor(AActor* inRobotActor);

	UFUNCTION(BlueprintCallable, Category = "Scenario|PedestrianRuntime")
	void StartFollowing();

	UFUNCTION(BlueprintCallable, Category = "Scenario|PedestrianRuntime")
	void StopFollowing();

	UFUNCTION(BlueprintPure, Category = "Scenario|PedestrianRuntime")
	bool HasPlan() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

private:
	struct FRobotConflict
	{
		TWeakObjectPtr<AActor> RobotActor;
		FVector RobotLocation = FVector::ZeroVector;
		FVector RobotVelocity = FVector::ZeroVector;
		FVector ClosestRobotLocation = FVector::ZeroVector;
		FVector ClosestPedestrianLocation = FVector::ZeroVector;
		double RobotRadiusCm = 0.0;
		double WarningDistanceCm = 0.0;
		double StopDistanceCm = 0.0;
		double TimeToClosestSeconds = 0.0;
		double ClosestDistanceCm = TNumericLimits<double>::Max();
		double PedestrianDistanceAtClosestCm = 0.0;
		double Severity = 0.0;
		bool bHasConflict = false;
		bool bHardConflict = false;
	};

	struct FSidestepCandidate
	{
		int32 Side = 0;
		double TargetOffsetCm = 0.0;
		double MinDistanceCm = TNumericLimits<double>::Max();
		double Score = -TNumericLimits<double>::Max();
		bool bValid = false;
		bool bMeetsClearance = false;
	};

	FVector GetLocationAtDistance(double distanceCm) const;
	FVector GetDirectionAtDistance(double distanceCm) const;
	FVector GetRightAtDistance(double distanceCm) const;
	FVector GetActualLocationAtDistance(double distanceCm, double lateralOffsetCm) const;
	void MoveOwnerToCurrentDistance(double deltaSeconds = 0.0, bool bSnapRotation = false);
	FRobotConflict FindMostSevereRobotConflict() const;
	void UpdateRobotDistanceMetrics();
	void UpdateRobotKinematics(double deltaSeconds);
	FVector ResolveRobotVelocity(const AActor* robotActor) const;
	FVector PredictRobotLocation(
		const FVector& robotLocation,
		const FVector& robotVelocity,
		double sampleTimeSeconds) const;
	double GetActorRadiusCm(const AActor* actor) const;
	double GetConflictWarningDistanceCm(double robotRadiusCm) const;
	double GetConflictStopDistanceCm(double robotRadiusCm) const;
	double GetSidestepLimitCm() const;
	bool ShouldSidestepForConflict(const FRobotConflict& conflict) const;
	double ComputeSidestepTargetForSide(const FRobotConflict& conflict, int32 side) const;
	int32 ComputeDefaultSidestepSide(const FRobotConflict& conflict) const;
	FSidestepCandidate EvaluateSidestepCandidate(const FRobotConflict& conflict, int32 side) const;
	FSidestepCandidate ChooseSidestepCandidate(const FRobotConflict& conflict) const;
	bool RequestSidestep(const FRobotConflict& conflict);
	void ResetSidestepLock();
	double ComputeSpeedScale(const FRobotConflict& conflict) const;
	void UpdateRuntimeState(const FRobotConflict& conflict, double deltaSeconds);
	void SetRuntimeState(EScenarioPedestrianRuntimeState newState, const FRobotConflict& conflict);
	bool CanLeaveCurrentState() const;
	double GetMinimumStateHoldSeconds() const;
	bool IsStateEscalation(EScenarioPedestrianRuntimeState newState) const;
	void RequestLateralOffset(double targetOffsetCm);
	double ComputeLateralCurveDurationSeconds(double startOffsetCm, double targetOffsetCm) const;
	void UpdateLateralOffset(double deltaSeconds);
	void UpdateAnimationKinematics(double speedScale, double previousLateralOffsetCm, double deltaSeconds);
	void UpdateScheduleDelay();
	void ResetRuntimeMetrics();

	// 현재 baseline path에서 벗어난 좌우 offset과 목표 offset이다. actor location은 baseline + offset으로 계산된다.
	double ActualLateralOffsetCm = 0.0;
	double TargetLateralOffsetCm = 0.0;

	// Sidestep/recover를 직선 보간하지 않고 짧은 smootherstep curve로 재생하기 위한 curve 상태다.
	double LateralCurveStartOffsetCm = 0.0;
	double LateralCurveTargetOffsetCm = 0.0;
	double LateralCurveElapsedSeconds = 0.0;
	double LateralCurveDurationSeconds = 0.0;

	// State 최소 유지 시간과 conflict latch/hysteresis를 위한 runtime 상태다.
	double StateElapsedSeconds = 0.0;
	double ClearConflictElapsedSeconds = 0.0;
	double StoppedByRobotSeconds = 0.0;
	double CachedRobotRadiusCm = 0.0;
	FVector LastRobotLocation = FVector::ZeroVector;
	FVector ObservedRobotVelocityCmPerSecond = FVector::ZeroVector;
	bool bLateralCurveActive = false;
	bool bConflictLatched = false;
	bool bHasLastRobotLocation = false;
	bool bSidestepTargetLocked = false;
	int32 ActiveSidestepSide = 0;
	double ActiveSidestepTargetOffsetCm = 0.0;
};
