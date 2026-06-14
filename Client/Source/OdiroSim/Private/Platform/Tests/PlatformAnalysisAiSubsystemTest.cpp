#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/PlatformAnalysisAiSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString MakeAnalysisAiTestDirectory()
	{
		const FString Directory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("PlatformAnalysisAi"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Directory;
	}

	bool SaveAnalysisAiTestFile(const FString& FilePath, const FString& Contents)
	{
		return FFileHelper::SaveStringToFile(
			Contents,
			*FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiReportPathExtractionTest,
	"OdiroSim.Platform.AnalysisAi.ReportPathExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiReportPathExtractionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ReportJson = TEXT(R"({
		"schema": "episode_evaluation_report",
		"version": 1,
		"run": {
			"episode_setup": { "path": "Json/Input/ScenarioSetupSample_1.json", "hash": "episode-hash" },
			"delivery_bot_setup": { "path": "Json/Input/DeliveryBotSetupSample_1.json", "hash": "bot-hash" }
		}
	})");

	FString EpisodeSetupPath;
	FString DeliveryBotSetupPath;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("setup paths extract"),
		UPlatformAnalysisAiSubsystem::ExtractSetupPathsFromReportJson(
			ReportJson,
			EpisodeSetupPath,
			DeliveryBotSetupPath,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);
	TestEqual(TEXT("episode setup path"), EpisodeSetupPath, FString(TEXT("Json/Input/ScenarioSetupSample_1.json")));
	TestEqual(TEXT("delivery bot setup path"), DeliveryBotSetupPath, FString(TEXT("Json/Input/DeliveryBotSetupSample_1.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiRequestJsonBuildTest,
	"OdiroSim.Platform.AnalysisAi.RequestJsonBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiRequestJsonBuildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Directory = MakeAnalysisAiTestDirectory();
	const FString ReportPath = FPaths::Combine(Directory, TEXT("evaluation_report.json"));
	const FString MeasurementLogPath = FPaths::Combine(Directory, TEXT("MeasurementLog.jsonl"));
	const FString EpisodeSetupPath = FPaths::Combine(Directory, TEXT("EpisodeSetup.json"));
	const FString DeliveryBotSetupPath = FPaths::Combine(Directory, TEXT("DeliveryBotSetup.json"));

	TestTrue(TEXT("write log"), SaveAnalysisAiTestFile(MeasurementLogPath, TEXT("{\"type\":\"header\"}\n")));
	TestTrue(TEXT("write episode setup"), SaveAnalysisAiTestFile(EpisodeSetupPath, TEXT("{}")));
	TestTrue(TEXT("write bot setup"), SaveAnalysisAiTestFile(DeliveryBotSetupPath, TEXT("{}")));

	const FString ReportJson = FString::Printf(
		TEXT(R"({
			"schema": "episode_evaluation_report",
			"version": 1,
			"run": {
				"episode_setup": { "path": "%s", "hash": "episode-hash" },
				"delivery_bot_setup": { "path": "%s", "hash": "bot-hash" }
			}
		})"),
		*EpisodeSetupPath.Replace(TEXT("\\"), TEXT("/")),
		*DeliveryBotSetupPath.Replace(TEXT("\\"), TEXT("/")));
	TestTrue(TEXT("write report"), SaveAnalysisAiTestFile(ReportPath, ReportJson));

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("request json builds"),
		UPlatformAnalysisAiSubsystem::BuildAnalysisRequestJsonFromReport(
			ReportPath,
			MeasurementLogPath,
			true,
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString RequestReportPath;
	FString RequestLogPath;
	FString RequestEpisodeSetupPath;
	FString RequestBotSetupPath;
	TestTrue(TEXT("has report path"), RequestObject->TryGetStringField(TEXT("evaluation_report_path"), RequestReportPath));
	TestTrue(TEXT("has log path"), RequestObject->TryGetStringField(TEXT("measurement_log_path"), RequestLogPath));
	TestTrue(TEXT("has episode setup path"), RequestObject->TryGetStringField(TEXT("episode_setup_path"), RequestEpisodeSetupPath));
	TestTrue(TEXT("has bot setup path"), RequestObject->TryGetStringField(TEXT("bot_setup_path"), RequestBotSetupPath));
	TestTrue(TEXT("fallback only"), RequestObject->GetBoolField(TEXT("fallback_only")));
	TestTrue(TEXT("request report path is absolute"), FPaths::IsRelative(RequestReportPath) == false);
	TestTrue(TEXT("request log path is absolute"), FPaths::IsRelative(RequestLogPath) == false);
	TestTrue(TEXT("request episode setup path is absolute"), FPaths::IsRelative(RequestEpisodeSetupPath) == false);
	TestTrue(TEXT("request bot setup path is absolute"), FPaths::IsRelative(RequestBotSetupPath) == false);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiStaleAbsoluteSetupPathRemapTest,
	"OdiroSim.Platform.AnalysisAi.StaleAbsoluteSetupPathRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiStaleAbsoluteSetupPathRemapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Directory = MakeAnalysisAiTestDirectory();
	const FString ReportPath = FPaths::Combine(Directory, TEXT("evaluation_report.json"));
	const FString MeasurementLogPath = FPaths::Combine(Directory, TEXT("MeasurementLog.jsonl"));

	TestTrue(TEXT("write log"), SaveAnalysisAiTestFile(MeasurementLogPath, TEXT("{\"type\":\"header\"}\n")));

	const FString ReportJson = TEXT(R"({
		"schema": "episode_evaluation_report",
		"version": 1,
		"run": {
			"episode_setup": {
				"path": "C:/Users/old/Documents/Unreal Projects/Proto-Unreal/Json/Input/ScenarioSetupSample_1.json",
				"hash": "episode-hash"
			},
			"delivery_bot_setup": {
				"path": "C:/Users/old/Documents/Unreal Projects/Proto-Unreal/Json/Input/DeliveryBotSetupSample_1.json",
				"hash": "bot-hash"
			}
		}
	})");
	TestTrue(TEXT("write report"), SaveAnalysisAiTestFile(ReportPath, ReportJson));

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("request json builds"),
		UPlatformAnalysisAiSubsystem::BuildAnalysisRequestJsonFromReport(
			ReportPath,
			MeasurementLogPath,
			true,
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString RequestEpisodeSetupPath;
	FString RequestBotSetupPath;
	TestTrue(TEXT("has episode setup path"), RequestObject->TryGetStringField(TEXT("episode_setup_path"), RequestEpisodeSetupPath));
	TestTrue(TEXT("has bot setup path"), RequestObject->TryGetStringField(TEXT("bot_setup_path"), RequestBotSetupPath));

	FString ExpectedEpisodeSetupPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Json/Input/ScenarioSetupSample_1.json"));
	FString ExpectedBotSetupPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Json/Input/DeliveryBotSetupSample_1.json"));
	FPaths::NormalizeFilename(ExpectedEpisodeSetupPath);
	FPaths::NormalizeFilename(ExpectedBotSetupPath);
	FPaths::CollapseRelativeDirectories(ExpectedEpisodeSetupPath);
	FPaths::CollapseRelativeDirectories(ExpectedBotSetupPath);

	TestEqual(TEXT("episode setup remapped"), RequestEpisodeSetupPath, ExpectedEpisodeSetupPath);
	TestEqual(TEXT("bot setup remapped"), RequestBotSetupPath, ExpectedBotSetupPath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiDisplayTextTest,
	"OdiroSim.Platform.AnalysisAi.DisplayText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiDisplayTextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ResponseJson = TEXT(R"({
		"schemaVersion": "1.0.0",
		"analysisId": "analysis-001",
		"generationMethod": "fallback_rules",
		"summary": "fallback 규칙 기반으로 추천을 생성했습니다.",
		"episodeStatistics": {
			"outcome": "Failure",
			"terminal_reason": "DeliveryBotSimulationFailed",
			"duration_s": 38.6,
			"score": -7.0
		},
		"botSetupRecommendations": [
			{
				"param": "stop_distance_m",
				"current": 1.2,
				"suggested": 1.5,
				"reason": "near miss 때문에 정지 거리를 늘립니다."
			}
		],
		"episodeSetupRecommendations": [],
		"policyServerRecommendations": [
			{
				"param": "force_action_override",
				"current": "Repath",
				"suggested": "None",
				"reason": "운영 회차에서 강제 액션을 해제합니다."
			}
		],
		"llmWarnings": ["fallback_only=True"]
	})");

	TArray<FString> Diagnostics;
	const FString DisplayText = UPlatformAnalysisAiSubsystem::BuildDisplayTextFromAnalysisResponse(
		ResponseJson,
		Diagnostics);
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);
	TestTrue(TEXT("contains summary"), DisplayText.Contains(TEXT("fallback 규칙 기반")));
	TestTrue(TEXT("contains bot recommendation"), DisplayText.Contains(TEXT("stop_distance_m")));
	TestTrue(TEXT("contains policy recommendation"), DisplayText.Contains(TEXT("force_action_override")));
	TestTrue(TEXT("contains warning"), DisplayText.Contains(TEXT("fallback_only=True")));

	return true;
}

#endif
