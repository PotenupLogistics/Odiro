## Repository
- Asset edits: write with commandlet first, then verify touched assets with UE_MCP_Bridge/MCP readback; MCP may also be used earlier for inspection or authoring
- Automation: build `..\task-build.bat client` only when the editor is closed or Reload MCP is unavailable/failed; PIE preview `..\task-run.bat -SkipAgents -SkipBridge -- ...`
- Commands: no hardcoded local UE install paths

## MCP
- MCP is exposed as active Codex tools, not as repo files or shell commands.
- Root `task-setup.bat` installs local MCP configs for supported agents, including `odiro_ue_bridge` and `odiro_editor_reload`; start a new agent session after setup if tools are missing.
- Before the first Unreal MCP call in a session, inspect active tools or use `tool_search` when available for `UE_MCP_Bridge`, `Unreal`, or `MCP`; match by capability, not namespace text, and do not guess server names.
  - `UE_MCP_Bridge`: UMG writes, widget capture/runtime geometry, PIE/runtime screenshots, logs, build status, and screen-debug work
- If Unreal MCP tools are not exposed in the current tool list, state that MCP verification is unavailable instead of inventing `mcp__...` calls.
- Before any Client C++ verification, check active tools, then editor process/window state. If the editor is open and Editor Reload MCP is exposed, use Reload MCP before any `Build.ps1` or `task-build.bat client` invocation.
- Reuse an open editor when possible; launch it only when MCP needs an editor-backed server
- If the agent launched an editor only for MCP, close or reuse it before C++ edits, builds, or another editor launch unless the user wants it open
- Treat early `connection refused` as editor/MCP startup state
- Use the Editor Reload MCP, when exposed, for coordinated Live Coding, editor build locks, `maintenance_pending`, bridge disconnect during C++ work, or required MCP plugin reload. Try `editor_reload_hot_reload` before rebuild/restart when the change is Live Coding-compatible.
- For concurrent C++ reload/build requests, let Editor Reload MCP do the wait/recheck/skip flow. Treat `success=true`, `skipped=true` results such as `already_loaded`, `already_included_by_existing_job`, `already_included_by_existing_compile`, or `source_up_to_date` as covered; pass `force=true` only when a lifecycle restart is required even without source changes.
- Do not use direct `hot_reload` or `live_coding_compile` for agent C++ verification when Editor Reload MCP is exposed; those paths do not own source fingerprint coverage.
- If a normal `UE_MCP_Bridge` call returns `maintenance_pending`, stop editor-backed work and use Editor Reload MCP status/job wait tools instead of retrying the gated call.
- If `UE_MCP_Bridge` is unreachable but the editor process is still alive, use Editor Reload MCP `editor_reload_get_status` or `editor_reload_recover`; do not force-kill or restart unless the user explicitly approves unsafe recovery.
- If Reload MCP reports `crash_report_pending`, do not launch another editor. Close or ask the user to close the Unreal Crash Report window, then retry `editor_reload_recover`; a `CrashReportClientEditor` with `-MONITOR=<live editor pid>` is a normal monitor and is not a failure.
- After MCP plugin source changes or changes that Live Coding cannot apply, rebuild/restart the editor through Editor Reload MCP before runtime verification when that MCP is available

## Implementation
- Prefer Component/Interface composition; separate Actor, Component, and Subsystem concerns
- Prefer events/timers over Tick
- Expose tunables via `UPROPERTY(EditAnywhere)` or `BlueprintReadOnly`
- Use `IsValid()` at UObject boundaries because callbacks may deliver pending-kill pointers
- Do not rename reflected symbols without a migration plan
- UMG: layout, styling, and visual property values live in Widget Blueprint assets; C++ exposes data/events and owns logic only
- Dynamic or repeated UMG patterns: create a reusable Widget Blueprint per item or pattern and instantiate it from C++; do not copy widget trees or build the visual tree in C++

## Naming
- Class/member/function: PascalCase, e.g. `class Apple`, `void SetDead()`, `float Hp = 0.f;`
- Parameter/local: camelCase, e.g. `float hp`, `float damageValue`
- Bool: `bDead` for state, `IsDead()` for query

## Artifacts
- Plan files: `Docs/plans/<title>.md`
