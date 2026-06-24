#pragma once

#include "CoreMinimal.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "ProjectWorkspaceViewModel.generated.h"

class UGameInstance;
class UOdiroListItemViewModel;
class UPlatformAnalysisAiSubsystem;
class UPlatformUiSubsystem;
class UProjectSessionSubsystem;
class UScenarioEditorLaunchSubsystem;

// ScenarioEditorMap project workspace의 tab, run 선택, command 상태를 소유하는 ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UProjectWorkspaceViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 Subsystem 참조를 연결하고 active project 상태를 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 Subsystem을 주입할 때 사용한다.
	void SetSubsystemOverrides(
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
		UProjectSessionSubsystem* projectSessionSubsystem,
		UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem,
		UPlatformAnalysisAiSubsystem* analysisAiSubsystem);

	// ProjectSessionSubsystem에서 active project 상태를 다시 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	void RefreshFromProjectSession();

	// active project의 run directory 목록을 다시 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	void RefreshProjectRuns();

	// workspace logical tab을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	void SelectWorkspaceTab(FName tabId);

	// 선택된 run id와 directory를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool SelectRun(const FString& runId);

	// 현재 project 입력을 새 run snapshot으로 고정하고 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool CreateRun(FString& outRunId);

	// 선택 또는 새 run snapshot으로 simulator project-run을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool StartRun();

	// 새 run snapshot을 만든 뒤 해당 run으로 simulator project-run을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool StartNewRun(FString& outRunId);

	// 현재 simulator run을 중지한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	void StopRun();

	// 선택된 run에 대한 AI 분석을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool RequestAiAnalysis();

	// active project scenario를 Scenario Editor로 연다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool OpenScenarioEditor();

	// active project를 비우고 StartupMap으로 돌아간다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Workspace")
	bool ReturnToStartup();

	// active project root 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FString GetActiveProjectPath() const { return ActiveProjectPath; }

	// active project scenario.json 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FString GetActiveScenarioPath() const { return ActiveScenarioPath; }

	// 현재 선택된 workspace tab id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FName GetSelectedWorkspaceTabId() const { return SelectedWorkspaceTabId; }

	// 현재 선택된 run id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FString GetSelectedRunId() const { return SelectedRunId; }

	// 현재 선택된 run directory 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FString GetSelectedRunDirectory() const { return SelectedRunDirectory; }

	// workspace status 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	FString GetStatusText() const { return StatusText; }

	// run row/card ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Workspace")
	TArray<UOdiroListItemViewModel*> GetRunItems() const;

private:
	USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	UScenarioEditorLaunchSubsystem* ResolveScenarioEditorLaunchSubsystem() const;
	UPlatformUiSubsystem* ResolvePlatformUiSubsystem() const;
	UPlatformAnalysisAiSubsystem* ResolvePlatformAnalysisAiSubsystem() const;
	void HandleRunInfoChanged(const FSimulatorRunInfo& runInfo);
	void SetActiveProjectPath(const FString& projectPath);
	void SetActiveScenarioPath(const FString& scenarioPath);
	void SetSelectedRunState(const FString& runId, const FString& runDirectory);
	void SetStatusText(const FString& statusText);
	FString BuildRunDirectory(const FString& runId) const;
	FString ExtractRunId(const FString& runDirectory) const;

	// Subsystem lookup에 사용할 GameInstance.
	UPROPERTY(Transient)
	TObjectPtr<UGameInstance> GameInstance;

	// 테스트/host가 명시 주입한 simulator launch subsystem.
	UPROPERTY(Transient)
	TObjectPtr<USimulatorLaunchSubsystem> SimulatorLaunchOverride;

	// 테스트/host가 명시 주입한 project session subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectSessionSubsystem> ProjectSessionOverride;

	// 테스트/host가 명시 주입한 scenario editor launch subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorLaunchSubsystem> ScenarioEditorLaunchOverride;

	// 테스트/host가 명시 주입한 analysis AI subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UPlatformAnalysisAiSubsystem> AnalysisAiOverride;

	// active user project root 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FString ActiveProjectPath;

	// active user project scenario.json 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FString ActiveScenarioPath;

	// 현재 선택된 workspace tab id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FName SelectedWorkspaceTabId = FName(TEXT("ScenarioEdit"));

	// 현재 선택된 project run id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FString SelectedRunId;

	// 현재 선택된 project run directory 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FString SelectedRunDirectory;

	// workspace status 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	FString StatusText;

	// project run row/card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Workspace", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOdiroListItemViewModel>> RunItems;
};
