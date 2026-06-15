---
id: client-platform-execution
owner: Client
paths:
  - Client/Source/OdiroSim/Public/Platform/**
  - Client/Source/OdiroSim/Private/Platform/**
  - Client/Task-*.bat
  - Client/Tools/**
  - task-run.bat
  - task-dev.bat
  - tools/run-preview.ps1
  - tools/dev-session.ps1
entry:
  - SimulatorLaunchSubsystem.h / .cpp
  - SimulatorProcessSubsystem.h / .cpp
  - ScenarioEditorLaunchSubsystem.h / .cpp
  - PlatformAnalysisAiSubsystem.h / .cpp
  - MainMenuPlayerController.h / .cpp
  - Widget/MainMenuWidget.h / .cpp
  - Client/Task-RunPreview.bat
  - Client/Task-RunPythonPolicyServer.bat
  - Client/Tools/CheckPrerequisites.ps1
  - Client/Tools/Build.ps1
  - Client/Tools/Dev.ps1
  - Client/Tools/RunPreview.ps1
keep:
  - Platform UI reads simulator state via status/result/log files, not simulator world objects.
  - Client/Tools use Client/Tools/Common.ps1, never root tools/common.ps1.
  - Client prerequisite checks cover Windows Unreal C++ only; Android/iOS/macOS/MAUI are not failures.
  - Legacy Client root scripts such as BuildProject.bat, RunPreview.bat, and RunPythonPolicyServer.bat stay folded into Task-* wrappers.
  - Simulator launch public contract is `-Experiment=<ExperimentRef>` with optional `-RunId=<RunId>` and `-SampleIds=<Ids>`.
  - MainMenu launches experiment folders directly; child simulator runs write `runs/<RunId>/status.json` under the selected experiment folder.
  - MainMenu result detail reads canonical `episode_result` files; legacy evaluation reports remain only for compatibility analysis tools.
verify:
  - launcher command contract tests for launch arg changes
  - runtime log plus status JSON for process changes
  - UI smoke for MainMenu workflow changes
related:
  - client-runtime-foundation
  - client-simulation
  - agents-generation-runtime
  - root-dev-workflow
---
