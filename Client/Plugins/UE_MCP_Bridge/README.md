# UE_MCP_Bridge

`UE_MCP_Bridge` exposes Unreal Editor tools from the editor process. The rich bridge stays editor-owned, while Live Coding and editor lifecycle work are handled by a separate PowerShell stdio MCP server.

## Surfaces

- `UE_MCP_Bridge`: editor-backed WebSocket JSON-RPC bridge for UMG, assets, runtime capture, logs, build status, and normal Unreal automation.
- `BridgeMcp.ps1`: stdio MCP proxy that exposes the editor bridge to local agents and discovers C++ bridge handlers as tools.
- `EditorReloadMcp.ps1`: out-of-process stdio MCP server for coordinated Live Coding and editor rebuild/restart requests.

## Agent Setup

Run root `task-setup.bat` to install local MCP entries for detected agents. The setup installs both servers:

- `odiro_ue_bridge`: `Resources/MCP/BridgeMcp.ps1`
- `odiro_editor_reload`: `Resources/MCP/EditorReloadMcp.ps1`

Codex is installed to user scope because Codex loads MCP servers from `$CODEX_HOME/config.toml`, or `%USERPROFILE%\.codex\config.toml` when `CODEX_HOME` is unset.

Repo-local generated config files are machine-local and ignored by Git because they contain absolute script paths:

- `.mcp.json` for Claude Code
- `.cursor/mcp.json` for Cursor
- `.vscode/mcp.json` for VS Code Copilot

Use `task-setup.bat -SkipAgentMcp` when only the repository setup is needed. Start a new agent session after setup if MCP tools were not already loaded.

## Runtime State

Shared state lives in `Client/Saved/UE_MCP_Bridge`.

- `port.json`: written by the editor bridge. Reload tools use it to locate the active editor bridge.
- `maintenance.json`: sentinel written before maintenance. Normal bridge calls return `maintenance_pending` while it exists; `coordination_*` bridge calls remain available. Live Coding jobs include `singleFlight=true`.
- `jobs/<jobId>.json`: reload job progress and terminal state. Live Coding join paths may include `joinReason`, `sourceFingerprintBeforeJoin`, `sourceFingerprintAfterJoin`, and `upToDateCheckAfterJoin`.
- `last-success.json`: latest completed reload/build source fingerprint used to skip duplicate requests.
- `checks/*-outdated-actions.json`: UBT check-only action exports used to decide whether C++ work is already up to date.
- `editor.lock` and `state.lock`: file locks used by per-session reload MCP processes.

Terminal maintenance phases (`completed`, `failed`, `restart_failed`, `port_timeout`, `editor_crashed`, `modal_blocked`, `crash_report_pending`) are recoverable state, not an active editor gate. Use `editor_reload_recover` to clear stale terminal maintenance or stale `port.json`; do not remove state files manually during an active job.

Bridge status classifies `port.json` as `missing`, `stale_pid`, `process_alive_but_bridge_down`, or `ready` so agents can report the actual failure instead of guessing that the plugin is missing.

Crash reporter state is reported separately. A `CrashReportClientEditor` process with `-MONITOR=<live editor pid>` is a normal active monitor and does not block work. A reporter without a live monitor, or one still showing a report dialog for a dead editor, is reported as `crash_report_pending`; close or dismiss the Crash Report window before starting another editor.

## Bridge MCP

Use this command as the normal editor bridge MCP stdio server:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Client\Plugins\UE_MCP_Bridge\Resources\MCP\BridgeMcp.ps1
```

The server reads `Saved/UE_MCP_Bridge/port.json` and forwards requests to the active editor bridge. It exposes `ue_bridge_get_status`, `ue_bridge_call`, and discovered C++ bridge handlers as MCP tools.

## Reload MCP

Use this command as an MCP stdio server:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Client\Plugins\UE_MCP_Bridge\Resources\MCP\EditorReloadMcp.ps1
```

Exposed tools:

