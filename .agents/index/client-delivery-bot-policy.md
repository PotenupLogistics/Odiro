---
id: client-delivery-bot-policy
owner: Client
paths:
  - Client/Source/OdiroSim/Public/DeliveryBot/**
  - Client/Source/OdiroSim/Private/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Struct/DeliveryBot/**
  - Client/Source/OdiroSim/Public/Shared/Types/DeliveryBot*.h
  - Client/Tools/PythonAgent/**
  - Client/Json/Input/DeliveryBotSetupPlayable*.json
  - Client/Json/Input/PolicySpecs/**
entry:
  - DeliveryBot_GridSubsystem.h / .cpp
  - DeliveryBot_DriveComponent.h / .cpp
  - DeliveryBot_HttpPolicyComponent.h / .cpp
  - DeliveryBot_LidarSensorComponent.h / .cpp
  - DeliveryBotPointCloudReviewActor.h / .cpp
  - DeliveryBotPointCloudCaptureConfigInfo.h
  - DeliveryBotPolicyEventSnapshot.h
  - DeliveryBotSetupCompiler.h / .cpp
  - DeliveryBotPythonDeveloperSettings.h / .cpp
  - DeliveryBotPythonProcessSubsystem.h / .cpp
  - DeliveryBotPythonSettings.h
  - Client/Tools/PythonAgent/agent/user_agent.py
  - Client/Tools/PythonAgent/agent/lidar_selector.py
  - Client/Tools/PythonAgent/agent/lidar_point_cloud.py
  - Client/Tools/PythonAgent/templates
  - Client/Tools/PythonAgent/tools/reset_user_code.py
  - Client/Tools/PythonAgent/tools/validate_lidar_capture.py
  - Client/Docs/Portfolio/04-lidar-point-cloud-implementation.html
  - Client/Json/Input/DeliveryBotSetupPlayable_RealtimePointCloud.json
  - Client/Json/Input/DeliveryBotSetupPlayable_QualityPointCloud.json
  - Client/Json/Input/DeliveryBotSetupPlayable_NoPointCloud.json
  - Client/Json/Input/PolicySpecs
keep:
  - Prefer focused shared structs over direct DeliveryBot to Scenario/Episode header coupling.
  - Python policy process ownership lives in DeliveryBot Python subsystem/settings, not legacy root batch files.
  - Keep Unreal-only debug/log toggles on components or developer settings instead of user-facing DeliveryBotSetup JSON.
  - Runtime scenario grid rebuilds are orchestrated by ScenarioSimulationSubsystem; level-placed GridBoundsActor BeginPlay remains legacy/manual behavior.
  - Keep Python-side LiDAR Point Cloud support user-selectable through observation profiles, not mandatory policy behavior.
  - Prefer Unreal raycast hit locations for Point Cloud export when the request payload provides them.
  - Use the Unreal LiDAR Point Cloud plugin as the primary viewer for saved `map_accumulated.xyz` captures.
  - Keep per-frame point cloud debug files under `captures/lidar_point_cloud/frames/`; do not mix them with official root import files.
  - Keep `capture_summary.json` as saved artifact validation metadata; do not add point-cloud summaries to policy response payloads.
  - Route DeliveryBotSetup `robot.lidar.observation_profile` and `robot.lidar.point_cloud` through `FDeliveryBotPointCloudCaptureConfigInfo` into Python `lidarSpec`.
  - Treat NoPointCloud/basic runs with `capture_enabled=false` as valid when no LiDAR Point Cloud capture folder is created.
  - Use LiDAR component hit-location debug before diagnosing Point Cloud import alignment issues.
  - Keep point cloud capture folders readable with scenario/run ids rather than opaque GUID-only names.
  - Keep DeliveryBotPointCloudReviewActor as a path lookup and small-point debug helper, not the primary point cloud renderer.
  - Keep Python reset tools template-based with backups; do not use git history operations for user-code restore.
  - Keep Python policy event classification in DeliveryBot_HttpPolicyComponent; ScenarioEvaluationSubsystem receives normalized FDeliveryBotPolicyEventSnapshot values and owns episode event publication.
  - Keep Python user_agent.py emitting actual RePath decisions through response.events; C++ should not infer RePath from generic slowdown/path-follow reasons.
verify:
  - DeliveryBot automation tests for component changes
  - policy request/response contract for HTTP policy fields
  - policy server failure, policy failure, and RePath event routing through ScenarioEvaluationSubsystem.OnEvaluationEvent
  - PythonAgent py_compile after changing response.events emission
  - runtime movement/pathing smoke for behavior changes
  - PythonAgent py_compile for Python policy/observer changes
  - validate_lidar_capture for saved LiDAR Point Cloud capture folders
  - reset_user_code dry-run before template restore changes
related:
  - client-simulation
  - contracts-shared-data
  - agents-policy-rag-data
---
