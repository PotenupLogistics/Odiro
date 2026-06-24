#pragma once

#include "CoreMinimal.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "ExperimentResultViewModel.generated.h"

class UGameInstance;
class UExperimentResultEpisodeViewModel;
class UExperimentResultSuggestionViewModel;
class UPlatformUiSubsystem;
struct FPlatformAnalysisAiResponse;

// project run result dashboard의 표시 상태와 AI 분석 command를 소유하는 ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UExperimentResultViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 PlatformAnalysisAiSubsystem 참조를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 UI subsystem을 주입할 때 사용한다.
	void SetSubsystemOverride(UPlatformUiSubsystem* platformUiSubsystem);

	// run directory의 summary/review JSON을 dashboard 상태로 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	bool LoadRunDirectory(const FString& runDirectory);

	// 선택된 run에 대한 AI 분석을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	bool RequestAiAnalysis(const FString& projectPath, const FString& runId);

	// 선택된 run directory 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetSelectedRunDirectory() const { return SelectedRunDirectory; }

	// 선택된 run id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetRunId() const { return RunId; }

	// dashboard 원본 집계 데이터를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FProjectRunResultDashboardData GetDashboardData() const { return DashboardData; }

	// 총 플레이 시간 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetTotalDurationLabel() const { return TotalDurationLabel; }

	// 성공률 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetSuccessRateLabel() const { return SuccessRateLabel; }

	// 충돌 횟수 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetCollisionCountLabel() const { return CollisionCountLabel; }

	// AI summary 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetAiSummaryText() const { return AiSummaryText; }

	// Episode replay card item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	TArray<UExperimentResultEpisodeViewModel*> GetEpisodeItems() const;

	// AI suggestion row item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	TArray<UExperimentResultSuggestionViewModel*> GetSuggestionItems() const;

private:
	UPlatformUiSubsystem* ResolvePlatformUiSubsystem() const;
	void HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response);
	void SetSelectedRunDirectory(const FString& runDirectory);
	void SetRunId(const FString& runId);
	void SetDashboardData(const FProjectRunResultDashboardData& dashboardData);
	void RefreshDisplayLabels();
	void RebuildDashboardItemViewModels();

	// Subsystem lookup에 사용할 GameInstance.
	UPROPERTY(Transient)
	TObjectPtr<UGameInstance> GameInstance;

	// 테스트/host가 명시 주입한 platform UI subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UPlatformUiSubsystem> PlatformUiOverride;

	// 현재 dashboard가 표시하는 run directory.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString SelectedRunDirectory;

	// 현재 dashboard가 표시하는 run id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString RunId;

	// summary/review JSON에서 만든 dashboard 원본 데이터.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FProjectRunResultDashboardData DashboardData;

	// 총 플레이 시간 metric 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString TotalDurationLabel;

	// 성공률 metric 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString SuccessRateLabel;

	// 충돌 횟수 metric 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString CollisionCountLabel;

	// AI 분석 요약 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString AiSummaryText;

	// Episode replay 반복 card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UExperimentResultEpisodeViewModel>> EpisodeItems;

	// AI suggestion 반복 row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UExperimentResultSuggestionViewModel>> SuggestionItems;

	// 진행 중인 AI 분석이 대상으로 삼은 run id.
	FString PendingAnalysisRunId;
};
