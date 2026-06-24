#pragma once

#include "CoreMinimal.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "StartupMenuViewModel.generated.h"

class UGameInstance;
class UOdiroListItemViewModel;
class UProjectSessionSubsystem;
class UScenarioEditorLaunchSubsystem;

// StartupMap project 선택/생성 화면의 상태와 command를 소유하는 ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UStartupMenuViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 Subsystem 참조를 연결하고 최근 project/preset 상태를 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 Subsystem을 주입할 때 사용한다.
	void SetSubsystemOverrides(
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
		UProjectSessionSubsystem* projectSessionSubsystem,
		UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem);

	// 현재 project 입력을 절대 경로로 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void SetProjectPathForPrototype(const FString& projectPath);

	// 현재 project 입력의 절대 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FString GetProjectPathForPrototype() const;

	// 새 project parent 폴더 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void SetProjectParentFolder(const FString& parentFolder);

	// 새 project parent 폴더 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FString GetProjectParentFolder() const { return ProjectParentFolder; }

	// 새 project 이름 입력을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void SetProjectName(const FString& projectName);

	// 새 project 이름 입력을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FString GetProjectName() const { return ProjectName; }

	// 새 project에 적용할 preset id를 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void SelectProjectPresets(
		const FString& scenarioPresetId,
		const FString& profilePresetId,
		const FString& policyPresetId);

	// 저장된 project open 입력 상태를 user settings에서 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void LoadProjectOpenOptions();

	// 현재 project open 입력 상태를 user settings에 저장한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void SaveProjectOpenOptions() const;

	// parent/name/preset 입력으로 새 project를 만든다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	bool CreateProject(
		const FString& parentFolder,
		const FString& projectName,
		const FProjectPresetSelection& presets);

	// 현재 입력된 project root를 검증하고 editor를 연다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	bool OpenProject(const FString& projectPath);

	// project root가 열 수 있는 user project인지 검증한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	bool ValidateProject(const FString& projectPath, TArray<FString>& outDiagnostics);

	// 검증된 project root를 최근 project 목록에 추가한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	bool AddRecentProjectIfValid(const FString& projectPath, TArray<FString>& outDiagnostics);

	// 최근 project 목록에서 경로를 제거한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	bool RemoveRecentProject(const FString& projectPath);

	// 최근 project 목록을 강제로 다시 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void RefreshRecentProjects();

	// 사용 가능한 preset 목록을 강제로 다시 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Startup")
	void RefreshProjectPresets();

	// 최근 project item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	TArray<UOdiroListItemViewModel*> GetRecentProjectItems() const;

	// scenario preset item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	TArray<UOdiroListItemViewModel*> GetScenarioPresetItems() const;

	// profile preset item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	TArray<UOdiroListItemViewModel*> GetProfilePresetItems() const;

	// policy preset item ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	TArray<UOdiroListItemViewModel*> GetPolicyPresetItems() const;

	// 최신 project preset catalog cache를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FProjectPresetCatalog GetProjectPresetCatalog() const { return PresetCatalog; }

	// 현재 선택된 project preset id 묶음을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FProjectPresetSelection GetSelectedProjectPresetSelection() const;

	// 최근 project path 목록을 newest-first로 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	TArray<FString> GetRecentProjectPaths() const { return RecentProjectPaths; }

	// 표시용 project open warning 문구를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	FString GetProjectOpenWarningText() const { return ProjectOpenWarningText; }

	// create/open action 가능 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	bool CanCreateProject() const { return bCanCreateProject; }

	// open action 가능 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Startup")
	bool CanOpenProject() const { return bCanOpenProject; }

private:
	USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	UScenarioEditorLaunchSubsystem* ResolveScenarioEditorLaunchSubsystem() const;
	void LoadRecentProjectPaths();
	void SaveRecentProjectPaths() const;
	void RememberRecentProject(const FString& projectPath);
	bool PruneMissingRecentProjects();
	void RebuildRecentProjectItems();
	void RebuildPresetItems();
	void RefreshActionState();
	void SetProjectOpenWarningText(const FString& message);
	FProjectPresetSelection GetSelectedPresetSelection() const;

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

	// 새 project parent 폴더 입력.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString ProjectParentFolder;

	// 새 project 폴더 이름 입력.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString ProjectName;

	// 현재 입력으로 계산한 project root 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString ProjectPath;

	// 선택된 scenario preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString SelectedScenarioPresetId = TEXT("blank");

	// 선택된 profile preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString SelectedProfilePresetId = TEXT("basic");

	// 선택된 policy preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString SelectedPolicyPresetId = TEXT("blank");

	// create/open warning 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	FString ProjectOpenWarningText;

	// create project command 활성 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	bool bCanCreateProject = false;

	// open project command 활성 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	bool bCanOpenProject = false;

	// 최근 project 절대 경로 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	TArray<FString> RecentProjectPaths;

	// 최근 project row/card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOdiroListItemViewModel>> RecentProjectItems;

	// scenario preset row/card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOdiroListItemViewModel>> ScenarioPresetItems;

	// profile preset row/card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOdiroListItemViewModel>> ProfilePresetItems;

	// policy preset row/card ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|Startup", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOdiroListItemViewModel>> PolicyPresetItems;

	// 최신 preset catalog cache.
	UPROPERTY(Transient)
	FProjectPresetCatalog PresetCatalog;
};
