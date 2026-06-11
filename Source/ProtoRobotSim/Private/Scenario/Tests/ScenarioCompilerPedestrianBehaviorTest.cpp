#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioCompiler.h"
#include "Misc/AutomationTest.h"

namespace
{
	const FScenarioParamValue* FindPedestrianProperty(
		const FScenarioCompileResult& compileResult,
		const FString& key)
	{
		if (compileResult.WorldSpec.DynamicActors.Num() != 1)
		{
			return nullptr;
		}

		return compileResult.WorldSpec.DynamicActors[0].Properties.Find(key);
	}

	double GetFloatPropertyOrDefault(
		const FScenarioCompileResult& compileResult,
		const FString& key,
		double defaultValue)
	{
		const FScenarioParamValue* paramValue = FindPedestrianProperty(compileResult, key);
		return paramValue && paramValue->Type == EScenarioParamValueType::Float
			? paramValue->FloatValue
			: defaultValue;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCompilerPedestrianBehaviorOptionalTest,
	"ProtoRobotSim.Scenario.Compiler.PedestrianBehaviorOptional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCompilerPedestrianBehaviorOptionalTest::RunTest(const FString& Parameters)
{
	const FString json = TEXT(R"JSON(
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "pedestrian_behavior_optional_test",
  "map_id": "EpisodeSandbox",
  "actors": {
    "pedestrians": [
      {
        "instance_id": "ped_01",
        "archetype_id": "adult_pedestrian",
        "start_xy_m": [0, 0],
        "goal_xy_m": [2, 0],
        "movement": {
          "model": "planned_trajectory",
          "speed_mps": 1.2,
          "auto_start": true
        }
      }
    ]
  }
}
)JSON");

	const UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
	const FScenarioCompileResult result = compiler->CompileEpisodeWorldSpecFromJsonString(json);

	TestTrue(TEXT("compile succeeds without behavior"), result.bSuccess);
	TestEqual(TEXT("one pedestrian compiled"), result.WorldSpec.DynamicActors.Num(), 1);
	TestTrue(TEXT("behavior property is absent when omitted"), FindPedestrianProperty(result, TEXT("behavior_cooperation")) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCompilerPedestrianBehaviorValuesTest,
	"ProtoRobotSim.Scenario.Compiler.PedestrianBehaviorValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCompilerPedestrianBehaviorValuesTest::RunTest(const FString& Parameters)
{
	const FString json = TEXT(R"JSON(
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "pedestrian_behavior_values_test",
  "map_id": "EpisodeSandbox",
  "actors": {
    "pedestrians": [
      {
        "instance_id": "ped_01",
        "archetype_id": "adult_pedestrian",
        "start_xy_m": [0, 0],
        "goal_xy_m": [2, 0],
        "movement": {
          "model": "planned_trajectory",
          "speed_mps": 1.2,
          "auto_start": true
        },
        "behavior": {
          "cooperation": 0.8,
          "evasiveness": 0.7,
          "personal_space_m": 1.1,
          "awareness_horizon_s": 3.0,
          "max_yield_wait_s": 2.5,
          "sidestep_distance_m": 0.9
        }
      }
    ]
  }
}
)JSON");

	const UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
	const FScenarioCompileResult result = compiler->CompileEpisodeWorldSpecFromJsonString(json);

	TestTrue(TEXT("compile succeeds with behavior"), result.bSuccess);
	TestEqual(TEXT("one pedestrian compiled"), result.WorldSpec.DynamicActors.Num(), 1);
	TestEqual(TEXT("cooperation parsed"), GetFloatPropertyOrDefault(result, TEXT("behavior_cooperation"), -1.0), 0.8);
	TestEqual(TEXT("evasiveness parsed"), GetFloatPropertyOrDefault(result, TEXT("behavior_evasiveness"), -1.0), 0.7);
	TestEqual(TEXT("personal space converted to cm"), GetFloatPropertyOrDefault(result, TEXT("behavior_personal_space_cm"), -1.0), 110.0);
	TestEqual(TEXT("awareness horizon parsed"), GetFloatPropertyOrDefault(result, TEXT("behavior_awareness_horizon_s"), -1.0), 3.0);
	TestEqual(TEXT("yield wait parsed"), GetFloatPropertyOrDefault(result, TEXT("behavior_max_yield_wait_s"), -1.0), 2.5);
	TestEqual(TEXT("sidestep distance converted to cm"), GetFloatPropertyOrDefault(result, TEXT("behavior_sidestep_distance_cm"), -1.0), 90.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCompilerPedestrianPathCurveValuesTest,
	"ProtoRobotSim.Scenario.Compiler.PedestrianPathCurveValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCompilerPedestrianPathCurveValuesTest::RunTest(const FString& Parameters)
{
	const FString json = TEXT(R"JSON(
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "pedestrian_path_curve_values_test",
  "map_id": "EpisodeSandbox",
  "actors": {
    "pedestrians": [
      {
        "instance_id": "ped_01",
        "archetype_id": "adult_pedestrian",
        "start_xy_m": [0, 0],
        "goal_xy_m": [2, 0],
        "movement": {
          "model": "planned_trajectory",
          "speed_mps": 1.2,
          "curve_offset_m": 0.6,
          "curve_sample_spacing_m": 0.25,
          "auto_start": true
        }
      }
    ]
  }
}
)JSON");

	const UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
	const FScenarioCompileResult result = compiler->CompileEpisodeWorldSpecFromJsonString(json);

	TestTrue(TEXT("compile succeeds with path curve"), result.bSuccess);
	TestEqual(TEXT("one pedestrian compiled"), result.WorldSpec.DynamicActors.Num(), 1);
	TestEqual(TEXT("curve offset converted to cm"), GetFloatPropertyOrDefault(result, TEXT("path_curve_offset_cm"), -1.0), 60.0);
	TestEqual(TEXT("curve sample spacing converted to cm"), GetFloatPropertyOrDefault(result, TEXT("path_curve_sample_spacing_cm"), -1.0), 25.0);
	return true;
}

#endif
