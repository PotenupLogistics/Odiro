#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/SimulationSetupTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimulatorLaunchSubsystem.generated.h"

// Main menu process가 별도 simulator process 하나를 실행하고 status file로 추적한 결과
USTRUCT(BlueprintType)
struct ODIROSIM_API FSimulatorRunInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString RunId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString SetupPath;

	// Project-run mode에서 Bridge가 만든 사용자 project root.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString ProjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString StatusPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchExecutable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchArguments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bUsedPreviewLauncher = false;

	// true이면 legacy SimulationSetup 대신 project/run snapshot 계약으로 실행한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bProjectRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bProcessStarted = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bProcessRunning = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	int32 ProcessReturnCode = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FSimulationRunStatus Status;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	TArray<FString> Diagnostics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LastError;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSimulatorRunInfoChangedNative, const FSimulatorRunInfo&);

// Platform UI에서 simulation setup을 골라 별도 simulator process를 실행하는 launcher
UCLASS(BlueprintType)
class ODIROSIM_API USimulatorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	FSimulatorRunInfoChangedNative OnRunInfoChanged;

	// Legacy Json/Input SimulationSetup 목록. 새 실행은 StartProjectRun을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy SimulationSetup API. Use user project run APIs."))
	TArray<FString> ListSimulationSetupFiles() const;

	// Legacy Json/Input ScenarioSetup 목록. 새 실행은 project scenario.json을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy ScenarioSetup API. Use user project scenario.json."))
	TArray<FString> ListScenarioSetupFiles() const;

	// Legacy Json/Input DeliveryBotSetup 목록. 새 실행은 project profile.json과 policy/를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy DeliveryBotSetup API. Use user project profile.json and policy/."))
	TArray<FString> ListDeliveryBotSetupFiles() const;

	// Legacy PolicySpec 목록. 새 실행은 project policy/를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy PolicySpec API. Use user project policy/."))
	TArray<FString> ListPolicySpecFiles() const;

	// Legacy RunQueue 목록. 새 실행은 RunQueue를 만들지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Use user project runs."))
	TArray<FString> ListScenarioRunQueueFiles() const;

	// Legacy evaluation report 목록. 새 결과는 user project runs/<RunId>/에 기록한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy report API. Use user project run artifacts."))
	TArray<FString> ListEvaluationReportFiles() const;

	// Legacy status 목록. 새 run 상태는 project run status.json을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy status API. Use user project run status.json."))
	TArray<FString> ListSimulationRunStatusFiles() const;

	// Legacy run 결과 폴더 목록. 새 결과는 user project runs/<RunId>/에 기록한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy result directory API. Use user project runs."))
	TArray<FString> ListSimulationRunResultDirectories() const;

	// Legacy evaluation report 조회. 새 결과는 result.json과 summary.json을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy report API. Use user project result.json and summary.json."))
	TArray<FString> ListEvaluationReportFilesInDirectory(const FString& runDirectory) const;

	// Legacy measurement log 조회. 새 결과는 actions/events/trace JSONL을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy measurement log API. Use user project actions/events/trace JSONL."))
	TArray<FString> ListMeasurementLogFilesInDirectory(const FString& runDirectory) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy SimulationSetup API. Use user project setting/profile/scenario files."))
	FSimulationSetupParseResult LoadSimulationSetupFile(const FString& setupPath) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy SimulationSetup API. Save user project setting.json instead."))
	bool SaveFixedStepFpsToSetupFile(const FString& setupPath, int32 fps, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy SimulationSetup API. Save user project setting.json instead."))
	bool SaveSimulationSetupFile(const FString& setupPath, const FSimulationSetup& setup, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Use user project runs."))
	bool LoadScenarioRunQueueFile(
		const FString& runQueuePath,
		TArray<FScenarioRunInput>& outRunInputs,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not create RunQueue files for user project runs."))
	bool SaveScenarioRunQueueFile(
		const FString& runQueuePath,
		const TArray<FScenarioRunInput>& runInputs,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not create RunQueue files for user project runs."))
	bool AppendRunQueuePair(
		const FString& runQueuePath,
		const FString& pairId,
		const FString& scenarioSetupPath,
		const FString& deliveryBotSetupPath,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not edit RunQueue files for user project runs."))
	bool RemoveRunQueuePair(
		const FString& runQueuePath,
		int32 runIndex,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not edit RunQueue files for user project runs."))
	bool MoveRunQueuePair(
		const FString& runQueuePath,
		int32 runIndex,
		int32 direction,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not edit RunQueue files for user project runs."))
	bool ReplaceScenarioSetupReferencesInRunQueues(
		const FString& oldScenarioSetupPath,
		const FString& newScenarioSetupPath,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy RunQueue API. Do not edit RunQueue files for user project runs."))
	bool ReplaceDeliveryBotSetupReferencesInRunQueues(
		const FString& oldDeliveryBotSetupPath,
		const FString& newDeliveryBotSetupPath,
		TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy SimulationSetup launch API. Use StartProjectRun."))
	bool StartSimulationRun(const FString& setupPath, const FString& requestedRunId);

	// 사용자 project root와 run id로 simulator process를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool StartProjectRun(const FString& projectPath, const FString& runId);

	// 개발/배포 resource에 포함된 project template id 목록을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	TArray<FString> ListProjectTemplates() const;

	// 사용자 project root의 최소 계약을 검증한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	bool ValidateUserProject(const FString& projectPath, TArray<FString>& outDiagnostics) const;

	// 선택한 template을 새 사용자 project root로 복사한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	bool CreateProjectFromTemplate(const FString& projectPath, const FString& templateId, TArray<FString>& outDiagnostics) const;

	// 사용자 project 입력을 새 run snapshot으로 고정한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	bool CreateProjectRun(const FString& projectPath, FString& outRunId, TArray<FString>& outDiagnostics) const;

	// 사용자 project 입력을 지정 또는 다음 run id snapshot으로 고정한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool PrepareProjectRunSnapshot(
		const FString& projectPath,
		const FString& requestedRunId,
		FString& outRunId,
		TArray<FString>& outDiagnostics) const;

	// 사용자 project의 run directory 목록을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	TArray<FString> ListProjectRunDirectories(const FString& projectPath) const;

	// project run directory의 episode result.json 목록을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	TArray<FString> ListProjectEpisodeResultFiles(const FString& runDirectory) const;

	// project run directory의 episode JSONL log 목록을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Project")
	TArray<FString> ListProjectRunLogFiles(const FString& runDirectory) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool RefreshActiveRunStatus();

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	void StopActiveRun();

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	FSimulatorRunInfo GetActiveRunInfo() const { return ActiveRunInfo; }

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	FString GetLastError() const { return ActiveRunInfo.LastError; }

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static bool IsTerminalRunState(ESimulationRunState state);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString QuoteCommandLineArgument(const FString& value);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildSimulatorArgumentString(const FString& setupPath, const FString& runId);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildProjectRunSimulatorArgumentString(const FString& projectPath, const FString& runId);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildPreviewLauncherArgumentString(
		const FString& previewBatPath,
		const FString& setupPath,
		const FString& runId);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildProjectRunPreviewLauncherArgumentString(
		const FString& previewBatPath,
		const FString& projectPath,
		const FString& runId);

	// Legacy RunQueue parser helper. 호환 테스트와 tooling에서만 사용한다.
	static bool TryReadScenarioRunQueueJson(
		const FString& jsonString,
		TArray<FScenarioRunInput>& outRunInputs,
		TArray<FString>& outDiagnostics);

	// Legacy RunQueue writer helper. 호환 테스트와 tooling에서만 사용한다.
	static bool TryWriteScenarioRunQueueJson(
		const TArray<FScenarioRunInput>& runInputs,
		FString& outJson,
		TArray<FString>& outDiagnostics);

private:
	bool CreateProjectRunSnapshot(
		const FString& projectPath,
		const FString& requestedRunId,
		FString& outRunId,
		TArray<FString>& outDiagnostics) const;

	bool ReplaceRunQueueReferences(
		const FString& oldPath,
		const FString& newPath,
		bool bReplaceScenarioSetupReference,
		TArray<FString>& outDiagnostics) const;
	bool BuildLaunchCommand(const FString& setupPath, const FString& runId, FString& outExecutable, FString& outArguments, bool& bOutUsesPreviewLauncher) const;
	bool BuildProjectRunLaunchCommand(const FString& projectPath, const FString& runId, FString& outExecutable, FString& outArguments, bool& bOutUsesPreviewLauncher) const;
	bool ShouldUsePreviewLauncher(FString& outPreviewBatPath) const;
	bool CreateRuntimeSimulationSetupFile(
		const FSimulationSetup& sourceSetup,
		const FString& runId,
		FString& outRuntimeSetupPath,
		FSimulationSetup& outRuntimeSetup,
		TArray<FString>& outDiagnostics) const;
	void PollActiveRunStatus();
	void StartPolling();
	void StopPolling();
	void CloseActiveProcessHandle();
	void RefreshActiveProcessState();
	void MarkActiveRunFailed(const FString& error);
	void BroadcastRunInfoChanged();
	void SetActiveRunDiagnostics(const TArray<FString>& diagnostics);

	UPROPERTY(Transient)
	FSimulatorRunInfo ActiveRunInfo;

	FProcHandle ActiveProcessHandle;
	FTimerHandle PollTimerHandle;
};
