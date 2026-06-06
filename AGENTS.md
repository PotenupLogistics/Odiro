## Repository
- Context: `README.md`, `.agents/sourcemap/INDEX.md`
- Asset edits: `.uasset` and `.umap` through editor, commandlet, or project scripts
- Commands: no hardcoded local UE install paths

## MCP Routing
- MCP calls are editor-backed. If the editor is not running, launch the project editor for `ProtoRobotSim.uproject` and wait until the target MCP server is listening before calling or declaring MCP unavailable
- `connection refused` before an editor-backed MCP server is ready is setup state, not a fallback condition
- Do not use Unreal commandlet/Python/project scripts as fallback for MCP-routed work until the editor is running and the relevant MCP call still fails
- UMG asset writes, layout edits, UMG animation, and UMG material work: use `UmgMcp` first after editor/MCP readiness
- PIE/runtime inspection, screenshots, logs, editor status, build status, and screen-debug tasks: use `ue-mcp` first after editor/MCP readiness
- `ue-mcp` UI write actions are read-only by default unless the user explicitly asks to edit UI through `ue-mcp`
- After MCP plugin source changes, rebuild/restart the editor before runtime verification

## Language
- Docs/plans: Korean unless a template or external audience requires English

## Implementation
- Dead code: confirm removal; Blueprint/reflection may call code with no C++ callers
- Boundaries: prefer Component/Interface composition; separate Actor, Component, and Subsystem concerns
- Runtime: prefer events/timers over Tick; document ownership/lifecycle when runtime relationships change
- Blueprint: expose tunables via `UPROPERTY(EditAnywhere)` or `BlueprintReadOnly`
- Code: avoid all-public classes; use `IsValid()` at UObject boundaries because callbacks may deliver pending-kill pointers
- Reflected names: do not rename without a migration plan; `.uasset` and saved data embed names

## Naming
- Class/member/function: PascalCase, e.g. `class Apple`, `void SetDead()`, `float Hp{ 0.f }`
- Parameter/local: camelCase, e.g. `float hp`, `float damageValue`
- Bool: `bDead` for state, `IsDead()` for query

## Artifacts
- Plan files: `Docs/plans/PLAN-<title>.md`

## Skills
- Root: `.agents/skills/<name>/SKILL.md`
- `ue5-dev`: UE5 C++/Blueprint/build/log/module checks
