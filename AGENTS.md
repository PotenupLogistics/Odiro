## Hard Boundaries
- No commit, push, publish, history rewrite, recursive delete, or bulk move without explicit request
- Preserve unrelated user changes

## Ownership
- Project-specific content stays in its project by default
- `contracts` is for shared or externally exposed machine interfaces, not project-private schemas
- Minimize file and module dependencies by OO principles; prefer narrow interfaces and explicit ownership over cross-module reach-through
- Update `.agents/index` in the same change when paths, entry points, responsibilities, boundaries, or verification flow change

## Skill Routing
- UE5, Unreal, C++, Blueprint, UMG, module dependency, packaging, PIE, runtime log, or Unreal asset workflow: use `.agents/skills/ue5-dev/SKILL.md`

## Unreal MCP Routing
- Before launching an editor for MCP, check whether `Client/ProtoRobotSim.uproject` is already open and reuse that editor instead of starting another instance
- MCP calls are editor-backed. If the editor is not running, launch `Client/ProtoRobotSim.uproject` and wait until the target MCP server is listening before calling or declaring MCP unavailable
- `connection refused` before an editor-backed MCP server is ready is setup state, not a fallback condition
- Do not use Unreal commandlet/Python/project scripts as fallback for MCP-routed work until the editor is running and the relevant MCP call still fails
- UMG asset writes, layout edits, UMG animation, and UMG material work: use `UmgMcp` first after editor/MCP readiness
- PIE/runtime inspection, screenshots, logs, editor status, build status, and screen-debug tasks: use `ue-mcp` first after editor/MCP readiness
- `ue-mcp` UI write actions are read-only by default unless the user explicitly asks to edit UI through `ue-mcp`
- After MCP plugin source changes, rebuild/restart the editor before runtime verification
- If the agent launched the editor only for MCP, save/verify the asset work and close that editor before C++ edits, C++ builds, or final response unless the user asked to keep it open
- Do not launch another editor while an MCP-launched editor is still running; close or reuse the existing editor first

## Unreal Implementation
- Dead code: confirm removal; Blueprint/reflection may call code with no C++ callers
- Boundaries: prefer Component/Interface composition; separate Actor, Component, and Subsystem concerns
- Runtime: prefer events/timers over Tick; document ownership/lifecycle when runtime relationships change
- Blueprint: expose tunables via `UPROPERTY(EditAnywhere)` or `BlueprintReadOnly`
- Code: avoid all-public classes; use `IsValid()` at UObject boundaries because callbacks may deliver pending-kill pointers
- Reflected names: do not rename without a migration plan; `.uasset` and saved data embed names
