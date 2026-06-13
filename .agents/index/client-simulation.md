---
id: client-simulation
owner: Client
paths:
  - Client/Source/ProtoRobotSim/Public/Scenario/**
  - Client/Source/ProtoRobotSim/Private/Scenario/**
  - Client/Source/ProtoRobotSim/Public/Episode/**
  - Client/Source/ProtoRobotSim/Private/Episode/**
  - Client/Json/Input/**
  - contracts/specs/**
entry:
  - ScenarioCompiler.h / .cpp
  - ScenarioSimulationSubsystem.h / .cpp
  - ScenarioRunnerSubsystem.h / .cpp
  - EpisodeMeasurementLogSubsystem.h / .cpp
  - EpisodeEvaluationReportJson.h / .cpp
  - Client/Json/Input
  - contracts/specs
keep:
  - Parser-only samples/schemas owned only by Client stay in Client.
verify:
  - contract specs vs sample JSON alignment
  - focused automation tests for Scenario/Episode changes
  - Client/Task-RunPreview.bat smoke when wrapper supports the changed mode
related:
  - contracts-shared-data
  - client-delivery-bot-policy
  - agents-generation-runtime
---
