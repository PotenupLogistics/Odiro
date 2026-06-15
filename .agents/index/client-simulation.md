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
  - Client/Json/Input/**
  - contracts/specs/**
entry:
  - ScenarioCompiler.h / .cpp
  - ScenarioSchemaTypes.h
  - ScenarioTemplateTypes.h
  - ScenarioSampleTypes.h
  - ScenarioTemplateJson.h / .cpp
  - ScenarioSampleJson.h / .cpp
  - ScenarioTemplateSampler.h / .cpp
  - ScenarioSampleWorldSpecAdapter.h / .cpp
  - Scenario/Editor/ScenarioAuthoringSubsystem.h / .cpp
  - ScenarioTemplateSampleJsonTest.cpp
  - ScenarioTemplateSamplerTest.cpp
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
  - Scenario setup/run queue samples in Client/Json/Input are client-owned examples; shared contract truth stays in contracts/specs.
  - Scenario template/sample authoring types stay separate from runtime WorldSpec and actor-spawn payload types.
  - ScenarioSampleWorldSpecAdapter is the thin scenario_sample to runtime WorldSpec boundary; do not fold it into the legacy runtime ScenarioCompiler.
  - ScenarioTemplateSampler owns deterministic scenario_template to scenario_sample generation; keep it separate from the runtime ScenarioCompiler and sample adapter.
  - ScenarioAuthoringSubsystem stores the editor draft as scenario_template and builds runtime WorldSpec only as a preview/compatibility projection.
verify:
  - contract specs vs sample JSON alignment
  - scenario_template/scenario_sample docs vs Client shared schema type alignment
  - ScenarioTemplateJson/ScenarioSampleJson parse, version mismatch, and round-trip automation tests
  - ScenarioTemplateSampler automation tests for layout, robot anchors, fixed obstacle placement, deterministic range sampling, and version mismatch
  - ScenarioSampleWorldSpecAdapter automation tests and OdiroSimEditor build after adapter/editor draft changes
  - focused automation tests for Scenario/Episode changes
  - Client/Task-RunPreview.bat smoke when wrapper supports the changed mode
related:
  - contracts-shared-data
  - client-delivery-bot-policy
  - agents-generation-runtime
---
