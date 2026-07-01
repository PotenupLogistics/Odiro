#pragma once

#include "CoreMinimal.h"
#include "Platform/ExperimentConfigSettings.h"
#include "Platform/RobotProfileSettings.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Shared/SimulationSetupTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlatformUiSubsystem.generated.h"

class UExperimentConfigViewModel;
class UExperimentResultViewModel;
class UOdiroListItemViewModel;
class UPlatformAnalysisAiSubsystem;
class UProjectSessionSubsystem;
class UProjectWorkspaceViewModel;
class URobotProfileViewModel;
struct FPlatformAnalysisAiResponse;
struct FProjectRunResultDashboardData;

DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformUiRunInfoChangedNative, const FSimulatorRunInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformUiAnalysisCompletedNative, const FPlatformAnalysisAiResponse&);

// Legacy report 목록 표시용 경량 item; 파일 파싱은 PlatformUiSubsystem이 소유한다.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentResultReportItem
{
	GENERATED_BODY()

	// Report JSON 파일 경로.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|UI")
	FString ReportPath;

	// Report가 나타내는 run index.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|UI")
	int32 RunIndex = INDEX_NONE;
};

// Platform UI의 ViewModel lifecycle/factory를 소유하는 GameInstance subsystem.
UCLASS(BlueprintType)
class ODIROSIM_API UPlatformUiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance lifetime에 맞춰 ViewModel 인스턴스를 생성한다.
	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	// GameInstance 종료 시 ViewModel 참조를 정리한다.
	virtual void Deinitialize() override;

	// World context에서 Platform UI subsystem을 반환한다.
	static UPlatformUiSubsystem* ResolveForWorldContext(const UObject* worldContextObject);

	// Simulator run 변경을 Platform UI adapter에 중계한다.
	FPlatformUiRunInfoChangedNative OnRunInfoChanged;

	// AI 분석 완료를 Platform UI adapter에 중계한다.
	FPlatformUiAnalysisCompletedNative OnAnalysisCompleted;

	// Project workspace ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|UI")
	UProjectWorkspaceViewModel* GetProjectWorkspaceViewModel() const { return ProjectWorkspaceViewModel; }

	// Experiment config ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|UI")
	UExperimentConfigViewModel* GetExperimentConfigViewModel() const { return ExperimentConfigViewModel; }

	// Returns the Robot profile ViewModel.
	UFUNCTION(BlueprintPure, Category = "Platform|UI")
	URobotProfileViewModel* GetRobotProfileViewModel() const { return RobotProfileViewModel; }

	// Experiment result ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|UI")
	UExperimentResultViewModel* GetExperimentResultViewModel() const { return ExperimentResultViewModel; }

	// ProjectSession 기반 workspace/config 상태를 다시 동기화한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|UI")
	void RefreshFromProjectSession();

	// 개발/배포 resource에 포함된 project preset id 목록을 반환한다.
	FProjectPresetCatalog ListProjectPresets() const;

	// 사용자 project root의 최소 계약을 검증한다.
	bool ValidateUserProject(const FString& projectPath, TArray<FString>& outDiagnostics) const;

	// Active user project가 있는지 반환한다.
	bool HasActiveProject() const;

	// Active user project root 경로를 반환한다.
	FString GetActiveProjectPath() const;

	// Active user project scenario.json 경로를 반환한다.
	FString GetActiveProjectScenarioPath() const;

	// Active project를 비우고 ScenarioEditorMap의 startup screen으로 전환한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|UI")
	bool ReturnToStartupMap(FString& outErrorText) const;

	// Legacy simulation setup launch command를 subsystem으로 위임한다.
	bool StartLegacySimulationRun(
		const FString& setupPath,
		const FString& requestedRunId,
		FString& outErrorText) const;

	// Scenario editor launch command를 subsystem으로 위임한다.
	bool OpenScenarioEditorPath(const FString& scenarioPath, FString& outErrorText) const;

	// Project run directory 아래 episode result 파일 목록을 반환한다.
	TArray<FString> ListProjectEpisodeResultFiles(const FString& runDirectory) const;

	// Project run directory의 summary/review JSON을 dashboard 데이터로 읽는다.
	static bool LoadProjectRunDashboard(
		const FString& runDirectory,
		FProjectRunResultDashboardData& outDashboardData);

	// Project run AI 분석 요청을 model subsystem으로 위임한다.
	bool RequestProjectRunAnalysis(
		const FString& projectPath,
		const FString& runId,
		FString& outErrorText) const;

	// Legacy SimulationSetup 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacySimulationSetupFiles() const;

	// Legacy ScenarioSetup 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacyScenarioSetupFiles() const;

	// Legacy DeliveryBotSetup 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacyDeliveryBotSetupFiles() const;

	// Legacy PolicySpec 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacyPolicySpecFiles() const;

	// Legacy result directory 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacySimulationRunResultDirectories() const;

	// Legacy run status 파일 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacySimulationRunStatusFiles() const;

	// Legacy run directory의 report 파일 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacyEvaluationReportFilesInDirectory(const FString& runDirectory) const;

	// Legacy run directory의 measurement log 목록 조회를 UI subsystem 경계로 격리한다.
	TArray<FString> ListLegacyMeasurementLogFilesInDirectory(const FString& runDirectory) const;

	// 경로 목록을 반복 UI가 소비할 공통 item ViewModel 목록으로 변환한다.
	TArray<UOdiroListItemViewModel*> CreatePathItemViewModels(
		const TArray<FString>& itemPaths,
		bool bUseBaseFilenameAsTitle);

	// Legacy SimulationSetup 파일을 파싱한다.
	FSimulationSetupParseResult LoadLegacySimulationSetupFile(const FString& setupPath) const;

	// Legacy SimulationSetup fixed FPS 값을 저장한다.
	bool SaveLegacyFixedStepFpsToSetupFile(
		const FString& setupPath,
		int32 fps,
		TArray<FString>& outDiagnostics) const;

	// Legacy SimulationSetup 파일을 저장한다.
	bool SaveLegacySimulationSetupFile(
		const FString& setupPath,
		const FSimulationSetup& setup,
		TArray<FString>& outDiagnostics) const;

	// Legacy ScenarioRunQueue 파일을 읽는다.
	bool LoadLegacyScenarioRunQueueFile(
		const FString& runQueuePath,
		TArray<FScenarioRunInput>& outRunInputs,
		TArray<FString>& outDiagnostics) const;

	// Legacy ScenarioRunQueue 파일을 저장한다.
	bool SaveLegacyScenarioRunQueueFile(
		const FString& runQueuePath,
		const TArray<FScenarioRunInput>& runInputs,
		TArray<FString>& outDiagnostics) const;

	// user project setting.json에서 experiment 설정 subset을 읽는다.
	static bool LoadExperimentSettingsForProject(
		const FString& projectPath,
		FExperimentConfigSettings& outSettings,
		FString& outErrorText);

	// user project setting.json에 experiment 설정 subset을 저장한다.
	static bool SaveExperimentSettingsForProject(
		const FString& projectPath,
		const FExperimentConfigSettings& settings,
		FString& outStatusText);

	// Reads the robot.body subset from user-project profile.json.
	static bool LoadRobotProfileBodyForProject(
		const FString& projectPath,
		FRobotProfileBodySettings& outSettings,
		FString& outErrorText);

	// Writes the robot.body subset to user-project profile.json.
	static bool SaveRobotProfileBodyForProject(
		const FString& projectPath,
		const FRobotProfileBodySettings& settings,
		FString& outStatusText);

	// Reads the exposed robot profile subset from user-project profile.json.
	static bool LoadRobotProfileForProject(
		const FString& projectPath,
		FRobotProfileSettings& outSettings,
		FString& outErrorText);

	// Writes the exposed robot profile subset to user-project profile.json.
	static bool SaveRobotProfileForProject(
		const FString& projectPath,
		const FRobotProfileSettings& settings,
		FString& outStatusText);

	// Legacy RunQueue 내부 ScenarioSetup 참조를 교체한다.
	bool ReplaceLegacyScenarioSetupReferencesInRunQueues(
		const FString& oldScenarioSetupPath,
		const FString& newScenarioSetupPath,
		TArray<FString>& outDiagnostics) const;

	// Legacy RunQueue 내부 DeliveryBotSetup 참조를 교체한다.
	bool ReplaceLegacyDeliveryBotSetupReferencesInRunQueues(
		const FString& oldDeliveryBotSetupPath,
		const FString& newDeliveryBotSetupPath,
		TArray<FString>& outDiagnostics) const;

	// Active simulator run status를 최신 상태로 갱신한다.
	bool RefreshActiveRunStatus() const;

	// Active simulator run 상태 snapshot을 반환한다.
	FSimulatorRunInfo GetActiveRunInfo() const;

	// Platform AI 분석 요청이 진행 중인지 반환한다.
	bool IsAnalysisRequestPending() const;

	// Bridge status.json의 fallback state 값을 읽는다.
	static bool TryReadBridgeRunStatusState(const FString& statusPath, ESimulationRunState& outState);

	// Measurement log 파일을 UI preview 문자열로 읽는다.
	static FString BuildLogPreview(const FString& logPath, int32 edgeLineCount);

	// Experiment report JSON 하나를 목록 item으로 파싱한다.
	static bool TryReadExperimentResultReportItem(const FString& reportPath, FExperimentResultReportItem& outItem);

	// Experiment report JSON 목록을 run index 순서로 정렬한다.
	static TArray<FExperimentResultReportItem> BuildExperimentResultReportItems(const TArray<FString>& reportPaths);

	// Template 파일 내용을 output 파일로 복사하고 필요한 directory를 만든다.
	static bool CreateTextFileFromTemplate(
		const FString& templatePath,
		const FString& outputPath,
		FString& outErrorText);

	// Project-relative 파일을 목적 경로로 이동한다.
	static bool MoveProjectRelativeFile(
		const FString& sourcePath,
		const FString& targetPath,
		const FString& itemLabel,
		FString& outErrorText);

	// Json/Input 아래에 충돌하지 않는 project-relative JSON 경로를 만든다.
	static FString MakeUniqueInputJsonPath(const FString& baseFileName);

	// Project-relative 또는 absolute 경로의 파일 존재 여부를 반환한다.
	static bool DoesResolvedFileExist(const FString& path);

	// Project-relative 또는 absolute 경로의 directory 존재 여부를 반환한다.
	static bool DoesResolvedDirectoryExist(const FString& path);

	// Project-relative 또는 absolute text 파일을 읽는다.
	static bool ReadResolvedTextFile(const FString& path, FString& outText);

private:
	USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	UPlatformAnalysisAiSubsystem* ResolvePlatformAnalysisAiSubsystem() const;
	void HandleRunInfoChanged(const FSimulatorRunInfo& runInfo);
	void HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response);

	// Project workspace에서 startup screen을 표시할 root shell map id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|UI", meta = (AllowPrivateAccess = "true"))
	FString StartupMapId = TEXT("ScenarioEditorMap");

	// ScenarioEditorMap project workspace 상태.
	UPROPERTY(Transient)
	TObjectPtr<UProjectWorkspaceViewModel> ProjectWorkspaceViewModel;

	// user project setting.json 편집 상태.
	UPROPERTY(Transient)
	TObjectPtr<UExperimentConfigViewModel> ExperimentConfigViewModel;

	// user project profile.json robot profile editing state.
	UPROPERTY(Transient)
	TObjectPtr<URobotProfileViewModel> RobotProfileViewModel;

	// project run result dashboard 상태.
	UPROPERTY(Transient)
	TObjectPtr<UExperimentResultViewModel> ExperimentResultViewModel;
};
