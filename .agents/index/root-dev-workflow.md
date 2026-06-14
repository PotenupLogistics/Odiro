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
  - tools/verify-staged.ps1
keep:
  - Root scripts orchestrate project-owned task scripts; they do not implement Unreal or Agents startup details.
  - Root setup calls project task scripts directly, not project public .bat wrappers, to avoid duplicate phases.
  - Root build includes Bridge and Client; Agents has no build phase.
  - Main branch commits and merge commits are gated by staged verification; setup configures merge.ff=false so normal merges create merge commits for the gate.
verify:
  - PowerShell parse check for script edits
  - hook syntax and staged smoke for .githooks changes
  - tools/verify-staged.ps1 with no staged build inputs
  - task-setup.bat -AllowMissingPrerequisites -SkipAgents
  - tools/check-prerequisites.ps1 -AllowMissing
  - tools/build.ps1 -Target bridge
related:
  - agents-tooling-harness
  - bridge-host
  - client-platform-execution
---
