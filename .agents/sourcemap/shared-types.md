# Shared Types

Covers: `Source/ProtoRobotSim/Public/Shared`, `Source/ProtoRobotSim/Private/Shared`

## Entry Points
- `ScenarioCoreTypes.h`: shared scenario enums, `FScenarioParamValue` variant, static obstacle prop entry
- `ScenarioSpecTypes.h`: compiled world/ground/path/actor/event specs (`FScenarioWorldSpec`, `FScenarioSimulationSetupSpec`)
- `ScenarioCompileTypes.h`: compiler diagnostics and result wrapper (`FScenarioCompileResult`)
- `ScenarioConfigTypes.h`: run config (template id/version, generator version, base seed, iteration), seed ledger, evaluation config, runner state
- `ScenarioPedestrianPlanTypes.h`: pedestrian plan points/reservations, behavior params, path shape params, plan hash fields
- `EpisodeConfigTypes.h`: episode evaluation outcome/event/result, `FEpisodeRunRecord` (see episode.md)
- `EpisodeMeasurementLogTypes.h`, `EpisodeJsonlMeasurementWriter.h`, `EpisodeLogSubjectRegistry.h`, `EpisodeEvaluationReportJson.h`: episode-scoped logging and report (see episode.md)
- `SimulationSetupTypes.h` and `.cpp`: launcher/simulator shared simulation setup, run status JSON reader/writer, command-line contract types
- `Struct/`: delivery bot pathing, movement, drive, policy, observation structs

## Notes
- Surface: `FScenarioWorldSpec`, `FScenarioCompileResult`, `FScenarioRunConfig`, `FScenarioPedestrianPlan`, `FEpisodeEvaluationResult`, `FEpisodeRunRecord`, `FSimulationSetup`, `FSimulationRunStatus`, `FDeliveryBot*`
- Prefer adding focused structs here over coupling Delivery Bot and Scenario headers directly
