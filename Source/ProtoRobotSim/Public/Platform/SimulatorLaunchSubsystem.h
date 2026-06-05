#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Shared/SimulationSetupTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SimulatorLaunchSubsystem.generated.h"

// Main menu process가 별도 simulator process 하나를 실행하고 status file로 추적한 결과
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulatorRunInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString RunId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString SetupPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString StatusPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchExecutable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	FString LaunchArguments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulator|Launch")
	bool bUsedPreviewLauncher = false;

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
class PROTOROBOTSIM_API USimulatorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	FSimulatorRunInfoChangedNative OnRunInfoChanged;

	// Json/Input 아래에서 SimulationSetup 계약으로 읽히는 JSON 파일 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListSimulationSetupFiles() const;

	// Json/Output 아래 evaluation report JSON 후보 목록
	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	TArray<FString> ListEvaluationReportFiles() const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	FSimulationSetupParseResult LoadSimulationSetupFile(const FString& setupPath) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool SaveFixedStepFpsToSetupFile(const FString& setupPath, int32 fps, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Simulator|Launch")
	bool StartSimulationRun(const FString& setupPath, const FString& requestedRunId);

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
	static FString BuildPreviewLauncherArgumentString(
		const FString& previewBatPath,
		const FString& setupPath,
		const FString& runId);

private:
	bool BuildLaunchCommand(const FString& setupPath, const FString& runId, FString& outExecutable, FString& outArguments, bool& bOutUsesPreviewLauncher) const;
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
