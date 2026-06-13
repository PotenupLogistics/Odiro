---
id: root-dev-workflow
owner: Root
paths:
  - .githooks/**
  - docs/guides/branch-naming-rules.md
  - docs/guides/development-environment.md
  - task-setup.bat
  - task-build.bat
  - task-run.bat
  - task-dev.bat
  - tools/**
entry:
  - task-setup.bat
  - task-build.bat
  - task-run.bat
  - task-dev.bat
  - tools/check-prerequisites.ps1
  - tools/install.ps1
  - tools/build.ps1
  - tools/run-preview.ps1
  - tools/dev-session.ps1
  - tools/set-git-hooks.ps1
keep:
  - Root scripts orchestrate project-owned task scripts; they do not implement Unreal or Agents startup details.
  - Root setup calls project task scripts directly, not project public .bat wrappers, to avoid duplicate phases.
  - Go is excluded from setup checks until Bridge exists.
verify:
  - PowerShell parse check for script edits
  - hook syntax and stdin smoke for .githooks changes
  - task-setup.bat -AllowMissingPrerequisites -SkipAgents
  - tools/check-prerequisites.ps1 -AllowMissing
related:
  - agents-tooling-harness
  - client-platform-execution
---
