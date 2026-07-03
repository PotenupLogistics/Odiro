#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "ExperimentConfigViewModel.generated.h"

class UGameInstance;
class UProjectSessionSubsystem;

// user project setting.json의 실험 설정 입력 상태와 save command를 소유하는 ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UExperimentConfigViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 ProjectSessionSubsystem 참조를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 Subsystem을 주입할 때 사용한다.
	void SetSubsystemOverride(UProjectSessionSubsystem* projectSessionSubsystem);

	// active project setting.json을 읽어 입력 상태에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	bool LoadFromActiveProject();

	// 지정 project setting.json을 읽어 입력 상태에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	bool LoadFromProject(const FString& projectPath);

	// active project setting.json에 입력 상태를 저장한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	bool SaveExperimentSettings();

	// 지정 project setting.json에 입력 상태를 저장한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	bool SaveToProject(const FString& projectPath);

	// target map id 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetMapId(const FString& mapId);

	// target map id 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	FString GetMapId() const { return MapId; }

	// fixed FPS 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetFixedFps(int32 fixedFps);

	// fixed FPS 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	int32 GetFixedFps() const { return FixedFps; }

	// time scale 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetTimeScale(float timeScale);

	// time scale 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	float GetTimeScale() const { return TimeScale; }

	// max duration seconds 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetMaxDurationSeconds(float maxDurationSeconds);

	// max duration seconds 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	float GetMaxDurationSeconds() const { return MaxDurationSeconds; }

	// episode count 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetEpisodeCount(int32 episodeCount);

	// episode count 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	int32 GetEpisodeCount() const { return EpisodeCount; }

	// base seed 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetBaseSeed(int64 baseSeed);

	// base seed 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	int64 GetBaseSeed() const { return BaseSeed; }

	// tip over angle 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetTipOverAngleDegrees(float tipOverAngleDegrees);

	// tip over angle 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	float GetTipOverAngleDegrees() const { return TipOverAngleDegrees; }

	// near miss distance 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetNearMissDistanceMeters(float nearMissDistanceMeters);

	// near miss distance 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	float GetNearMissDistanceMeters() const { return NearMissDistanceMeters; }

	// goal acceptance radius 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentConfig")
	void SetGoalAcceptanceRadiusMeters(float goalAcceptanceRadiusMeters);

	// goal acceptance radius 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	float GetGoalAcceptanceRadiusMeters() const { return GoalAcceptanceRadiusMeters; }

	// 저장 가능한 입력 상태인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentConfig")
	bool CanSaveExperimentSettings() const;

private:
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	bool ValidateInputs(TArray<FString>& outDiagnostics) const;
	FString ResolveProjectPath(const FString& projectPath) const;

	// Subsystem lookup에 사용할 GameInstance.
	UPROPERTY(Transient)
	TObjectPtr<UGameInstance> GameInstance;

	// 테스트/host가 명시 주입한 project session subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectSessionSubsystem> ProjectSessionOverride;

	// target map id 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	FString MapId = TEXT("ScenarioSimulationMap");

	// runtime.fixed_fps 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	int32 FixedFps = 60;

	// runtime.time_scale 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	float TimeScale = 1.0f;

	// runtime.max_duration_s 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	float MaxDurationSeconds = 60.0f;

	// sampling.episode_count 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	int32 EpisodeCount = 1;

	// sampling.base_seed 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	int64 BaseSeed = 0;

	// evaluation.tip_over_angle_deg 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	float TipOverAngleDegrees = 60.0f;

	// evaluation.near_miss_distance_m 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	float NearMissDistanceMeters = 0.5f;

	// evaluation.goal_acceptance_radius_m 입력 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentConfig", meta = (AllowPrivateAccess = "true"))
	float GoalAcceptanceRadiusMeters = 1.0f;
};