- `editor_reload_get_status`: reports sentinel, bridge, editor pid, and job state.
- `editor_reload_check_up_to_date`: runs UBT `-WriteOutdatedActions` and reports whether the editor target has stale actions without compiling.
- `editor_reload_hot_reload`: waits for an existing reload job or active Live Coding compile, rechecks source state, skips if already covered or up to date, otherwise writes the sentinel, runs Live Coding through a coordination handler, then removes the sentinel.
- `editor_reload_rebuild_and_restart`: with `wait=true`, waits for an existing reload job, rechecks source state, skips if already covered or up to date, otherwise writes the sentinel, waits for active bridge work to drain, saves dirty packages, asks the editor to exit, rebuilds, restarts, then removes the sentinel.
- `editor_reload_wait_for_job`: waits for an accepted job to complete or fail.
- `editor_reload_recover`: inspects recovery state without killing a live editor.

`editor_reload_hot_reload` and `editor_reload_rebuild_and_restart` accept `force=true` to bypass duplicate/up-to-date skipping when a lifecycle restart is required even without source changes. Skipped requests return `success=true`, `accepted=false`, `skipped=true`, and a stable `code` such as `already_included_by_existing_job`, `already_included_by_existing_compile`, `already_loaded`, `already_built`, or `source_up_to_date`.

`editor_reload_rebuild_and_restart` also accepts `editorArgs` and `mcpSafeLaunch`. The safe launch profile adds `-DDC=InstalledNoZenLocalFallback -d3d11 -noraytracing` on top of the default `-NoSplash`; use it for MCP validation when the normal DX12/SM6/ray tracing editor path crashes before the bridge starts.

If UBT check-only is blocked because Live Coding is active, the check result uses `upToDateCheck.code = live_coding_active`. `editor_reload_hot_reload` checks `liveCoding.compiling`: when a compile is active it waits, reruns the freshness check, and can skip with `already_included_by_existing_compile`; when Live Coding is merely enabled but idle, it proceeds with the coordinated compile. It does not start another Live Coding compile while `liveCoding.compiling=true`.

If a previous editor crash left Crash Report Client open, reload status and recover return `code = crash_report_pending`. Rebuild/restart and direct `Client/Tools/Dev.ps1` launch refuse to start another editor until the pending Crash Report window is closed. The tools do not kill Crash Report Client automatically.

The helper script is `Resources/Automation/RebuildAndRestart.ps1`. It is launched detached so the reload job can continue after the editor process and its bridge disappear. Reload scripts resolve `.uproject`, `Build.bat`, and `UnrealEditor.exe` through the plugin-local `Resources/Automation/UnrealProjectTools.ps1`; do not source project tool scripts such as `Client/Tools/Common.ps1` from this plugin surface.

## Coordination Handlers

The editor bridge keeps a small coordination surface available during maintenance:

- `coordination_get_status`
- `coordination_prepare_maintenance`
- `coordination_save_dirty`
- `coordination_live_coding_compile`
- `coordination_request_exit`

All non-coordination bridge requests are counted while executing. Once nonterminal `maintenance.json` exists, new non-coordination requests fail fast with `code = maintenance_pending`, allowing active sessions to stop issuing editor work instead of racing Live Coding or shutdown.

`coordination_get_status` includes additive `liveCoding` fields: `available`, `enabledByDefault`, `enabledForSession`, `canEnableForSession`, `started`, `compiling`, and `enableError`. `coordination_live_coding_compile` can return `result=already_compiling`; Reload MCP treats that as a join/wait/recheck state, not as permission to start a duplicate compile.

`maintenance.json` includes `operation`, currently `live_coding` or `rebuild_restart`.

Direct bridge handlers such as `hot_reload` and `live_coding_compile` remain available for manual diagnostics, but agent C++ verification should use `editor_reload_hot_reload` so source fingerprint coverage and duplicate suppression stay centralized.

Known diagnostic non-failures: WBP_BaseIcon can capture blank when no default icon content is configured, WBP_BaseSwitch.SwitchRoot may keep fixed internal overrides, XGE license warnings indicate standalone build mode, and XAudio2 device warnings are unrelated unless audio behavior is under test.
