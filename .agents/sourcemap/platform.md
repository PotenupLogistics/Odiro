# Platform

Covers: `Source/ProtoRobotSim/Public/Platform`, `Source/ProtoRobotSim/Private/Platform`

## Entry Points
- `SimulatorLaunchSubsystem.h` and `.cpp`: simulator subprocess launcher, `RunPreview.bat` development fallback, status polling, fixed-step FPS save helper
- `EpisodeEditorLaunchSubsystem.h` and `.cpp`: MainMenu-to-`EpisodeEditorMap` navigation, pending EpisodeSetup path handoff, editor controller auto-load attempt
- `SimulatorProcessSubsystem.h` and `.cpp`: simulator process bootstrap, `-Simulate=<SimulationSetupFile>` detection, fixed-step setup, target map load, runner/log setup, run status JSON updates
- `MainMenuPlayerController.h` and `.cpp`: creates the MainMenu widget, adds it to the player viewport, and owns menu input mode
- `/Game/Blueprints/MainMenu/BP_MainMenuGameMode`: map-level GameMode asset that selects `AMainMenuPlayerController`
- `Widget/MainMenuWidget.h` and `.cpp`: document-aligned MainMenu sidebar sections for scenario, policy, experiment config, run status, and experiment result; includes setup/run queue selector, simulator launcher, new/load editor map entry, policy JSON external editor launch, status/report/log preview UI
- `Private/Platform/Tests`: launcher command contract, terminal-state automation, simulator process helper automation

## Notes
- Platform UI does not load `EpisodeSimulationMap` directly; simulator work runs in a separate process
- Development fallback calls `RunPreview.bat` with the same public args as packaged execution: `-Simulate=<SimulationSetupFile>` and `-RunId=<RunId>`
- SimulatorMode is inferred from `-Simulate`; external `-SimulatorMode`, `-UseFixedTimeStep`, and `-FPS` args are not required
- `SimulationSetup.report`, `SimulationSetup.logging`, and `SimulationSetup.status` configure simulator process output without direct UI/world coupling
- MainMenu UI ownership lives in `AMainMenuPlayerController`, not a GameInstanceSubsystem
- MainMenuMap must use `BP_MainMenuGameMode` or another Blueprint GameMode that selects `AMainMenuPlayerController`
- UI reads simulator progress through `SimulationRunStatus JSON`, report JSON paths, and measurement log paths instead of simulator world objects
- MainMenu opens `EpisodeEditorMap` through `UEpisodeEditorLaunchSubsystem`; selected EpisodeSetup auto-load requires the editor map's GameMode to use `AEpisodeEditorController`
- MainMenu can pass `EpisodeSetup=...` or `NewEpisode=1` when opening `EpisodeEditorMap`; `UEpisodeEditorEntryWidget` hides its initial load/new screen after this auto-start succeeds
