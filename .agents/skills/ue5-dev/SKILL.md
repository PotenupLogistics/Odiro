---
name: ue5-dev
description: UE5 C++, Blueprint, asset, build, packaging, and log triage
---

# UE5 Dev

## Inspect
- Project: `*.uproject`, `Source/*.Target.cs`, `Source/**/*.Build.cs`
- C++: touched `Public/` and `Private/` headers, export macro, `GENERATED_BODY()`, reflected fields
- Blueprint: `/Game/...` asset path, target graph
- Packaging: default map, maps-to-cook, enabled plugins, packaged log

## Module Rules
- Public dependency: public header exposes module type
- Private dependency: implementation-only module use
- Runtime module: no editor module dependency
- Include cycle: move concrete includes from `.h` to `.cpp`, forward declare where possible

## Commands
- Use repo-relative project references such as `*.uproject`.
- If local UE path or build command is unknown, ask the user to run the build.

## Build
- Before Client C++ verification, inspect active MCP tools and editor process/window state. If the editor is open and Editor Reload MCP is available, use Reload MCP before any build wrapper.
- Closed editor or Reload MCP unavailable/failed: run the affected build: Client-only `Client/Tools/Build.ps1`; Agents-only `Agents/tools/build.ps1`; two or more project areas `task-build.bat`.
- Open editor with Reload MCP: run `editor_reload_hot_reload` first for Live Coding-compatible C++ changes. If Hot Reload fails, the change needs full reload, or the editor build lock remains, use `editor_reload_rebuild_and_restart` with `wait=true` and then `editor_reload_wait_for_job` only when a non-waiting job was accepted. Treat `success=true`, `skipped=true` (`already_loaded`, `already_included_by_existing_job`, `already_included_by_existing_compile`, `source_up_to_date`) as covered unless the task explicitly requires a forced lifecycle restart. If Reload MCP is unavailable, trigger Live Coding once, retry the same build once, then use the approved build-triggered shutdown path below only if needed.
- Memory exhaustion: retry once with `-MaxParallelActions=4` through the build invocation or supported local override. Do not edit build scripts to set it; do not reduce it to 1.

## Editor Coordination
- Multiple Codex sessions may share one Unreal project; never open an additional editor instance. Open, close, restart, or relaunch the shared editor only with explicit user permission or the Editor Reload MCP coordination path.
- Root `task-setup.bat` installs local `odiro_ue_bridge` and `odiro_editor_reload` MCP entries for supported agents; start a new agent session after setup if tools are missing.
- Use normal `UE_MCP_Bridge` tools for Unreal work. Use Editor Reload MCP only for lifecycle/status/recovery around Live Coding, rebuild, and restart.
- For simultaneous reload/build requests, do not decide manually whether a second build is needed after waiting; rely on Editor Reload MCP's source fingerprint and UBT check-only skip result. Use `force=true` only when no source changed but the editor lifecycle still must restart.
- Do not use direct `hot_reload` or `live_coding_compile` for agent C++ verification when Editor Reload MCP is exposed; those paths do not own source fingerprint coverage.
- If a normal `UE_MCP_Bridge` call returns `maintenance_pending`, stop editor-backed work and wait via Editor Reload MCP instead of retrying the gated call.
- If `UE_MCP_Bridge` disconnects during C++ work, first check Editor Reload MCP status/recovery. When the editor process is still alive, do not force-kill it unless the user explicitly approves unsafe recovery.
- If Editor Reload MCP reports `crash_report_pending`, stop editor launch/restart attempts and close or ask the user to close the Unreal Crash Report window before retrying recover. A `CrashReportClientEditor` process with `-MONITOR=<live editor pid>` is a normal active monitor and should not be treated as a crash dialog.
- Before an approved shutdown, compile and save unsaved assets. If `Unsaved Changes` appears, save; if `Restore Unsaved Changes` appears on relaunch, ignore it.
- During approved editor automation, if the editor exits unexpectedly, wait 3 seconds and check for another Codex-requested build. If none is running, run the affected build, then relaunch.
- The user's approval covers relaunching the shared editor for required MCP/editor validation after widget edits or build-triggered shutdowns. Outside that scope, ask before opening, closing, or restarting the editor.
- Before searching for or calling MCP tools, check for a running Unreal Editor process and, when practical, a visible editor window. If no editor process/window is present and the task needs MCP/editor validation, launch the shared editor instead of ending on missing MCP tools.
- After launching or relaunching for MCP, wait up to 180 seconds for editor startup and bridge/plugin discovery, then retry MCP discovery once. If discovery or an MCP call fails because tools are missing or disconnected, re-check editor state and apply the same launch-and-retry path at most once per task session before reporting failure.
- If MCP discovery still fails after the editor launch/retry path, check that the MCP plugin is enabled in the project when practical, then report the editor process/window state, launch command, wait duration, plugin state, and bridge/tool discovery error.

## Widget Boundary
- Prefer Widget Blueprint for design: layout, hover/focus responses, and animations.
- Put C++ only in non-design logic or required C++ API; skip a C++ widget class when Widget Blueprint is enough.
- Put application logic in Subsystem, ActorComponent, or ViewModel; prefer `UMVVMViewModelBase` for ViewModel state.

## Blueprint Boundary
- UMG asset edits: use MCP for inspection, authoring, compile/save, and readback; avoid commandlets unless the user explicitly requests them.
- Non-UMG Blueprint asset edits: prefer editor/MCP automation when available; if another approved automation path is used, verify touched assets with MCP readback.
- EventGraph edits: do not author directly unless project automation explicitly targets the exact graph change
- Manual handoff: editor steps, nodes/events, property values, expected wiring, and verification

## Verify
- Blueprint: compile result and duplicate event/input nodes
- Packaging: packaged log separate from PIE log
- Do not bypass build failures with alternate UE path, copied workspace, clean/delete of `Binaries` or `Intermediate`, packaging command, IDE build, or generated project refresh.
