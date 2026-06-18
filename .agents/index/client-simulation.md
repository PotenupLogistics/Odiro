---
id: client-simulation
owner: Client
paths:
  - Client/Source/OdiroSim/Public/Scenario/**
  - Client/Source/OdiroSim/Private/Scenario/**
  - Client/Source/OdiroSim/Public/Episode/**
  - Client/Source/OdiroSim/Private/Episode/**
  - Client/Source/OdiroSim/Public/Shared/**
  - Client/Source/OdiroSim/Private/Shared/**
  - Client/Docs/plans/PLAN-260602-sim-logging.md
  - Client/Json/Input/**
  - contracts/specs/**
entry:
  - ScenarioSchemaTypes.h
  - ScenarioDocumentTypes.h
  - ScenarioSampleTypes.h
  - ScenarioDocumentJson.h / .cpp
  - ScenarioSampleJson.h / .cpp
  - ScenarioSampleWorldSpecAdapter.h / .cpp
  - UserProjectEpisodeScenarioWorldSpecAdapter.h / .cpp
  - Scenario/Editor/ScenarioAuthoringSubsystem.h / .cpp
  - Scenario/Llm/ScenarioLlmAuthoringSubsystem.h / .cpp
  - Scenario/Llm/ScenarioLlmPromptWidget.h / .cpp
  - SimulationSetupTypes.h / .cpp
  - UserProjectDataTypes.h / .cpp
  - SimulationSetupTypesTest.cpp
  - UserProjectDataTypesTest.cpp
  - ScenarioDocumentSampleJsonTest.cpp
  - ScenarioSampleWorldSpecAdapterTest.cpp
  - UserProjectEpisodeScenarioWorldSpecAdapterTest.cpp
  - ScenarioSimulationSubsystem.h / .cpp
  - ScenarioRunnerSubsystem.h / .cpp
  - EpisodeMeasurementLogSubsystem.h / .cpp
  - EpisodeResultTypes.h
  - Client/Docs/plans/PLAN-260602-sim-logging.md
  - Client/Json/Input
  - contracts/specs
  - contracts/specs/user-project-data.md
keep:
  - Parser-only samples/schemas owned only by Client stay in Client.
  - Client/Docs/plans/PLAN-260602-sim-logging.md is superseded legacy context; final log file contracts belong in contracts/specs/user-project-data.md.
  - Client/Docs/Data was removed; shared user project file contracts belong in contracts/specs/user-project-data.md.
  - Scenario setup/run queue samples in Client/Json/Input are legacy client-owned examples; project run execution no longer accepts them through ScenarioRunnerSubsystem.
  - ScenarioDocument* and ScenarioSample* remain internal editor/materialization surfaces unless a task explicitly targets them; public C++ editor entry points use project scenario naming.
  - Final user project contract uses one editable `<UserProject>/scenario.json`; do not add new user-facing template/sample split.
  - Episode scenario files under `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json` are derived execution artifacts.
  - Project run uses episode input arrays and `episode_scenario` adapter, not generated RunQueue files or old actor-spawn scenario setup JSON.
  - ScenarioRunnerSubsystem public start API accepts direct episode scenario/profile run inputs; file-based RunQueue and ScenarioSetup start helpers are removed.
  - Scenario LLM authoring saves v2 `scenario` responses to user project `scenario.json`; it must not save or execute RunQueue files.
  - Scenario LLM prompt generate/load/run is scoped to the current `<UserProject>/scenario.json`; run launches create `<UserProject>/runs/<RunId>` snapshots through SimulatorLaunchSubsystem.
  - Project run output uses `FUserProjectRunOutputJson` for `result.json`, `events.jsonl`, `actions.jsonl`, `trace.jsonl`, and `summary.json`.
  - Scenario authoring/runtime projection stays separate from runtime WorldSpec and actor-spawn payload types.
verify:
  - contract specs vs sample JSON alignment
  - `scenario`/`episode_scenario` docs vs Client shared schema type alignment
  - Scenario document parse, project `scenario.json` adapter, version mismatch, episode scenario generation, and round-trip automation tests
  - Scenario-to-WorldSpec adapter automation tests, including user-project episode scenario adapter, and OdiroSimEditor build after adapter/editor draft changes
  - `OdiroSim.UserProjectData.RunOutput.Write` after user project result writer changes
  - `OdiroSim.UserProjectData.RobotAction.Write` after policy action logging changes
  - `OdiroSim.UserProjectData.EpisodeTrace.Write` after runtime trace logging changes
  - focused automation tests for Scenario/Episode changes
  - Client/Task-RunPreview.bat smoke when wrapper supports the changed mode
related:
  - contracts-shared-data
  - client-delivery-bot-policy
  - agents-generation-runtime
---
