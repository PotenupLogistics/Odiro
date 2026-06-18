---
id: client-delivery-bot-policy
owner: Client
paths:
  - Client/Source/OdiroSim/Public/DeliveryBot/**
  - Client/Source/OdiroSim/Private/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Struct/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Types/DeliveryBot*.h
  - Client/Docs/Python_Delivery_JsonFlow.md
  - Client/Resources/policy-runtime.py
  - Client/Tools/PythonAgent/**
  - Client/Json/Input/PolicySpecs/**
  - static/project-templates/**/policy/**
entry:
  - DeliveryBot_GridSubsystem.h / .cpp
  - DeliveryBot_DriveComponent.h / .cpp
  - DeliveryBot_HttpPolicyComponent.h / .cpp
  - DeliveryBot_LidarSensorComponent.h / .cpp
  - DeliveryBotSetupCompiler.h / .cpp
  - DeliveryBotPythonDeveloperSettings.h / .cpp
  - DeliveryBotPythonProcessSubsystem.h / .cpp
  - DeliveryBotPythonSettings.h
  - Client/Docs/Python_Delivery_JsonFlow.md
  - Client/Resources/policy-runtime.py
  - Client/Tools/PythonAgent
  - Client/Json/Input/PolicySpecs
  - static/project-templates/<TemplateId>/policy/__init__.py
keep:
  - Prefer focused shared structs over direct DeliveryBot to Scenario/Episode header coupling.
  - Python policy process ownership lives in DeliveryBot Python subsystem/settings, not legacy root batch files.
  - Python runtime source is Client/Resources/policy-runtime.py; project policy code lives under `<UserProject>/policy/` and run snapshots.
  - Project run command line `-PolicyPort` overrides the Python runtime port before policy lifecycle starts.
  - Project policy entrypoint is `policy/__init__.py:create_policy`; do not introduce a `policy/agent` entrypoint.
  - `simulation_profile.robot.drive/lidar` fields compile through DeliveryBotSetupCompiler aliases until legacy DeliveryBotSetup is removed.
  - Project run policy decide results append to `runs/<RunId>/episodes/<EpisodeId>/actions.jsonl` from `DeliveryBot_HttpPolicyComponent`.
verify:
  - DeliveryBot automation tests for component changes
  - policy request/response contract for HTTP policy fields
  - `OdiroSim.UserProjectData.RobotAction.Write` after action JSONL contract changes
  - user-project scenario_sample adapter test when profile field aliases change
  - project template policy entrypoint smoke when template policy changes
  - runtime movement/pathing smoke for behavior changes
related:
  - client-simulation
  - contracts-shared-data
  - agents-policy-rag-data
---
