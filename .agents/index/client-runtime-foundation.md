---
id: client-runtime-foundation
owner: Client
paths:
  - Client/AGENTS.md
  - Client/OdiroSim.uproject
  - Client/Source/*.Target.cs
  - Client/Source/OdiroSim/OdiroSim.Build.cs
  - Client/Config/**
  - Client/Plugins/UEGitPlugin/**
  - Client/Content/**
entry:
  - Client/AGENTS.md
  - Client/OdiroSim.uproject
  - Client/Plugins/UEGitPlugin/GitSourceControl.uplugin
  - Client/Source/OdiroSim.Target.cs
  - Client/Source/OdiroSimEditor.Target.cs
  - Client/Source/OdiroSim/OdiroSim.Build.cs
  - Client/Config/Default*.ini
  - Client/Content/Maps
  - Client/Content/Blueprints
  - Client/Content/Widgets/MainMenu/WBP_StartupMenu
  - Client/Content/Widgets/MainMenu/WBP_MainMenu
  - Client/Content/Widgets/MainMenu/WBP_ProjectTemplateCard
  - Client/Content/Widgets/MainMenu/WBP_ProjectWorkspaceTab
  - Client/Content/Widgets/MainMenu/WBP_ProjectRunMetricCard
  - Client/Content/Widgets/MainMenu/WBP_ProjectEpisodeReplayCard
  - Client/Content/Widgets/MainMenu/WBP_ProjectAiSuggestionRow
  - Client/Content/Widgets/Editor/WBP_ScenarioEditorRootWidget
  - Client/Content/Maps/StartupMap
  - Client/Content/Blueprints/Startup/BP_StartupMenuBootstrap
keep:
  - Unreal-specific config/assets stay in Client.
  - Add module dependencies only at the public/private boundary that needs them.
  - DefaultEngine.ini starts in `/Game/Maps/StartupMap.StartupMap` with Engine GameModeBase; Startup UI is attached by the map-owned startup bootstrap actor.
  - StartupMap is a project selection/creation shell; ScenarioEditorMap owns WBP_MainMenu and editor workspace UI.
  - WBP_MainMenu must expose ProjectWorkspaceScreen as its root surface and include ScenarioEditorRootWidget under ProjectScenarioEditPanel; recent project selection/add button, recent-list removal dialog, folder browse, preset dropdowns, and preset-composition project creation widgets belong to WBP_StartupMenu.
  - WBP_ProjectWorkspaceTab owns the visual layout for project workspace tabs; C++ only drives label, active/visible/closable state, and click routing.
  - WBP_ProjectRunMetricCard, WBP_ProjectEpisodeReplayCard, and WBP_ProjectAiSuggestionRow own project result dashboard card/row layout and default visuals; C++ updates named child text and visibility only.
  - WBP_ScenarioEditorRootWidget owns Scenario editor right Sidebar layout and must expose SaveButton, SaveStatusText, ScenarioEditorOutlinerWidget, InspectorSwitcher, DetailInspectorPanel, LlmInspectorPanel, TemplateSidebarPanel, PlaceableContextMenuPanel, and Detail/LLM tab buttons. ToolbarWidget must not remain in the widget tree.
  - StartupMenu/MainMenu asset edits are made and verified through UmgMcp; do not rely on runtime fallback to move ProjectOpenScreen or ScenarioEditorRootWidget.
  - ProjectBorealis UEGitPlugin is used for Editor checkout only; do not initialize `filter=lfs` attributes from the plugin UI.
  - DefaultEditorPerProjectUserSettings.ini defaults asset-modification checkout prompts; tools/set-git-config.ps1 corrects existing local Saved settings.
verify:
  - smallest affected Unreal target build
  - source control provider loads as Git LFS 2 in Editor
  - UmgMcp get_widget_tree after StartupMenu/MainMenu/ProjectWorkspaceTab/project result dashboard/ScenarioEditor root UMG structure edits, including `WBP_StartupMenu.RecentProjectAddButton`
  - Blueprint compile/save for StartupMap, WBP_StartupMenu, WBP_MainMenu, WBP_ProjectTemplateCard, WBP_ProjectWorkspaceTab, WBP_ProjectRunMetricCard, WBP_ProjectEpisodeReplayCard, WBP_ProjectAiSuggestionRow, WBP_ScenarioEditorRootWidget, and ScenarioEditorMap binding changes
  - separate PIE logs from packaged logs for packaging issues
related:
  - client-simulation
  - client-platform-execution
---
