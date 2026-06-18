#pragma once

#include "CoreMinimal.h"
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

	// True when this launch follows the user project run snapshot contract.
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

// MainMenuMap에서 simulation setup을 골라 별도 simulator process를 실행하는 launcher
UCLASS(BlueprintType)
class ODIROSIM_API USimulatorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	FSimulatorRunInfoChangedNative OnRunInfoChanged;

	// Legacy Json/Input ScenarioSetup 목록. 새 실행은 project scenario.json을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy ScenarioSetup API. Use user project scenario.json."))
	TArray<FString> ListScenarioSetupFiles() const;

	// Legacy Json/Input DeliveryBotSetup 목록. 새 실행은 project profile.json과 policy/를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy DeliveryBotSetup API. Use user project profile.json and policy/."))
	TArray<FString> ListDeliveryBotSetupFiles() const;

	// Legacy PolicySpec 목록. 새 실행은 project policy/를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Setup", meta = (DeprecatedFunction, DeprecationMessage = "Legacy PolicySpec API. Use user project policy/."))
	TArray<FString> ListPolicySpecFiles() const;

	// Legacy status 목록. 새 run 상태는 project run status.json을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy status API. Use user project run status.json."))
	TArray<FString> ListSimulationRunStatusFiles() const;

	// Legacy run 결과 폴더 목록. 새 결과는 user project runs/<RunId>/에 기록한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy result directory API. Use user project runs."))
	TArray<FString> ListSimulationRunResultDirectories() const;


	// Legacy measurement log 조회. 새 결과는 actions/events/trace JSONL을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch", meta = (DeprecatedFunction, DeprecationMessage = "Legacy measurement log API. Use user project actions/events/trace JSONL."))
	TArray<FString> ListMeasurementLogFilesInDirectory(const FString& runDirectory) const;

	// 사용자 project root와 run id로 simulator process를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool StartProjectRun(const FString& projectPath, const FString& runId);

	// Creates a local user project run snapshot before StartProjectRun consumes it.
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool PrepareProjectRunSnapshot(
		const FString& projectPath,
		const FString& requestedRunId,
		FString& outRunId,
		TArray<FString>& outDiagnostics) const;

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
	static FString BuildProjectRunSimulatorArgumentString(const FString& projectPath, const FString& runId);

	UFUNCTION(BlueprintPure, Category = "Simulator|Launch")
	static FString BuildProjectRunPreviewLauncherArgumentString(
		const FString& previewBatPath,
		const FString& projectPath,
		const FString& runId);

private:
	bool BuildProjectRunLaunchCommand(const FString& projectPath, const FString& runId, FString& outExecutable, FString& outArguments, bool& bOutUsesPreviewLauncher) const;
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

	FProcHandle ActiveProcessHandle;
	FTimerHandle PollTimerHandle;
};
