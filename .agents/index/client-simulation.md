---
id: client-simulation
owner: Client
paths:
  - Client/Source/OdiroSim/Public/Scenario/**
  - Client/Source/OdiroSim/Private/Scenario/**
  - Client/Source/OdiroSim/Public/Episode/**
  - Client/Source/OdiroSim/Private/Episode/**
  - Client/Json/Input/**
  - contracts/specs/**
entry:
  - ScenarioCompiler.h / .cpp
  - ScenarioSchemaTypes.h
  - ScenarioTemplateTypes.h
  - ScenarioSampleTypes.h
  - ScenarioSimulationSubsystem.h / .cpp
  - ScenarioRunnerSubsystem.h / .cpp
  - EpisodeMeasurementLogSubsystem.h / .cpp
  - EpisodeEvaluationReportJson.h / .cpp
  - EpisodeResultTypes.h
  - Client/Json/Input
  - contracts/specs
keep:
  - Parser-only samples/schemas owned only by Client stay in Client.
  - Scenario setup/run queue samples in Client/Json/Input are client-owned examples; shared contract truth stays in contracts/specs.
  - Scenario template/sample authoring types stay separate from runtime WorldSpec and actor-spawn payload types.
verify:
  - contract specs vs sample JSON alignment
  - scenario_template/scenario_sample docs vs Client shared schema type alignment
  - focused automation tests for Scenario/Episode changes
  - Client/Task-RunPreview.bat smoke when wrapper supports the changed mode
related:
  - contracts-shared-data
  - client-delivery-bot-policy
  - agents-generation-runtime
---
