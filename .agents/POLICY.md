# Policy

## Language
- Docs/Plans: Korean unless template or external audience requires English

## Implementation
- Dead code: confirm removal — may still be called via Blueprint or reflection even with no C++ callers

## Architecture
- Boundaries: Component/Interface composition; separate Actor/Component/Subsystem concerns
- Runtime: events/timers over Tick; document ownership/lifecycle on runtime relationship changes
- Blueprint: expose tunables via `UPROPERTY(EditAnywhere)` or `BlueprintReadOnly`
- Code: avoid all-public classes; `IsValid()` at UObject boundaries — callbacks may deliver pending-kill pointers
- Reflected names: no rename without migration plan — embedded in binary `.uasset` and saved data

## Naming
- class/member/function: PascalCase, e.g. `class Apple`, `void SetDead()`, `float Hp{ 0.f }`
- parameter/local: camelCase, e.g. `float hp`, `float damageValue`
- Bool: `bDead` (state), `IsDead()` (query)

## Artifacts
- Plan files: `Docs/plans/PLAN-<title>.md`
