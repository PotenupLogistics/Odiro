#pragma once

#include "CoreMinimal.h"
#include "Shared/ExperimentSettingTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/SimulationRunStatusTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimulatorLaunchSubsystem.generated.h"

// Main menu process launch state mirrored from the child simulator status file.
USTRUCT(BlueprintType)
struct ODIROSIM_API FSimulatorRunInfo
{
	GENERATED_BODY()

	// Stable run folder id used under the selected experiment folder.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString RunId;

	// Selected experiment folder; legacy name is kept for Blueprint compatibility.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString SetupPath;

	// Status JSON path polled by the launcher while the child process runs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString StatusPath;

	// Executable or command host used for the child simulator process.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchExecutable;

	// Full command-line arguments passed to LaunchExecutable.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchArguments;

	// True when editor fallback routing uses Task-RunPreview.bat instead of a packaged executable.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bUsedPreviewLauncher = false;

	// True after FPlatformProcess successfully created the child process.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bProcessStarted = false;

	// Last observed process liveness.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bProcessRunning = false;

	// Last return code captured after the child process exits.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	int32 ProcessReturnCode = INDEX_NONE;

	// Last parsed simulation run status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FSimulationRunStatus Status;

	// Launcher-side validation or polling diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	TArray<FString> Diagnostics;

	// Most recent launch or simulator error text shown by the main menu.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LastError;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSimulatorRunInfoChangedNative, const FSimulatorRunInfo&);

// MainMenuMap launcher for selecting an experiment folder and starting a child simulator process.
UCLASS(BlueprintType)
class ODIROSIM_API USimulatorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	FSimulatorRunInfoChangedNative OnRunInfoChanged;

	// Json/Experiments folders that contain a valid experiment setting.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListExperimentRefs() const;

	// Json/Input 아래에서 scenario_template/scenario_sample 계약으로 컴파일되는 JSON 파일 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup")
	TArray<FString> ListScenarioSetupFiles() const;

	// Json/Input 아래에서 simulation_profile 계약으로 컴파일되는 JSON 파일 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup")
	TArray<FString> ListDeliveryBotSetupFiles() const;

	// Json/Input/PolicySpecs 아래에서 policySpec update payload로 읽히는 JSON 파일 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup")
	TArray<FString> ListPolicySpecFiles() const;

	// Json/Output 아래 evaluation report JSON 후보 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListEvaluationReportFiles() const;

	// Saved/SimulationRuns 아래 simulation run status JSON 후보 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListSimulationRunStatusFiles() const;

	// Saved/SimulationRuns 아래 run별 결과 폴더 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListSimulationRunResultDirectories() const;

	// 특정 run 결과 폴더 안의 evaluation report JSON 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListEvaluationReportFilesInDirectory(const FString& runDirectory) const;

	// 특정 run 결과 폴더 안의 measurement JSONL 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListMeasurementLogFilesInDirectory(const FString& runDirectory) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	FExperimentSettingParseResult LoadExperimentSettingFile(const FString& experimentRef) const;

	// Starts a child simulator process through the canonical experiment folder contract.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool StartExperimentRun(const FString& experimentRef, const FString& requestedRunId);

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
	static FString BuildSimulatorArgumentString(const FString& experimentRef, const FString& runId);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildPreviewLauncherArgumentString(
		const FString& previewBatPath,
		const FString& experimentRef,
		const FString& runId);

private:
	bool BuildLaunchCommand(const FString& experimentRef, const FString& runId, FString& outExecutable, FString& outArguments, bool& bOutUsesPreviewLauncher) const;
	bool ShouldUsePreviewLauncher(FString& outPreviewBatPath) const;
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

	// OS process handle owned by this subsystem while a child simulator is active.
	FProcHandle ActiveProcessHandle;

	// Polling timer used to refresh status.json and process liveness.
	FTimerHandle PollTimerHandle;
};
