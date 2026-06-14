## Hard Boundaries
- No commit, push, publish, history rewrite, recursive delete, or bulk move without explicit request
- Preserve unrelated user changes

## Ownership
- Project-specific content stays in its project by default
- `contracts` is for shared or externally exposed machine interfaces, not project-private schemas
- Minimize file and module dependencies by OO principles; prefer narrow interfaces and explicit ownership over cross-module reach-through
- Update `.agents/index` in the same change when paths, entry points, responsibilities, boundaries, or verification flow change

## Implementation
- Dead code: confirm removal; reflection, scripting, assets, config, or external integrations may call code with no static callers
- Runtime: prefer event/callback/timer-driven flow over polling loops when practical
- Lifecycle: document ownership and cleanup when runtime relationships change
- Code shape: avoid all-public types; expose only the surface other modules need
- Boundary validation: validate objects, pointers, handles, paths, and external payloads at trust boundaries

## Version Control
- Binary assets use Git LFS locking only; do not convert them to LFS objects; never add `filter=lfs`
- Do not run `tools/manual-unlock.ps1` unless the user explicitly requests unlock for a dangling path

## Comments
- User-facing docs and code comments follow prompt language unless a stronger project convention applies
- Do not restate code behavior in prose
- Comment intent, boundaries, lifecycle, OS/platform differences, failure conditions, compatibility reasons, or non-obvious invariants
- Add at least a minimal comment for every non-local function, method, variable, property, field, struct, class, and module
- Function and method comments describe behavior or responsibility
- Property, field, and variable comments describe state, invariant, or ownership

## Skill Routing
- UE5, Unreal, C++, Blueprint, UMG, module dependency, packaging, PIE, runtime log, or Unreal asset workflow: use `.agents/skills/ue5-dev/SKILL.md`
