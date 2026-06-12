# Sourcemap

Use before broad source navigation.

## Areas
- [Runtime Module](module.md): module, targets, dependencies, plugins
- [Delivery Bot](delivery-bot.md): robot actors, movement, pathing, grid, policy
- [Scenario](scenario.md): scenario JSON compile, runtime spawn/evaluation, pedestrian plans, in-game editor, LLM authoring
- [Episode Logging](episode.md): per-episode measurement logs and evaluation report artifacts
- [Platform](platform.md): MainMenuMap UI, simulator launch/process bootstrap, status polling
- [Shared Types](shared-types.md): specs, diagnostics, run/evaluation records
- [Assets And Config](assets-config.md): content roots, maps, Blueprint/config boundary

## Terminology
- "Scenario" = authored/compiled environment spec (ground regions, obstacles, pedestrians, robot setup)
- "Episode" = one completed simulation iteration and its result artifacts (logs, evaluation result)

## Maintenance
- Update only when mapped paths, responsibilities, or entry points change
