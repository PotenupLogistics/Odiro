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
  - task-push.bat
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
  - tools/check-source-sanity.ps1
  - tools/pre-push-policy.ps1
  - tools/open-pull-request.ps1
  - tools/manual-unlock.ps1
keep:
  - Root scripts orchestrate project-owned task scripts; they do not implement Unreal or Agents startup details.
  - Root setup calls project task scripts directly, not project public .bat wrappers, to avoid duplicate phases.
  - Root build includes Bridge and Client; Agents has no build phase.
  - Main branch direct commits, local merges, and fast-forward pushes are blocked by hooks; local main deletion and intentional non-fast-forward force push are allowed.
  - Local pulls prefer rebase via pull.rebase=true, rebase.autoStash=true, branch.autoSetupRebase=always, and pull.ff=true so main sync and subbranch updates avoid merge commits without ff-only rejection.
  - post-merge and rebase post-rewrite hooks call tools/install.ps1 through .githooks/helpers/run-install after pulls update the worktree.
  - tools/set-git-config.ps1 stays idempotent and reports changed Git config, Editor checkout preference values, Git identity warnings, and completion, not already-correct keys or routine successful checks.
  - tools/set-git-config.ps1 sets missing repo-local user.name from the current Git LFS lock owner when available; user.email is warned, not inferred, because LFS lock data has no commit email.
  - Unreal binary assets stay Git blobs; Git LFS is used for lock/read-only/push verification only.
  - post-commit skips Git LFS read-only refresh when HEAD has no Unreal binary asset changes.
  - Feature branch commit and merge hooks return without source sanity checks; PR source sanity check runs tools/check-source-sanity.ps1 on a shallow merge-ref soft-reset staged diff and narrows UnityBuild helper scans from changed definitions.
  - PR helper task-push.bat delegates to tools/open-pull-request.ps1, pushes the current topic branch, skips duplicate open PRs into main, and uses GitHub CLI authentication only when the helper runs.
  - Post-Merge Tasks asset unlock queues lock-mutating runs, uses shallow checkout, path-limits auto unlocks to pushed Unreal assets, and rechecks lock id plus trusted push cutoff before unlock.
  - Manual LFS unlock is human-only exact-path recovery.
verify:
  - PowerShell parse check for script edits
  - hook syntax plus run-install smoke and staged main-delete smoke for .githooks changes
  - git check-attr lockable for sample Unreal asset
  - tools/set-git-config.ps1 local config smoke
  - tools/set-git-config.ps1 Git identity warning smoke with missing repo-local user.email
  - Client/Saved/Config/*Editor/EditorPerProjectUserSettings.ini checkout prompt smoke after tools/set-git-config.ps1
  - git config --local --get pull.ff returns true; pull.rebase returns true; rebase.autoStash returns true; branch.autoSetupRebase returns always
  - pre-push dry-run with empty updates and main fast-forward rejection
  - task-push.bat -DryRun -AllowDirty
  - tools/check-source-sanity.ps1 with no staged source inputs
  - GitHub Merge Checks workflow runs Source Sanity from the PR merge ref, conditionally sets up Go for Bridge changes, and runs Asset Lock Ownership for Unreal binary assets
  - GitHub Post-Merge Tasks workflow shallow-checks out main, queues lock-mutating runs, fetches the before commit for automatic push diffs, rechecks lock id/time before unlock, and preserves manual exact-path unlock handling
  - GitHub asset lock workflows fail closed when Git LFS lock queries fail or return empty JSON
  - task-setup.bat -AllowMissingPrerequisites -SkipAgents
  - tools/check-prerequisites.ps1 -AllowMissing
  - tools/build.ps1 -Target bridge
related:
  - agents-tooling-harness
  - bridge-host
  - client-platform-execution
---
