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
- After work, run the affected build: Client-only `Client/Tools/Build.ps1`; Agents-only `Agents/tools/build.ps1`; two or more project areas `task-build.bat`.
- Live Coding/editor-lock failure: if the editor is open, trigger Live Coding once, then retry the same build once. If Live Coding cannot be triggered or the retry fails, stop and report.
- Memory exhaustion: retry once with `-MaxParallelActions=4` through the build invocation or supported local override. Do not edit build scripts to set it; do not reduce it to 1.

## Editor Coordination
- Multiple Codex sessions may share one Unreal project; never open an additional editor instance. Open, close, restart, or relaunch the shared editor only with explicit user permission.
- Before an approved shutdown, compile and save unsaved assets. If `Unsaved Changes` appears, save; if `Restore Unsaved Changes` appears on relaunch, ignore it.
- During approved editor automation, if the editor exits unexpectedly, wait 3 seconds and check for another Codex-requested build. If none is running, run the affected build, then relaunch.

## Blueprint Boundary
- Asset edits: write with commandlet first, then verify touched assets with UE_MCP_Bridge/MCP readback. MCP may also be used earlier for inspection or authoring.
- EventGraph edits: do not author directly unless project automation explicitly targets the exact graph change
- Manual handoff: editor steps, nodes/events, property values, expected wiring, and verification

## Verify
- Blueprint: compile result and duplicate event/input nodes
- Packaging: packaged log separate from PIE log
- Do not bypass build failures with alternate UE path, copied workspace, clean/delete of `Binaries` or `Intermediate`, packaging command, IDE build, or generated project refresh.
