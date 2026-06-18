#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeResultTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotSimulationFailureInfo.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioEvaluationSubsystem.generated.h"

class ADeliveryBot;
class AActor;
class ADeliveryBot_ChaosActor;
class AScenarioGroundRegion;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEndedSignature, FEpisodeEvaluationResult, result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEventSignature, FEpisodeEvaluationEvent, event);

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

	UFUNCTION(BlueprintCallable, Category = "Scenario|Evaluation")
	bool StartEvaluation(
		const FScenarioEvaluationConfig& evaluationConfig,
		const FScenarioRuntimeContext& runtimeContext,
		double inTimeLimitSeconds);

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
	void EndForTimeout();

	UPROPERTY(Transient)
	FScenarioEvaluationConfig ActiveEvaluationConfig;

	UPROPERTY(Transient)
	FScenarioRuntimeContext ActiveRuntimeContext;

	UPROPERTY(Transient)
	FEpisodeEvaluationResult CurrentResult;

	bool bEvaluating = false;
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

	static constexpr double NearMissClearanceGraceSeconds = 0.25;
	static constexpr double CollisionEventCooldownSeconds = 0.5;
};
