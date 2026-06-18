#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "Shared/EpisodeResultTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/SimulationSetupTypes.h"
#include "UserProjectDataTypes.generated.h"

class FJsonObject;

// user project JSON root 계약 검증 결과.
USTRUCT(BlueprintType)
struct ODIROSIM_API FUserProjectJsonParseResult
{
	GENERATED_BODY()

	// root schema/version과 필수 구조가 유효하면 true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	bool bSuccess = false;

	// root schema field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	FString Schema;

	// root version field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	int32 Version = 0;

	// 검증 중 발견한 메시지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

// episode별 scenario_sample 파일 생성 결과.
USTRUCT(BlueprintType)
struct ODIROSIM_API FUserProjectEpisodeScenarioWriteResult
{
	GENERATED_BODY()

	// 파일 생성과 root 검증이 성공하면 true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	bool bSuccess = false;

	// 생성한 6자리 episode id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	FString EpisodeId;

	// 생성한 scenario_sample JSON path.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	FString ScenarioPath;

	// episode seed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	int64 Seed = 0;

	// 생성된 JSON의 content hash.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	FString ScenarioHash;

	// 생성 중 발견한 메시지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

// episode별 scenario_sample 파일 parse 결과.
USTRUCT(BlueprintType)
struct ODIROSIM_API FUserProjectEpisodeScenarioParseResult
{
	GENERATED_BODY()

	// root schema/version과 필수 section이 유효하면 true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	bool bSuccess = false;

	// episode id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	FString EpisodeId;

	// episode seed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	int64 Seed = 0;

	// parse 중 발견한 메시지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|UserProject")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

// user project 공유 JSON 파일의 최소 root 계약 utility.
struct ODIROSIM_API FUserProjectDataJson
{
	// JSON 문자열의 schema/version root 계약을 검증한다.
	static FUserProjectJsonParseResult ValidateRootJsonString(
		const FString& jsonString,
		const FString& expectedSchema);

	// JSON 파일의 schema/version root 계약을 검증한다.
	static FUserProjectJsonParseResult ValidateRootJsonFile(
		const FString& jsonFilePath,
		const FString& expectedSchema);

	// root 계약을 통과한 JSON 문자열을 파일로 저장한다.
	static bool SaveRootJsonFile(
		const FString& jsonFilePath,
		const FString& jsonString,
		const FString& expectedSchema,
		TArray<FScenarioCompileDiagnostic>& outDiagnostics);
};

// run snapshot에서 episode별 scenario_sample JSON을 생성하고 읽는 utility.
struct ODIROSIM_API FUserProjectEpisodeScenarioJson
{
	// 0-based episode index를 1-based 6자리 episode id로 변환한다.
	static FString BuildEpisodeId(int32 episodeIndex);

	// episode id가 6자리 decimal string이면 true.
	static bool IsValidEpisodeId(const FString& episodeId);

	// snapshot/scenario.json과 setting seed로 scenario_sample 하나를 생성한다.
	static FUserProjectEpisodeScenarioWriteResult WriteEpisodeScenario(
		const FUserProjectRunSnapshotPaths& paths,
		const FUserProjectRunSetting& setting,
		int32 episodeIndex);

	// setting.episode_count만큼 scenario_sample을 생성한다.
	static bool WriteAllEpisodeScenarios(
		const FUserProjectRunSnapshotPaths& paths,
		const FUserProjectRunSetting& setting,
		TArray<FUserProjectEpisodeScenarioWriteResult>& outResults,
		TArray<FScenarioCompileDiagnostic>& outDiagnostics);

	// scenario_sample JSON 문자열의 root 계약을 읽는다.
	static FUserProjectEpisodeScenarioParseResult ParseFromString(const FString& jsonString);

	// scenario_sample JSON 파일의 root 계약을 읽는다.
	static FUserProjectEpisodeScenarioParseResult ParseFromFile(const FString& jsonFilePath);
};

// user project run 결과 파일 writer.
struct ODIROSIM_API FUserProjectRunOutputJson
{
	// `<RunPath>/episodes/<EpisodeId>` directory path를 만든다.
	static FString BuildEpisodeDirectory(const FUserProjectRunSnapshotPaths& paths, const FString& episodeId);

	// 완료된 episode record에서 result/events/actions/trace 파일을 쓴다.
	static bool SaveEpisodeArtifacts(
		const FUserProjectRunSnapshotPaths& paths,
		const FEpisodeRunRecord& runRecord,
		TArray<FString>& outDiagnostics);

	// 완료된 episode record 목록에서 summary.json을 쓴다.
	static bool SaveRunSummary(
		const FUserProjectRunSnapshotPaths& paths,
		const TArray<FEpisodeRunRecord>& runRecords,
		TArray<FString>& outDiagnostics);

	// Python policy decide request/response에서 actions.jsonl line을 추가한다.
	static bool AppendRobotActionRecord(
		const FUserProjectRunSnapshotPaths& paths,
		const FString& episodeId,
		const TSharedRef<FJsonObject>& requestObject,
		const TSharedPtr<FJsonObject>& responseObject,
		bool bActionSucceeded,
		int32 httpStatusCode,
		const FString& errorMessage,
		TArray<FString>& outDiagnostics);

	// runtime tick capture에서 trace.jsonl line을 추가한다.
	static bool AppendEpisodeTraceRecord(
		const FUserProjectRunSnapshotPaths& paths,
		const FString& episodeId,
		const FEpisodeMeasurementLogTickRecord& tickRecord,
		int32 sampleIndex,
		TArray<FString>& outDiagnostics);

	// 지정한 trace.jsonl path에 runtime trace line을 추가한다.
	static bool AppendEpisodeTraceRecordToFile(
		const FString& traceJsonlPath,
		const FEpisodeMeasurementLogTickRecord& tickRecord,
		int32 sampleIndex,
		TArray<FString>& outDiagnostics);
};
