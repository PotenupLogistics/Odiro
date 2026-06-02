#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeEvaluationSubsystem.generated.h"

class AActor;
class AEpisodeGroundRegion;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEndedSignature, FEpisodeEvaluationResult, Result);

// 현재 월드의 episode runtime을 관찰하고 평가 결과와 종료 결정을 생성하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeEvaluationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Episode|Evaluation")
	FEpisodeEvaluationEndedSignature OnEpisodeEnded;

	UFUNCTION(BlueprintCallable, Category = "Episode|Evaluation")
	bool StartEvaluation(
		const FEpisodeEvaluationConfig& EvaluationConfig,
		const FEpisodeRuntimeContext& RuntimeContext,
		double InTimeLimitSeconds);

	UFUNCTION(BlueprintCallable, Category = "Episode|Evaluation")
	void StopEvaluation();

	UFUNCTION(BlueprintCallable, Category = "Episode|Evaluation")
	void RequestEndEpisode(const FEpisodeEvaluationResult& Result);

	UFUNCTION(BlueprintPure, Category = "Episode|Evaluation")
	bool IsEvaluating() const { return bEvaluating; }

	UFUNCTION(BlueprintPure, Category = "Episode|Evaluation")
	FEpisodeEvaluationResult GetCurrentResult() const { return CurrentResult; }

	virtual void Tick(float DeltaTime) override;
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

	static FEpisodeParamValue MakeFloatParam(double Value);
	static FEpisodeParamValue MakeStringParam(const FString& Value);

	void AddEvaluationEvent(
		EEpisodeEvaluationEventType EventType,
		EEpisodeEvaluationEventSeverity Severity,
		const FString& Message);
	void AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType EventType,
		EEpisodeEvaluationEventSeverity Severity,
		const FString& Message,
		const FString& TargetInstanceId,
		const FVector& Location,
		double Value,
		const TMap<FString, FEpisodeParamValue>& Properties);

	void BindEvaluationHitDelegates();
	void BindActorHitDelegates(AActor* Actor);
	void UnbindEvaluationHitDelegates();
	bool IsHitComponentBound(const UPrimitiveComponent* PrimitiveComponent) const;

	UFUNCTION()
	void HandleObservedComponentHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	bool CheckGoalReached();
	bool CheckRobotFall();
	void UpdateBlockedRegionViolations();
	void UpdatePenaltyRegionViolations();
	void UpdateNearMisses();
	void FlushActiveNearMisses();
	void CloseNearMissInterval(
		const FString& PedestrianInstanceId,
		const FNearMissIntervalState& State,
		double EndTimeSeconds);
	void SetFloatMetric(const FString& Key, double Value);
	void AddScore(double ScoreDelta);
	void FinishEpisode(
		bool bSuccess,
		EEpisodeEvaluationOutcome Outcome,
		EEpisodeEvaluationTerminalReason TerminalReason);
	void RecordCollisionEvent(
		EEpisodeEvaluationEventType EventType,
		AActor* TargetActor,
		const FVector& Location,
		double ScoreDelta,
		const FString& Message);
	bool HasWarningEventsOrScore() const;
	bool IsRobotActor(const AActor* Actor) const;
	bool ContainsRuntimeActor(const TArray<TObjectPtr<AActor>>& Actors, const AActor* Actor) const;
	FString GetActorInstanceId(const AActor* Actor) const;

	double GetElapsedTimeSeconds() const;
	void EndForTimeout();

	UPROPERTY(Transient)
	FEpisodeEvaluationConfig ActiveEvaluationConfig;

	UPROPERTY(Transient)
	FEpisodeRuntimeContext ActiveRuntimeContext;

	UPROPERTY(Transient)
	FEpisodeEvaluationResult CurrentResult;

	bool bEvaluating = false;
	double EvaluationStartTimeSeconds = 0.0;
	double TimeLimitSeconds = 0.0;
	double CurrentScore = 0.0;
	int32 NearMissCount = 0;
	double NearMissTotalDurationSeconds = 0.0;
	double NearMissMinDistanceCm = TNumericLimits<double>::Max();
	TMap<FString, FNearMissIntervalState> ActiveNearMisses;
	TMap<FString, FPenaltyRegionState> PenaltyRegionStates;
	TMap<FString, FBlockedRegionState> BlockedRegionStates;
	TMap<FString, double> LastCollisionEventTimes;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundHitComponents;

	int32 GoalReachedCount = 0;
	int32 RobotFallCount = 0;
	int32 StaticObstacleCollisionCount = 0;
	int32 BlockedRegionCollisionCount = 0;
	int32 PenaltyRegionViolationCount = 0;
	int32 PedestrianCollisionCount = 0;

	static constexpr double NearMissClearanceGraceSeconds = 0.25;
	static constexpr double CollisionEventCooldownSeconds = 0.5;
};
