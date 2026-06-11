# Shared Types

Covers: `Source/ProtoRobotSim/Public/Shared`, `Source/ProtoRobotSim/Private/Shared`

## Entry Points
- `EpisodeCoreTypes.h`: shared episode enums and parameter values
- `EpisodeSpecTypes.h`: compiled world, ground, path, actor, and event specs
- `EpisodeCompileTypes.h`: compiler diagnostics and result wrapper
- `EpisodeConfigTypes.h`: run config, seed ledger, runner records, saved report path
- `EpisodeReplayTypes.h` and `.cpp`: replay settings, actor info, frame/header/footer records
- `SimulationSetupTypes.h` and `.cpp`: launcher/simulator shared simulation setup, run status, status JSON reader/writer, and command-line contract types
- `ScenarioSpecTypes.h`: scenario authoring structs
- `Struct/`: delivery bot pathing, movement, drive, and legacy replay structs

## Notes
- Surface: `FEpisodeWorldSpec`, `FEpisodeCompileResult`, `FEpisodeRunConfig`, `FEpisodeReplay*`, `FSimulationSetup`, `FSimulationRunStatus`, `FScenarioSpec`, `FDeliveryBot*`
- Prefer adding focused structs here over coupling Delivery Bot and Episode headers directly
