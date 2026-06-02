# Episode

Covers: `Source/ProtoRobotSim/Public/Episode`, `Source/ProtoRobotSim/Private/Episode`, `Docs/EpisodeSetup_JSON_Guide.md`

## Entry Points
- `Public/Episode/EpisodeCompiler.h`: JSON to `FEpisodeWorldSpec` compile entry points
- `Public/Episode/EpisodeSimulationSubsystem.h`: runtime spawn and lookup API
- `Public/Episode/Actors`: ground region, spline path, static obstacle, pedestrian, vehicle actors
- `Public/Episode/Components`: placeable metadata, obstacle collision, path follower components
- `Public/Episode/Interfaces`: physics participant and replay trackable extension points
- `Docs/EpisodeSetup_JSON_Guide.md`: authored JSON contract

## Notes
- Surface: `UEpisodeCompiler`, `UEpisodeSimulationSubsystem`, `UEpisodeDefinition`, `UEpisodePlaceableAssetCatalog`, episode actors/components, `IEpisode*`
- Authored JSON uses meters/degrees; runtime transforms use Unreal centimeters/degrees
- Actor `instance_id` is unique across static obstacles, pedestrians, and robot
