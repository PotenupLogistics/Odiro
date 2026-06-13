---
id: client-delivery-bot-policy
owner: Client
paths:
  - Client/Source/ProtoRobotSim/Public/DeliveryBot/**
  - Client/Source/ProtoRobotSim/Private/DeliveryBot/**
  - Client/Source/ProtoRobotSim/Public/Shared/Struct/DeliveryBot/**
  - Client/Source/ProtoRobotSim/Public/Shared/Types/DeliveryBot*.h
  - Client/Json/Input/PolicySpecs/**
entry:
  - DeliveryBot_GridSubsystem.h / .cpp
  - DeliveryBot_*Component
  - DeliveryBotSetupCompiler.h / .cpp
  - DeliveryBotPolicyActionType.h
  - Client/Json/Input/PolicySpecs
keep:
  - Prefer focused shared structs over direct DeliveryBot to Scenario/Episode header coupling.
verify:
  - DeliveryBot automation tests for component changes
  - policy request/response contract for HTTP policy fields
  - runtime movement/pathing smoke for behavior changes
related:
  - client-simulation
  - contracts-shared-data
  - agents-policy-rag-data
---
