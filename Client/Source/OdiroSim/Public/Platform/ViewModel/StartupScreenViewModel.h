#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "StartupScreenViewModel.generated.h"

class UGameInstance;
class UProjectSessionSubsystem;
class UScenarioEditorLaunchSubsystem;
class USimulatorLaunchSubsystem;

// StartupScreen이 표시하는 최근 project 카드의 값 객체.
USTRUCT(BlueprintType)
struct ODIROSIM_API FStartupScreenRecentProjectItem
{
	GENERATED_BODY()

	// Project root 절대 경로.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	FString ProjectPath;

	// 카드 주 표시 이름.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	FString Title;

	// 카드 보조 표시 이름.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	FString Subtitle;

	// Project root 아래 preview.png 절대 경로.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	FString PreviewImagePath;

	// 카드가 현재 선택된 project인지 여부.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	bool bSelected = false;

	// 카드가 열 수 있는 project를 가리키는지 여부.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|StartupScreen")
	bool bEnabled = true;
};

// StartupScreen 오류 상황별 진단 문구.
USTRUCT(BlueprintType)
struct ODIROSIM_API FStartupScreenDiagnosticMessages
{
	GENERATED_BODY()

	// Project 선택 없이 열기를 요청했을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString ProjectRequired;

	// 선택한 project folder가 존재하지 않을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString ProjectFolderMissing;

	// Project 검증용 subsystem을 사용할 수 없을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString SimulatorLaunchUnavailable;

	// Project 검증 실패 원인이 비어 있을 때 표시할 fallback 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString ProjectValidationFailed;

	// Project session 또는 scenario editor launch subsystem을 사용할 수 없을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString ProjectOpenSubsystemUnavailable;

	// Scenario editor map 열기에 실패했을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics")
	FString ScenarioEditorOpenFailed;
};

// Master Widget 안에 들어가는 StartupScreen content panel의 상태와 command를 소유한다.
UCLASS(BlueprintType)
class ODIROSIM_API UStartupScreenViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 Subsystem 참조를 연결하고 최근 project 상태를 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 Subsystem을 주입할 때 사용한다.
	void SetSubsystemOverrides(
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
		UProjectSessionSubsystem* projectSessionSubsystem,
		UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem);

	// 저장된 최근 project 목록을 다시 읽고 표시 item을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void RefreshRecentProjects();

	// 표시 목록에서 project path를 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void SelectProject(const FString& projectPath);

	// project root가 열 수 있는 user project인지 검증한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool ValidateProject(const FString& projectPath, TArray<FString>& outDiagnostics);

	// 검증된 project root를 최근 project 목록에 추가한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool AddRecentProjectIfValid(const FString& projectPath, TArray<FString>& outDiagnostics);

	// project root를 검증하고 ScenarioEditorMap을 연다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool OpenProject(const FString& projectPath);

	// 최근 project 목록에서 경로를 제거한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool RemoveRecentProject(const FString& projectPath);

	// 최근 project item 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	TArray<FStartupScreenRecentProjectItem> GetRecentProjects() const { return RecentProjects; }

	// 최근 project path 목록을 newest-first로 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	TArray<FString> GetRecentProjectPaths() const;

	// 현재 선택된 project root 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	FString GetSelectedProjectPath() const { return SelectedProjectPath; }

	// UI에 표시할 진단/검증 메시지를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	FString GetDiagnosticsText() const { return DiagnosticsText; }

	// UI에 표시할 진단/검증 메시지를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void SetDiagnosticsText(const FString& message);

	// StartupScreen WBP가 지정한 오류 상황별 진단 문구를 적용한다.
	void SetDiagnosticMessages(const FStartupScreenDiagnosticMessages& messages);

	// 진단 메시지를 비운다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void ClearDiagnostics();

	// command가 비동기 작업 중인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	bool IsBusy() const { return bBusy; }

	// command 진행 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void SetBusy(bool bInBusy);

private:
	USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	UScenarioEditorLaunchSubsystem* ResolveScenarioEditorLaunchSubsystem() const;
	void LoadRecentProjectPaths();
	void SaveRecentProjectPaths() const;
	void RememberRecentProject(const FString& projectPath);
	bool PruneMissingRecentProjects();
	void RebuildRecentProjectItems();

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

	// 최근 project 카드 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|StartupScreen", meta = (AllowPrivateAccess = "true"))
	TArray<FStartupScreenRecentProjectItem> RecentProjects;

	// 저장된 최근 project 절대 경로 목록.
	UPROPERTY(Transient)
	TArray<FString> RecentProjectPaths;

	// 현재 선택된 project root 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|StartupScreen", meta = (AllowPrivateAccess = "true"))
	FString SelectedProjectPath;

	// View가 그대로 표시할 진단/검증 메시지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|StartupScreen", meta = (AllowPrivateAccess = "true"))
	FString DiagnosticsText;

	// 오류 상황별 기본 진단 문구.
	UPROPERTY(Transient)
	FStartupScreenDiagnosticMessages DiagnosticMessages;

	// command control의 busy/disabled 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|StartupScreen", meta = (AllowPrivateAccess = "true"))
	bool bBusy = false;
};
