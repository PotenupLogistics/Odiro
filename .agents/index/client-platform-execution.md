---
id: client-platform-execution
owner: Client
paths:
  - Client/README.md
  - Client/Source/OdiroSim/Public/Platform/**
  - Client/Source/OdiroSim/Private/Platform/**
  - Client/Task-*.bat
  - Client/Docs/plans/PLAN-platform-architecture.md
  - Client/Tools/**
  - task-run.bat
  - task-dev.bat
  - tools/run-preview.ps1
  - tools/dev-session.ps1
entry:
  - Client/README.md
  - SimulatorLaunchSubsystem.h / .cpp
  - SimulatorProcessSubsystem.h / .cpp
  - ScenarioEditorLaunchSubsystem.h / .cpp
  - PlatformAnalysisAiSubsystem.h / .cpp
  - MainMenuPlayerController.h / .cpp
  - Widget/MainMenuWidget.h / .cpp
  - Client/Docs/plans/PLAN-platform-architecture.md
  - Client/Task-RunPreview.bat
  - Client/Task-RunPythonPolicyServer.bat
  - Client/Tools/CheckPrerequisites.ps1
  - Client/Tools/Build.ps1
  - Client/Tools/Dev.ps1
  - Client/Tools/RunPreview.ps1
keep:
  - Platform UI reads launcher process state and canonical user-project run artifacts, not simulator world objects.
  - Client/Tools use Client/Tools/Common.ps1, never root tools/common.ps1.
  - Client prerequisite checks cover Windows Unreal C++ only; Android/iOS/macOS/MAUI are not failures.
  - Legacy Client root scripts such as BuildProject.bat, RunPreview.bat, and RunPythonPolicyServer.bat stay folded into Task-* wrappers.
  - Project run launch passes `-OdiroProject`, `-RunId`, and Bridge-assigned `-PolicyPort`; simulator starts direct episode inputs instead of a generated RunQueue file.
  - SimulatorProcessSubsystem accepts only the project-run command contract; `-Simulate` is rejected as an unsupported legacy argument.
  - Project run child process exits after terminal runner state; Bridge owns status JSON lifecycle.
  - Project run completion writes user project result artifacts through the simulator process path; episode trace starts/stops through runner lifecycle.
  - PlatformAnalysisAi has a v2 project-run request path using `project_path` + `run_id`; successful responses are saved under run `review/`.
  - ScenarioEditorLaunchSubsystem treats the pending path as a project scenario JSON path; URL options are inspectable only and subsystem state is authoritative.
  - MainMenu Scenario page lists, opens, and starts runs from `Saved/UserProjects/**/scenario.json` by preparing a user project run snapshot before `StartProjectRun`.
  - MainMenu Experiment Config mirrors the project scenario list and routes edit/run actions through `<UserProject>/scenario.json` project-run flow.
  - MainMenu project result mode reads result runs from `<UserProject>/runs/<RunId>` and sends AI analysis through the v2 project-run path.
  - MainMenu project mode must not call SimulationSetup or RunQueue writer/launcher paths.
  - MainMenu project result mode does not fall back to legacy Json/Input, SimulationSetup, RunQueue, Saved/SimulationRuns, or Saved/AnalysisLogs lists.
  - Legacy report + MeasurementLog analysis is removed; MainMenu project mode sends v2 project-run analysis only.
  - SimulatorLaunchSubsystem public API exposes project-run launch/snapshot helpers; legacy SimulationSetup/RunQueue launch/edit helpers are removed from the public surface.
verify:
  - launcher command contract tests for launch arg changes
  - runtime log plus process exit code for simulator process changes
  - `OdiroSim.SimulatorLaunch.ProjectRun.Validation` for project launch validation changes
  - `OdiroSim.Platform.AnalysisAi.ProjectRunRequestJsonBuild` for v2 analysis request changes
  - OdiroSimEditor build after runner/trace lifecycle changes
  - UI smoke only after MainMenu workflow changes
related:
  - client-runtime-foundation
  - client-simulation
  - agents-generation-runtime
  - root-dev-workflow
---
