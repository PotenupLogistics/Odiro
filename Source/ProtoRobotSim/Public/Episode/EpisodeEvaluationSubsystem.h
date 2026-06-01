#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeSpecTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeEvaluationSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodeEvaluationEndedSignature, FEpisodeEvaluationResult, Result);

// 현재 월드의 episode runtime actor를 관찰하고 평가 결과와 종료 결정을 생성하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeEvaluationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Episode|Evaluation")
	FEpisodeEvaluationEndedSignature OnEpisodeEnded;

	UFUNCTION(BlueprintCallable, Category = "Episode|Evaluation")
	bool StartEvaluation(const FEpisodeWorldSpec& WorldSpec, const FEpisodeRuntimeContext& RuntimeContext);

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

	static double GetFloatProperty(
		const TMap<FString, FEpisodeParamValue>& Properties,
		const FString& Key,
		double DefaultValue);

	static FEpisodeParamValue MakeFloatParam(double Value);
	static FEpisodeParamValue MakeStringParam(const FString& Value);

	void AddEvaluationEvent(
		const FString& EventType,
		EEpisodeEvaluationEventSeverity Severity,
		const FString& Message);

	void UpdateNearMisses();
	void FlushActiveNearMisses();
	void CloseNearMissInterval(
		const FString& PedestrianInstanceId,
		const FNearMissIntervalState& State,
		double EndTimeSeconds);
	void SetFloatMetric(const FString& Key, double Value);
	void AddScore(double ScoreDelta);

	double GetElapsedTimeSeconds() const;
	void EndForTimeout();

	UPROPERTY(Transient)
	FEpisodeWorldSpec ActiveWorldSpec;

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

	static constexpr double NearMissClearanceGraceSeconds = 0.25;
};
