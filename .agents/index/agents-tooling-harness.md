---
id: agents-tooling-harness
owner: Agents
paths:
  - Agents/harness/**
  - Agents/scripts/**
  - Agents/tools/**
  - Agents/tests/**
  - Agents/docs/tooling/**
  - Agents/task-setup.bat
  - Agents/task-run.bat
  - Agents/task-dev.bat
entry:
  - Agents/harness/checks
  - Agents/scripts
  - Agents/tests
  - Agents/task-setup.bat
  - Agents/task-run.bat
  - Agents/task-dev.bat
  - Agents/tools/check-prerequisites.ps1
  - Agents/tools/install.ps1
  - Agents/tools/run.ps1
  - Agents/tools/dev.ps1
keep:
  - Agents/tools use Agents/tools/common.ps1, never root tools/common.ps1.
  - Agents has no build task; it is a Python server.
verify:
  - focused pytest for touched test surface
  - harness.checks.check_all only for broad Agents readiness changes
  - CLI help/smoke for script changes
related:
  - root-dev-workflow
  - agents-generation-runtime
---
