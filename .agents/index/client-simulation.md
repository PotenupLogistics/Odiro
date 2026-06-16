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
  - Client/Docs/README.md
  - Client/Docs/Data/**
  - Client/Json/Input/**
  - contracts/specs/**
entry:
  - ScenarioCompiler.h / .cpp
  - ScenarioSchemaTypes.h
  - ScenarioTemplateTypes.h
  - ScenarioSampleTypes.h
  - ScenarioTemplateJson.h / .cpp
  - ScenarioSampleJson.h / .cpp
  - ScenarioSampleWorldSpecAdapter.h / .cpp
  - Scenario/Editor/ScenarioAuthoringSubsystem.h / .cpp
  - ScenarioTemplateSampleJsonTest.cpp
  - ScenarioSampleWorldSpecAdapterTest.cpp
  - ScenarioSimulationSubsystem.h / .cpp
  - ScenarioRunnerSubsystem.h / .cpp
  - EpisodeMeasurementLogSubsystem.h / .cpp
  - EpisodeEvaluationReportJson.h / .cpp
  - EpisodeResultTypes.h
  - Client/Json/Input
  - contracts/specs
keep:
  - Parser-only samples/schemas owned only by Client stay in Client.
  - Scenario setup/run queue samples in Client/Json/Input are legacy client-owned examples until the user project migration removes them.
  - Final user project contract uses one editable `<UserProject>/scenario.json`; do not add new user-facing template/sample split.
  - Episode scenario files under `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json` are derived execution artifacts.
  - Scenario authoring/runtime projection stays separate from runtime WorldSpec and actor-spawn payload types.
verify:
  - contract specs vs sample JSON alignment
  - `scenario`/`episode_scenario` docs vs Client shared schema type alignment
  - Scenario parse, version mismatch, episode scenario generation, and round-trip automation tests
  - Scenario-to-WorldSpec adapter automation tests and OdiroSimEditor build after adapter/editor draft changes
  - focused automation tests for Scenario/Episode changes
  - Client/Task-RunPreview.bat smoke when wrapper supports the changed mode
related:
  - contracts-shared-data
  - client-delivery-bot-policy
  - agents-generation-runtime
---
