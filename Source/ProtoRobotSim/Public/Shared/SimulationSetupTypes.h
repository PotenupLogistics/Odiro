#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "SimulationSetupTypes.generated.h"

// SimulatorMode에서 적용할 fixed-step 설정
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationFixedStepSettings
{
	GENERATED_BODY()

	// 초당 고정 simulation step 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup", meta = (ClampMin = "1"))
	int32 Fps = 60;
};

// Episode evaluation report 출력 설정
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationReportSettings
{
	GENERATED_BODY()

	// EpisodeRunner가 evaluation report JSON을 저장할지 결정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	bool bSaveEvaluationReportJson = true;

	// Evaluation report JSON을 저장할 project-relative 또는 absolute directory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FString OutputDirectory = TEXT("Json/Output");
};

// 별도 simulator process가 launcher UI에 노출할 status 파일 설정
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationStatusSettings
{
	GENERATED_BODY()

	// Run status JSON을 주기적으로 갱신할 project-relative 또는 absolute path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FString OutputPath = TEXT("Saved/SimulationRuns/latest_status.json");
};

// Simulator process 하나의 run session을 정의하는 최상위 setup
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationSetup
{
	GENERATED_BODY()

	// JSON 계약 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FString Schema = TEXT("simulation_setup");

	// JSON 계약 버전
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup", meta = (ClampMin = "1"))
	int32 Version = 1;

	// SimulatorMode에서 로드할 UE level identifier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FString MapId = TEXT("ScenarioSimulationMap");

	// ScenarioSetup, DeliveryBotSetup, PolicySpec 조합 목록을 담은 ScenarioRunQueue JSON path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FString RunQueueJsonPath;

	// Simulator process에만 적용할 fixed-step 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FSimulationFixedStepSettings FixedStep;

	// Measurement JSONL writer에 전달할 logging 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FEpisodeMeasurementLogSettings MeasurementLog;

	// Evaluation report JSON 출력 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FSimulationReportSettings Report;

	// launcher UI가 polling할 status file 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FSimulationStatusSettings Status;
};

// SimulationSetup JSON parse 결과와 diagnostic 묶음
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationSetupParseResult
{
	GENERATED_BODY()

	// 오류 diagnostic이 없으면 true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	bool bSuccess = false;

	// parse와 validation을 통과한 setup 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	FSimulationSetup Setup;

	// setup 파일 또는 field validation 중 발견한 메시지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Setup")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

// launcher와 simulator가 공유하는 public command line 계약
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationCommandLineOptions
{
	GENERATED_BODY()

	// `-Simulate=<SimulationSetupFile>`가 있으면 SimulatorMode로 진입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	bool bSimulate = false;

	// `-Simulate`가 가리키는 SimulationSetup JSON path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	FString SimulationSetupFile;

	// 외부에서 지정한 run id. 비어 있으면 simulator가 생성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	FString RunId;
};

// Command line parse 결과와 diagnostic 묶음
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationCommandLineParseResult
{
	GENERATED_BODY()

	// command line 계약 위반이 없으면 true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	bool bSuccess = false;

	// parse된 simulator 실행 옵션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	FSimulationCommandLineOptions Options;

	// command line validation 중 발견한 메시지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|CommandLine")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

// launcher UI가 polling하는 simulator run 상태
UENUM(BlueprintType)
enum class ESimulationRunState : uint8
{
	Pending,
	Running,
	Completed,
	Failed,
	Canceled
};

// 별도 simulator process의 진행 상태를 파일로 노출하는 payload
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FSimulationRunStatus
{
	GENERATED_BODY()

	// JSON 계약 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString Schema = TEXT("simulation_run_status");

	// JSON 계약 버전
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "1"))
	int32 Version = 1;

	// 한 simulator run session의 stable id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString RunId;

	// 현재 run lifecycle 상태
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	ESimulationRunState State = ESimulationRunState::Pending;

	// 이 run을 시작한 SimulationSetup JSON path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString SetupPath;

	// status 파일을 마지막으로 갱신한 UTC timestamp
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString UpdatedAt;

	// 현재 실행 중인 ScenarioRunQueue pair id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString CurrentPairId;

	// 완료된 run pair 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "0"))
	int32 CompletedRuns = 0;

	// 전체 run pair 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "0"))
	int32 TotalRuns = 0;

	// 완료된 evaluation report JSON path 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	TArray<FString> ReportPaths;

	// 생성된 measurement log JSONL path 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	TArray<FString> LogPaths;

	// 실패 상태의 사람이 읽는 오류 메시지. 비어 있으면 JSON writer가 null로 출력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString Error;
};

// SimulationSetup JSON을 C++ 계약 타입으로 읽는 utility
struct PROTOROBOTSIM_API FSimulationSetupJson
{
	static FSimulationSetupParseResult ParseFromFile(const FString& jsonFilePath);
	static FSimulationSetupParseResult ParseFromString(const FString& jsonString);
	static bool TryWriteSetupJson(
		const FSimulationSetup& setup,
		FString& outJson,
		TArray<FString>& outDiagnostics);
	static bool SaveToFile(
		const FSimulationSetup& setup,
		const FString& setupFilePath,
		TArray<FString>& outDiagnostics);
	static FString ResolveProjectPath(const FString& filePath);
	static FString BuildRunOutputDirectory(const FString& runId);
	static FString BuildRunSetupPath(const FString& runId);
	static void ApplyRunOutputPaths(FSimulationSetup& setup, const FString& runId);
};

// Simulator public command line 계약을 읽는 utility
struct PROTOROBOTSIM_API FSimulationCommandLine
{
	static FSimulationCommandLineParseResult Parse(const FString& commandLine);
	static FSimulationCommandLineParseResult ParseCurrent();
};

// SimulationRunStatus JSON을 launcher UI가 polling할 파일로 쓰는 utility
struct PROTOROBOTSIM_API FSimulationRunStatusJson
{
	static bool TryReadStatusJson(
		const FString& jsonString,
		FSimulationRunStatus& outStatus,
		TArray<FString>& outDiagnostics);

	static bool ParseFromFile(
		const FString& statusFilePath,
		FSimulationRunStatus& outStatus,
		TArray<FString>& outDiagnostics);

	static bool TryWriteStatusJson(
		const FSimulationRunStatus& status,
		FString& outJson,
		TArray<FString>& outDiagnostics);

	static bool SaveToFile(
		const FSimulationRunStatus& status,
		const FString& statusFilePath,
		TArray<FString>& outDiagnostics);
};
