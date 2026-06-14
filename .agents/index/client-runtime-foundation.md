---
id: client-runtime-foundation
owner: Client
paths:
  - Client/AGENTS.md
  - Client/ProtoRobotSim.uproject
  - Client/Source/*.Target.cs
  - Client/Source/ProtoRobotSim/ProtoRobotSim.Build.cs
  - Client/Config/**
  - Client/Plugins/UEGitPlugin/**
  - Client/Content/**
entry:
  - Client/AGENTS.md
  - Client/ProtoRobotSim.uproject
  - Client/Plugins/UEGitPlugin/GitSourceControl.uplugin
  - Client/Source/ProtoRobotSim.Target.cs
  - Client/Source/ProtoRobotSimEditor.Target.cs
  - Client/Source/ProtoRobotSim/ProtoRobotSim.Build.cs
  - Client/Config/Default*.ini
  - Client/Content/Maps
  - Client/Content/Blueprints
keep:
  - Unreal-specific config/assets stay in Client.
  - Add module dependencies only at the public/private boundary that needs them.
  - ProjectBorealis UEGitPlugin is used for Editor checkout only; do not initialize `filter=lfs` attributes from the plugin UI.
verify:
  - smallest affected Unreal target build
  - source control provider loads as Git LFS 2 in Editor
  - Blueprint compile/save for asset binding changes
  - separate PIE logs from packaged logs for packaging issues
related:
  - client-simulation
  - client-platform-execution
---
