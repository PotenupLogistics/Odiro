---
id: client-delivery-bot-policy
owner: Client
paths:
  - Client/Source/OdiroSim/Public/DeliveryBot/**
  - Client/Source/OdiroSim/Private/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Struct/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Types/DeliveryBot*.h
  - Client/Json/Input/PolicySpecs/**
entry:
  - DeliveryBot_GridSubsystem.h / .cpp
  - DeliveryBot_DriveComponent.h / .cpp
  - DeliveryBot_HttpPolicyComponent.h / .cpp
  - DeliveryBot_LidarSensorComponent.h / .cpp
  - DeliveryBotPythonDeveloperSettings.h / .cpp
  - DeliveryBotPythonProcessSubsystem.h / .cpp
  - DeliveryBotPythonSettings.h
  - Client/Json/Input/PolicySpecs
keep:
  - Prefer focused shared structs over direct DeliveryBot to Scenario/Episode header coupling.
  - Python policy process ownership lives in DeliveryBot Python subsystem/settings, not legacy root batch files.
verify:
  - DeliveryBot automation tests for component changes
  - policy request/response contract for HTTP policy fields
  - runtime movement/pathing smoke for behavior changes
related:
  - client-simulation
  - contracts-shared-data
  - agents-policy-rag-data
---
