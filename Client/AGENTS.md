## Repository
- Asset edits: `.uasset` and `.umap` through editor, commandlet, or project scripts
- Build: `Task-Build.bat`
- PIE Preview: `Task-RunPreview.bat`
- Python Policy Server: `Task-RunPythonPolicyServer.bat`
- Commands: no hardcoded local UE install paths

## Naming
- Class/member/function: PascalCase, e.g. `class Apple`, `void SetDead()`, `float Hp = 0.f;`
- Parameter/local: camelCase, e.g. `float hp`, `float damageValue`
- Bool: `bDead` for state, `IsDead()` for query

## Artifacts
- Plan files: `Docs/plans/<title>.md`
