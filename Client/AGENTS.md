## Repository
- Asset edits: write with commandlet first, then verify touched assets with UE_MCP_Bridge/MCP readback; MCP may also be used earlier for inspection or authoring
- Automation: build `..\task-build.bat client`, PIE preview `..\task-run.bat -SkipAgents -SkipBridge -- ...`
- Commands: no hardcoded local UE install paths

## MCP
- MCP is exposed as active Codex tools, not as repo files or shell commands.
- Before the first Unreal MCP call in a session, inspect active tools or use `tool_search` when available for `UE_MCP_Bridge`, `Unreal`, or `MCP`; match by capability, not namespace text, and do not guess server names.
  - `UE_MCP_Bridge`: UMG writes, widget capture/runtime geometry, PIE/runtime screenshots, logs, build status, and screen-debug work
- If Unreal MCP tools are not exposed in the current tool list, state that MCP verification is unavailable instead of inventing `mcp__...` calls.
- Reuse an open editor when possible; launch it only when MCP needs an editor-backed server
- If the agent launched an editor only for MCP, close or reuse it before C++ edits, builds, or another editor launch unless the user wants it open
- Treat early `connection refused` as editor/MCP startup state
- After MCP plugin source changes, rebuild/restart the editor before runtime verification

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
