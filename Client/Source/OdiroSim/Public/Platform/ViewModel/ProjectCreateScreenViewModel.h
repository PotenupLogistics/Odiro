#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "ProjectCreateScreenViewModel.generated.h"

class UGameInstance;
class UProjectSessionSubsystem;

// Project Create 화면의 preset section category.
UENUM(BlueprintType)
enum class EProjectCreatePresetCategory : uint8
{
	Scenario,
	Profile,
	Policy
};

// Project Create 화면에서 카드 하나가 표시할 preset 값 객체.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectCreatePresetItem
{
	GENERATED_BODY()

	// 카드가 속한 preset category.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	EProjectCreatePresetCategory Category = EProjectCreatePresetCategory::Scenario;

	// 선택 command에 쓰는 stable preset id.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	FString PresetId;

	// 카드 주 표시 이름.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	FString Title;

	// 카드 보조 설명.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	FString Subtitle;

	// 카드 media에 사용할 thumbnail.png 절대 경로.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	FString ThumbnailPath;

	// 카드가 현재 선택된 preset인지 여부.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|ProjectCreate")
	bool bSelected = false;
};

// ProjectCreateScreen 오류 상황별 진단 문구.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectCreateScreenDiagnosticMessages
{
	GENERATED_BODY()

	// Project path form이 생성 가능한 상태가 아닐 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString ProjectPathInvalid;

	// Project 생성에 필요한 subsystem을 사용할 수 없을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString SubsystemUnavailable;

	// Project 생성 실패 원인이 비어 있을 때 표시할 fallback 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString ProjectCreateFailed;

	// 생성된 project 검증 실패 원인이 비어 있을 때 표시할 fallback 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString CreatedProjectValidationFailed;

	// Project 생성 성공 시 표시할 문구 template; {ProjectPath} 또는 {0} placeholder를 지원한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString ProjectCreatedFormat;

	// Parent folder picker를 취소했을 때 표시할 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics")
	FString ParentFolderSelectionCanceled;
};

// ProjectCreateScreen WBP가 소유하는 create form 기본값.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectCreateScreenDefaultValues
{
	GENERATED_BODY()

	// 저장된 config가 없을 때 사용할 project parent folder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Defaults")
	FString ProjectParentFolder;

	// 저장된 config가 없을 때 사용할 project folder name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Defaults")
	FString ProjectName;
};

// Master Widget 안에 들어가는 Project Create content panel의 상태와 command를 소유한다.
UCLASS(BlueprintType)
class ODIROSIM_API UProjectCreateScreenViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// GameInstance 기반 Subsystem 참조를 연결하고 create form state를 초기화한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// 자동화 테스트나 host가 명시 Subsystem을 주입할 때 사용한다.
	void SetSubsystemOverrides(
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
		UProjectSessionSubsystem* projectSessionSubsystem);

	// 저장된 create form state와 preset catalog를 다시 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void Refresh();

	// Project name input 값을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetProjectName(const FString& projectName);

	// Project parent folder input 값을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetProjectParentFolder(const FString& projectParentFolder);

	// 지정 category의 preset 선택을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SelectPreset(EProjectCreatePresetCategory category, const FString& presetId);

	// 현재 form state로 사용자 project를 생성하고 active project로 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	bool CreateProject(TArray<FString>& outDiagnostics);

	// 현재 선택된 preset id 묶음을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FProjectPresetSelection GetSelectedPresetSelection() const;

	// Scenario preset card item 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	TArray<FProjectCreatePresetItem> GetScenarioPresetItems() const { return ScenarioPresetItems; }

	// Profile preset card item 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	TArray<FProjectCreatePresetItem> GetProfilePresetItems() const { return ProfilePresetItems; }

	// Policy preset card item 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	TArray<FProjectCreatePresetItem> GetPolicyPresetItems() const { return PolicyPresetItems; }

	// Project parent folder absolute path를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetProjectParentFolder() const { return ProjectParentFolder; }

	// Project folder name을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetProjectName() const { return ProjectName; }

	// 생성될 project root absolute path를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetProjectPath() const { return ProjectPath; }

	// UI에 표시할 scenario summary path를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetSelectedScenarioSummary() const;

	// UI에 표시할 profile summary path를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetSelectedProfileSummary() const;

	// UI에 표시할 policy summary path를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetSelectedPolicySummary() const;

	// UI에 표시할 진단/검증 메시지를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	FString GetDiagnosticsText() const { return DiagnosticsText; }

	// UI에 표시할 진단/검증 메시지를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetDiagnosticsText(const FString& message);

	// ProjectCreateScreen WBP가 지정한 오류 상황별 진단 문구를 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetDiagnosticMessages(const FProjectCreateScreenDiagnosticMessages& messages);

	// ProjectCreateScreen WBP가 지정한 create form 기본값을 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetDefaultValues(const FProjectCreateScreenDefaultValues& values);

	// 진단 메시지를 비운다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void ClearDiagnostics();

	// Project 생성 command가 가능한지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	bool CanCreateProject() const { return bCanCreateProject; }

	// command가 비동기 작업 중인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	bool IsBusy() const { return bBusy; }

private:
	USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	void LoadProjectCreateOptions();
	void SaveProjectCreateOptions() const;
	void RememberRecentProject(const FString& projectPath);
	void RefreshProjectPresets();
	void RebuildPresetItems();
	void RefreshProjectPath();
	void RefreshActionState();
	void SetBusy(bool bInBusy);

	// Subsystem lookup에 사용할 GameInstance.
	UPROPERTY(Transient)
	TObjectPtr<UGameInstance> GameInstance;

	// 테스트/host가 명시 주입한 simulator launch subsystem.
	UPROPERTY(Transient)
	TObjectPtr<USimulatorLaunchSubsystem> SimulatorLaunchOverride;

	// 테스트/host가 명시 주입한 project session subsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectSessionSubsystem> ProjectSessionOverride;

	// Project name input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString ProjectName;

	// Project parent folder input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString ProjectParentFolder;

	// Project create target path derived from parent folder and name.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString ProjectPath;

	// Selected scenario preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString SelectedScenarioPresetId;

	// Selected profile preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString SelectedProfilePresetId;

	// Selected policy preset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString SelectedPolicyPresetId;

	// Scenario preset card values.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	TArray<FProjectCreatePresetItem> ScenarioPresetItems;

	// Profile preset card values.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	TArray<FProjectCreatePresetItem> ProfilePresetItems;

	// Policy preset card values.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	TArray<FProjectCreatePresetItem> PolicyPresetItems;

	// Latest catalog read from SimulatorLaunchSubsystem.
	UPROPERTY(Transient)
	FProjectPresetCatalog PresetCatalog;

	// View가 그대로 표시할 진단/검증 메시지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	FString DiagnosticsText;

	// 오류 상황별 기본 진단 문구.
	UPROPERTY(Transient)
	FProjectCreateScreenDiagnosticMessages DiagnosticMessages;

	// 저장된 config가 없을 때 사용할 form 기본값.
	UPROPERTY(Transient)
	FProjectCreateScreenDefaultValues DefaultValues;

	// Project 생성 command enabled state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	bool bCanCreateProject = false;

	// command control의 busy/disabled 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	bool bBusy = false;
};
