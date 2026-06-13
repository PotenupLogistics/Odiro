---
name: ue5-dev
description: UE5 C++ Blueprint build and log checks
---

# UE5 Dev

UE5 C++, Blueprint, asset, build, packaging, and log triage.

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

## Blueprint Boundary
- Commandlet OK: asset creation, parent class assignment, default property edits, compile/save
- EventGraph edits: do not author directly unless a project script owns the exact graph change
- Manual handoff: editor steps, nodes/events, property values, expected wiring, and verification

## Verify
- Build: smallest affected editor target
- Blueprint: compile result and duplicate event/input nodes
- Packaging: packaged log separate from PIE log

## Build Failure Boundary
- Live Coding active, editor lock, mutex, locked binaries, or missing local UE path: stop and report
- Live Coding Start: trigger once only through an existing local command or hook
- Do not bypass with alternate UE path, copied workspace, clean/delete of `Binaries` or `Intermediate`, packaging command, IDE build, or generated project refresh
- Otherwise ask the user to close the editor or disable Live Coding, then run their local build command
