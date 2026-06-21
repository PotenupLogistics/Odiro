#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/ScenarioDocumentJson.h"

namespace
{
	// Parses a JSON object for scenario LLM authoring automation tests.
	bool ParseScenarioLlmTestJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioLlmAuthoringV2RequestTest,
	"OdiroSim.ScenarioLlm.Authoring.V2Request",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioLlmAuthoringV2RequestTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("v2 request json builds"),
		UScenarioLlmAuthoringSubsystem::BuildScenarioGenerateV2RequestJson(
			TEXT("좁은 보도에서 정적 장애물을 배치해줘"),
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseScenarioLlmTestJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString Prompt;
	TestTrue(TEXT("has prompt"), RequestObject->TryGetStringField(TEXT("prompt"), Prompt));
	TestTrue(TEXT("prompt preserved"), Prompt.Contains(TEXT("좁은 보도")));
	TestFalse(TEXT("no project path"), RequestObject->HasField(TEXT("project_path")));
	TestFalse(TEXT("no scenario path"), RequestObject->HasField(TEXT("scenario_path")));
	TestFalse(TEXT("no episode count"), RequestObject->HasField(TEXT("episode_count")));

	FString BlankRequestJson;
	TArray<FString> BlankDiagnostics;
	TestFalse(
		TEXT("blank prompt rejected"),
		UScenarioLlmAuthoringSubsystem::BuildScenarioGenerateV2RequestJson(
			TEXT("   "),
			BlankRequestJson,
			BlankDiagnostics));
	TestTrue(TEXT("blank prompt diagnostics"), BlankDiagnostics.Num() > 0);

	const FString ScenarioJson = TEXT(R"({
		"schema": "scenario",
		"version": 1,
		"scenario_id": "llm_generated_static_obstacle",
		"intent": "정적 장애물이 있는 좁은 보도 주행을 검증한다.",
		"corridor": {
			"axis": {
				"type": "polyline",
				"points_m": [[0.0, 0.0], [10.0, 0.0]]
			},
			"walkway_width_m": 3.0,
			"segments": [
				{
					"id": "main",
					"type": "straight",
					"along_range_m": [0.0, 10.0]
				}
			]
		},
		"obstacles": {},
		"pedestrians": {},
		"robot": {
			"start": { "type": "entry" },
			"goal": { "type": "exit" }
		}
	})");

	const FScenarioDocumentParseResult ParseResult =
		FScenarioDocumentJson::ParseProjectScenarioFromString(ScenarioJson);
	TestTrue(TEXT("wrapper-free v2 scenario response parses"), ParseResult.bSuccess);

	return true;
}

#endif
