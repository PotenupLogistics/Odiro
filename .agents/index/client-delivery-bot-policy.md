---
id: client-delivery-bot-policy
owner: Client
paths:
  - Client/Source/OdiroSim/Public/DeliveryBot/**
  - Client/Source/OdiroSim/Private/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Struct/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Types/DeliveryBot*.h
  - Client/Tools/PythonAgent/**
  - Client/Json/Input/PolicySpecs/**
entry:
  - DeliveryBot_GridSubsystem.h / .cpp
  - DeliveryBot_DriveComponent.h / .cpp
  - DeliveryBot_HttpPolicyComponent.h / .cpp
  - DeliveryBot_LidarSensorComponent.h / .cpp
  - DeliveryBotSetupCompiler.h / .cpp
  - DeliveryBotPythonDeveloperSettings.h / .cpp
  - DeliveryBotPythonProcessSubsystem.h / .cpp
  - DeliveryBotPythonSettings.h
  - Client/Tools/PythonAgent/agent/user_agent.py
  - Client/Tools/PythonAgent/agent/lidar_selector.py
  - Client/Tools/PythonAgent/agent/lidar_point_cloud.py
  - Client/Tools/PythonAgent/templates
  - Client/Tools/PythonAgent/tools/reset_user_code.py
  - Client/Json/Input/PolicySpecs
keep:
  - Prefer focused shared structs over direct DeliveryBot to Scenario/Episode header coupling.
  - Python policy process ownership lives in DeliveryBot Python subsystem/settings, not legacy root batch files.
  - Keep Unreal-only debug/log toggles on components or developer settings instead of user-facing DeliveryBotSetup JSON.
  - Keep Python-side LiDAR Point Cloud support user-selectable through observation profiles, not mandatory policy behavior.
  - Prefer Unreal raycast hit locations for Point Cloud export when the request payload provides them.
  - Use LiDAR component hit-location debug before diagnosing Point Cloud import alignment issues.
  - Keep Python reset tools template-based with backups; do not use git history operations for user-code restore.
verify:
  - DeliveryBot automation tests for component changes
  - policy request/response contract for HTTP policy fields
  - runtime movement/pathing smoke for behavior changes
  - PythonAgent py_compile for Python policy/observer changes
  - reset_user_code dry-run before template restore changes
related:
  - client-simulation
  - contracts-shared-data
  - agents-policy-rag-data
---
