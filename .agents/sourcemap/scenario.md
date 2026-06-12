# Scenario

Covers: `Source/ProtoRobotSim/Public/Scenario`, `Source/ProtoRobotSim/Private/Scenario`, `Docs/JSON_Guide/EpisodeSetup_JSON_Guide.md`

## Entry Points
- `Public/Scenario/ScenarioCompiler.h`: scenario setup JSON to `FScenarioWorldSpec` compile entry points, diagnostics
- `Public/Scenario/ScenarioRunnerSubsystem.h`: batch loop over run queue pairs; compile -> setup -> evaluate -> `FEpisodeRunRecord`
- `Public/Scenario/ScenarioSimulationSubsystem.h`: world spawn/clear, runtime actor lookup by `instance_id`, delivery bot grid bounds derivation, pedestrian plan context build
- `Public/Scenario/ScenarioEvaluationSubsystem.h`: tick evaluation (collisions, near-miss intervals, region violations), score/events, episode end decision
- `Public/Scenario/ScenarioPedestrianPlanSubsystem.h` + `Private/Scenario/ScenarioPedestrianPlanBuilder.h`: deterministic pedestrian baseline plans with hash chain (`SourceSpecHash`/`ResolvedFootprintHash`/`SemanticNavigationHash`/`PlanHash`/`BehaviorHash`/`PedestrianScenarioHash`)
- `Public/Scenario/Components/ScenarioPedestrianRuntimeComponent.h`: baseline follow + deterministic robot-only reaction state machine (`FollowBaseline`/`YieldSlowdown`/`YieldStop`/`Sidestep`/`Blocked`/`Recover`), runtime metrics (schedule delay, forced wait, deviation)
- `Public/Scenario/Editor/ScenarioAuthoringSubsystem.h` + `Editor/ScenarioEditorController.h`: `ScenarioEditorMap` in-game editor, authoring records, save to scenario setup JSON (schema `scenario_actor_spawn_mvp`)
- `Public/Scenario/Llm/ScenarioLlmAuthoringSubsystem.h`: natural-language prompt -> AI server -> run queue + scenario setup JSON save
- `Public/Scenario/Data`: `ScenarioStaticObstaclePropCatalog` (prop entries, collision/safety settings), `ScenarioAssetPaletteCatalog` (editor palette); default assets under `/Game/Data/Scenario/`
- `Public/Scenario/Actors`, `Components`, `Widget`: ground region, spline path, static obstacle, pedestrian actors; placeable metadata; editor UI widgets

## Notes
- Authored JSON uses meters/degrees; runtime specs use Unreal centimeters
- Actor `instance_id` is unique across static obstacles, pedestrians, and robot
- Pedestrian = setup-time planned baseline trajectory + deterministic robot-only runtime reaction; no catch-up after robot-caused delay (`Docs/Pedestrian_Social_Evaluation_Design.md`)
- Planned refactor: recipe -> generator -> manifest layering (`Docs/specs/ScenarioRecipe_V1.md`)
