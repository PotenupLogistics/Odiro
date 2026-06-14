---
id: root-dev-workflow
owner: Root
paths:
  - .github/workflows/**
  - .githooks/**
  - .lfsconfig
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
  - tools/set-git-config.ps1
  - tools/verify-staged.ps1
  - tools/pre-push-policy.ps1
  - tools/manual-unlock.ps1
keep:
  - Root scripts orchestrate project-owned task scripts; they do not implement Unreal or Agents startup details.
  - Root setup calls project task scripts directly, not project public .bat wrappers, to avoid duplicate phases.
  - Root build includes Bridge and Client; Agents has no build phase.
  - Main branch direct commits, local merges, and fast-forward pushes are blocked by hooks; intentional non-fast-forward force push is allowed.
  - Unreal binary assets stay Git blobs; Git LFS is used for lock/read-only/push verification only.
  - Manual LFS unlock is human-only exact-path recovery.
verify:
  - PowerShell parse check for script edits
  - hook syntax and staged smoke for .githooks changes
  - git check-attr lockable for sample Unreal asset
  - tools/set-git-config.ps1 local config smoke
  - pre-push dry-run with empty updates and main fast-forward rejection
  - tools/verify-staged.ps1 with no staged build inputs
  - task-setup.bat -AllowMissingPrerequisites -SkipAgents
  - tools/check-prerequisites.ps1 -AllowMissing
  - tools/build.ps1 -Target bridge
related:
  - agents-tooling-harness
  - bridge-host
  - client-platform-execution
---
