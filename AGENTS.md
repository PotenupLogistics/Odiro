## Repository
- Context: `README.md`, `.agents/sourcemap/INDEX.md`
- Asset edits: `.uasset` and `.umap` through editor, commandlet, or project scripts
- Commands: no hardcoded local UE install paths

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
