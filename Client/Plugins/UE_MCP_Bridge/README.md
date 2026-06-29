# UE_MCP_Bridge

`UE_MCP_Bridge` exposes Unreal Editor tools from the editor process. The rich bridge stays editor-owned, while Live Coding and editor lifecycle work are handled by a separate PowerShell stdio MCP server.

## Surfaces

- `UE_MCP_Bridge`: editor-backed WebSocket JSON-RPC bridge for UMG, assets, runtime capture, logs, build status, and normal Unreal automation.
- `EditorReloadMcp.ps1`: out-of-process stdio MCP server for coordinated Live Coding and editor rebuild/restart requests.

## Runtime State

Shared state lives in `Client/Saved/UE_MCP_Bridge`.

- `port.json`: written by the editor bridge. Reload tools use it to locate the active editor bridge.
- `maintenance.json`: sentinel written before maintenance. Normal bridge calls return `maintenance_pending` while it exists; `coordination_*` bridge calls remain available.
- `jobs/<jobId>.json`: reload job progress and terminal state.
- `last-success.json`: latest completed reload/build source fingerprint used to skip duplicate requests.
- `checks/*-outdated-actions.json`: UBT check-only action exports used to decide whether C++ work is already up to date.
- `editor.lock` and `state.lock`: file locks used by per-session reload MCP processes.

## Reload MCP

Use this command as an MCP stdio server:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Client\Plugins\UE_MCP_Bridge\Resources\MCP\EditorReloadMcp.ps1
```

Exposed tools:

- `editor_reload_get_status`: reports sentinel, bridge, editor pid, and job state.
- `editor_reload_check_up_to_date`: runs UBT `-WriteOutdatedActions` and reports whether the editor target has stale actions without compiling.
- `editor_reload_hot_reload`: waits for an existing reload job, rechecks source state, skips if already covered or up to date, otherwise writes the sentinel, runs Live Coding through a coordination handler, then removes the sentinel.
- `editor_reload_rebuild_and_restart`: with `wait=true`, waits for an existing reload job, rechecks source state, skips if already covered or up to date, otherwise writes the sentinel, waits for active bridge work to drain, saves dirty packages, asks the editor to exit, rebuilds, restarts, then removes the sentinel.
- `editor_reload_wait_for_job`: waits for an accepted job to complete or fail.
- `editor_reload_recover`: inspects recovery state without killing a live editor.

`editor_reload_hot_reload` and `editor_reload_rebuild_and_restart` accept `force=true` to bypass duplicate/up-to-date skipping when a lifecycle restart is required even without source changes. Skipped requests return `success=true`, `accepted=false`, `skipped=true`, and a stable `code` such as `already_included_by_existing_job`, `already_loaded`, `already_built`, or `source_up_to_date`.

If UBT check-only is blocked because Live Coding is active, the check result uses `upToDateCheck.code = live_coding_active`. Reload tools treat that as inconclusive and continue with the requested coordinated Live Coding or rebuild/restart instead of skipping.

The helper script is `Resources/Automation/RebuildAndRestart.ps1`. It is launched detached so the reload job can continue after the editor process and its bridge disappear. Reload scripts resolve `.uproject`, `Build.bat`, and `UnrealEditor.exe` through the plugin-local `Resources/Automation/UnrealProjectTools.ps1`; do not source project tool scripts such as `Client/Tools/Common.ps1` from this plugin surface.

## Coordination Handlers

The editor bridge keeps a small coordination surface available during maintenance:

- `coordination_get_status`
- `coordination_prepare_maintenance`
- `coordination_save_dirty`
- `coordination_live_coding_compile`
- `coordination_request_exit`

All non-coordination bridge requests are counted while executing. Once `maintenance.json` exists, new non-coordination requests fail fast with `code = maintenance_pending`, allowing active sessions to stop issuing editor work instead of racing Live Coding or shutdown.

`maintenance.json` includes `operation`, currently `live_coding` or `rebuild_restart`.
