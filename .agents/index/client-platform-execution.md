---
id: client-platform-execution
owner: Client
paths:
  - Client/Source/ProtoRobotSim/Public/Platform/**
  - Client/Source/ProtoRobotSim/Private/Platform/**
  - Client/Task-*.bat
  - Client/Tools/**
  - task-run.bat
  - task-dev.bat
  - tools/run-preview.ps1
  - tools/dev-session.ps1
entry:
  - SimulatorLaunchSubsystem.h / .cpp
  - SimulatorProcessSubsystem.h / .cpp
  - EpisodeEditorLaunchSubsystem.h / .cpp
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
  - Platform UI reads simulator state via status/report/log files, not simulator world objects.
  - Client/Tools use Client/Tools/Common.ps1, never root tools/common.ps1.
  - Client prerequisite checks cover Windows Unreal C++ only; Android/iOS/macOS/MAUI are not failures.
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
