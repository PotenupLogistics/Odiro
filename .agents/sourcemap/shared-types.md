# Shared Types

Covers: `Source/ProtoRobotSim/Public/Shared`, `Source/ProtoRobotSim/Private/Shared`

## Entry Points
- `EpisodeCoreTypes.h`: shared episode enums and parameter values
- `EpisodeSpecTypes.h`: compiled world, ground, path, actor, and event specs
- `EpisodeCompileTypes.h`: compiler diagnostics and result wrapper
- `EpisodeConfigTypes.h`: run config and seed ledger
- `EpisodeReplayTypes.h` and `.cpp`: replay settings, actor info, frame/header/footer records
- `ScenarioSpecTypes.h`: scenario authoring structs
- `Struct/`: delivery bot pathing, movement, drive, and legacy replay structs

## Notes
- Surface: `FEpisodeWorldSpec`, `FEpisodeCompileResult`, `FEpisodeRunConfig`, `FEpisodeReplay*`, `FScenarioSpec`, `FDeliveryBot*`
- Prefer adding focused structs here over coupling Delivery Bot and Episode headers directly
