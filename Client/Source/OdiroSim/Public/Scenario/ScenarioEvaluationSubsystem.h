#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeResultTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotSimulationFailureInfo.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioEvaluationSubsystem.generated.h"

class ADeliveryBot;
struct FDeliveryBotPolicyEventSnapshot;
class AActor;
class ADeliveryBot_ChaosActor;
class AScenarioGroundRegion;
class UPrimitiveComponent;
struct FScenarioRuntimeCorridorSurfaceQueryResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEndedSignature, FEpisodeEvaluationResult, result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEventSignature, FEpisodeEvaluationEvent, event);
DECLARE_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEndRequestedNative, const FEpisodeEvaluationResult&);

// 현재 월드의 episode runtime을 관찰하고 평가 결과와 종료 결정을 생성하는 subsystem.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEvaluationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Evaluation")
	FEpisodeEvaluationEndedSignature OnEpisodeEnded;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Evaluation")
	FEpisodeEvaluationEventSignature OnEvaluationEvent;

	FEpisodeEvaluationEndRequestedNative OnEpisodeEndRequested;	// Runner가 구독하는 Episode 종료 요청 이벤트다.

	UFUNCTION(BlueprintCallable, Category = "Scenario|Evaluation")
	bool StartEvaluation(
		const FScenarioEvaluationConfig& evaluationConfig,
		const FScenarioRuntimeContext& runtimeContext,
		double inTimeLimitSeconds);

	// 외부 종료 절차가 끝난 뒤 Episode를 최종 완료한다.
	void CompleteEndEpisode();

	// 외부 종료 완료를 기다리는지 반환한다.
	bool IsAwaitingEndFinalization() const
	{
		return bAwaitingEndFinalization;
	}

	UFUNCTION(BlueprintCallable, Category = "Scenario|Evaluation")
	void StopEvaluation();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Evaluation")
	void RequestEndEpisode(const FEpisodeEvaluationResult& result);

	UFUNCTION(BlueprintPure, Category = "Scenario|Evaluation")
	bool IsEvaluating() const { return bEvaluating; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Evaluation")
	FEpisodeEvaluationResult GetCurrentResult() const { return CurrentResult; }

	UFUNCTION()
	void HandleDeliveryBotSimulationFailed(
		ADeliveryBot* DeliveryBotActor,
		const FDeliveryBotSimulationFailureInfo& FailureInfo);

	void ReportDeliveryBotPolicyEvent(
		ADeliveryBot* DeliveryBotActor,
		const FDeliveryBotPolicyEventSnapshot& Snapshot); // Python policy/server event snapshot을 episode event로 기록한다

	virtual void Tick(float deltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	struct FNearMissIntervalState
	{
		double StartTimeSeconds = 0.0;
		double LastInsideTimeSeconds = 0.0;
		double MinDistanceCm = TNumericLimits<double>::Max();
		FVector ClosestRobotLocation = FVector::ZeroVector;
		FVector ClosestPedestrianLocation = FVector::ZeroVector;
	};

	struct FPenaltyRegionState
	{
		double EnterTimeSeconds = 0.0;
		bool bInside = false;
		bool bEventRecorded = false;
	};

	struct FBlockedRegionState
	{
		bool bInside = false;
	};

	static FScenarioParamValue MakeBoolParam(bool value);
	static FScenarioParamValue MakeIntegerParam(int32 value);
	static FScenarioParamValue MakeFloatParam(double value);
	static FScenarioParamValue MakeStringParam(const FString& value);
	static FScenarioParamValue MakeVectorParam(const FVector& value);

	void AddEvaluationEvent(
		EEpisodeEvaluationEventType eventType,
		EEpisodeEvaluationEventSeverity severity,
		const FString& message);
	void AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType eventType,
		EEpisodeEvaluationEventSeverity severity,
		const FString& message,
		const FString& targetInstanceId,
		const FVector& location,
		double value,
		const TMap<FString, FScenarioParamValue>& properties);
	void PublishEvaluationEvent(FEpisodeEvaluationEvent& event);

	void BindEvaluationHitDelegates();
	void BindActorHitDelegates(AActor* actor);
	void UnbindEvaluationHitDelegates();
	bool IsHitComponentBound(const UPrimitiveComponent* primitiveComponent) const;

	UFUNCTION()
	void HandleObservedComponentHit(
		UPrimitiveComponent* hitComponent,
		AActor* otherActor,
		UPrimitiveComponent* otherComp,
		FVector normalImpulse,
		const FHitResult& hit);

	bool CheckGoalReached();
	bool CheckRobotTipOver();
	void UpdateBlockedRegionViolations();
	void UpdatePenaltyRegionViolations();
	// Records a non-terminal stuck event once the robot stops making goal progress.
	void UpdateStuckDetection();
	// Adds Corridor-relative position fields for an event actor or impact point when sampled Corridor data is available.
	bool TryFindCorridorSurfaceAtWorldLocation(
		const FVector& location,
		FScenarioRuntimeCorridorSurfaceQueryResult& outSurface) const;
	// Writes event snapshot aliases that match the external events.jsonl property contract.
	void AddCorridorSnapshotProperties(
		TMap<FString, FScenarioParamValue>& properties,
		const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
		const FString& prefix) const;
	// Adds robot Corridor-relative fields for runtime detector snapshots.
	void AddRobotCorridorSnapshotProperties(TMap<FString, FScenarioParamValue>& properties) const;
	// Adds target Corridor-relative fields for collision and near-miss snapshots.
	void AddTargetCorridorSnapshotProperties(
		TMap<FString, FScenarioParamValue>& properties,
		const FVector& targetLocation) const;
	// Records a blocked sampled Corridor lane collision with surface-level identity.
	void RecordBlockedCorridorSurfaceCollision(
		const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
		AActor* targetActor,
		const FVector& location,
		const FString& message);
	// Records a penalty sampled Corridor lane violation after its dwell condition is met.
	void AddPenaltyCorridorSurfaceViolation(
		const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
		const FVector& location,
		double enterTimeSeconds,
		double durationSeconds,
		double requiredDurationSeconds);
	void UpdateNearMisses();
	void FlushActiveNearMisses();
	void CloseNearMissInterval(
		const FString& pedestrianInstanceId,
		const FNearMissIntervalState& state,
		double endTimeSeconds);
	void SetFloatMetric(const FString& key, double value);
	void FinishEpisode(
		bool bSuccess,
		EEpisodeEvaluationOutcome outcome,
		EEpisodeEvaluationTerminalReason terminalReason);
	void RecordCollisionEvent(
		EEpisodeEvaluationEventType eventType,
		AActor* targetActor,
		const FVector& location,
		double eventValue,
		const FString& message);
	bool HasWarningEvents() const;
	bool IsRobotActor(const AActor* actor) const;
	bool ContainsRuntimeActor(const TArray<TObjectPtr<AActor>>& actors, const AActor* actor) const;
	FString GetActorInstanceId(const AActor* actor) const;

	double GetElapsedTimeSeconds() const;
	// Starts a new stuck detection window from the supplied robot state.
	void ResetStuckDetectionWindow(
		double elapsedTimeSeconds,
		const FVector& robotLocation,
		double distanceToGoalCm);
	// Clears stuck detector state between episodes.
	void ClearStuckDetectionState();
	void EndForTimeout();

	UPROPERTY(Transient)
	FScenarioEvaluationConfig ActiveEvaluationConfig;

	UPROPERTY(Transient)
	FScenarioRuntimeContext ActiveRuntimeContext;

	UPROPERTY(Transient)
	FEpisodeEvaluationResult CurrentResult;

	bool bEvaluating = false;
	bool bAwaitingEndFinalization = false;	// 평가 결과를 동결하고 외부 종료 완료를 기다리는 상태다.
	double EvaluationStartTimeSeconds = 0.0;
	double TimeLimitSeconds = 0.0;
	int32 NearMissCount = 0;
	double NearMissTotalDurationSeconds = 0.0;
	double NearMissMinDistanceCm = TNumericLimits<double>::Max();
	TMap<FString, FNearMissIntervalState> ActiveNearMisses;
	TMap<FString, FPenaltyRegionState> PenaltyRegionStates;
	TMap<FString, FBlockedRegionState> BlockedRegionStates;
	TMap<FString, double> LastCollisionEventTimes;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundHitComponents;

	int32 GoalReachedCount = 0;
	int32 RobotTipOverCount = 0;
	int32 StaticObstacleCollisionCount = 0;
	int32 BlockedRegionCollisionCount = 0;
	int32 PenaltyRegionViolationCount = 0;
	int32 PedestrianCollisionCount = 0;
	// Number of non-terminal stuck events recorded for the current episode.
	int32 StuckEventCount = 0;
	// True after the stuck detector records its single diagnostic event.
	bool bStuckEventRecorded = false;
	// True once the stuck detector has a baseline sample for the current window.
	bool bHasStuckDetectionSample = false;
	// Episode elapsed time at the start of the current stuck detection window.
	double StuckWindowStartTimeSeconds = 0.0;
	// Distance to goal at the start of the current stuck detection window.
	double StuckWindowStartDistanceToGoalCm = 0.0;
	// Last sampled robot location used to estimate observed movement speed.
	FVector LastStuckSampleLocation = FVector::ZeroVector;
	// Episode elapsed time for the last observed movement speed sample.
	double LastStuckSampleTimeSeconds = 0.0;
	// Last observed movement speed estimated from robot transform deltas.
	double LastObservedRobotSpeedCmPerSecond = 0.0;

	static constexpr double NearMissClearanceGraceSeconds = 0.25;
	static constexpr double CollisionEventCooldownSeconds = 0.5;
};
