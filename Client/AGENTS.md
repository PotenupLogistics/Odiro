## Repository
- Asset edits: `.uasset` and `.umap` through editor, commandlet, or project scripts
- Build: `Task-Build.bat`
- PIE Preview: `Task-RunPreview.bat`
- Python Policy Server: `Task-RunPythonPolicyServer.bat`
- Commands: no hardcoded local UE install paths

## MCP
- Reuse an open `Client/OdiroSim.uproject` editor when possible; launch it only when MCP needs an editor-backed server
- Treat early `connection refused` as editor/MCP startup state
- Use `UmgMcp` for UMG writes and `ue-mcp` for PIE/runtime screenshots, logs, build status, and screen-debug work
- Do not use `ue-mcp` UI write actions unless explicitly requested
- After MCP plugin source changes, rebuild/restart the editor before runtime verification
- If the agent launched an editor only for MCP, close or reuse it before C++ edits, builds, or another editor launch unless the user wants it open

## Implementation
- Prefer Component/Interface composition; separate Actor, Component, and Subsystem concerns
- Prefer events/timers over Tick
- Expose tunables via `UPROPERTY(EditAnywhere)` or `BlueprintReadOnly`
- Use `IsValid()` at UObject boundaries because callbacks may deliver pending-kill pointers
- Do not rename reflected symbols without a migration plan

## Naming
- Class/member/function: PascalCase, e.g. `class Apple`, `void SetDead()`, `float Hp = 0.f;`
- Parameter/local: camelCase, e.g. `float hp`, `float damageValue`
- Bool: `bDead` for state, `IsDead()` for query

## Artifacts
- Plan files: `Docs/plans/<title>.md`
