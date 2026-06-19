---
id: bridge-host
owner: Bridge
paths:
  - Bridge/**
  - static/templates/**
  - static/run-defaults/**
  - contracts/specs/bridge-ipc.md
entry:
  - Bridge/AGENTS.md
  - Bridge/cmd/odirohost/main.go
  - Bridge/internal/api
  - Bridge/internal/ipc
  - Bridge/internal/process
  - Bridge/internal/protocol
  - Bridge/internal/workspace
  - static/templates
  - static/run-defaults
  - Bridge/internal/tooling
  - Bridge/tools/common.ps1
  - Bridge/tools/check-prerequisites.ps1
  - Bridge/tools/build.ps1
  - Bridge/tools/run.ps1
  - Bridge/task-setup.bat
  - Bridge/task-build.bat
  - Bridge/task-run.bat
keep:
  - Bridge exposes local IPC only; do not introduce HTTP ports for Controller communication without a contract update.
  - IPC transport is selected by Go build tags: Windows named pipe, Unix domain socket.
  - Shared observable IPC messages are documented in contracts/specs/bridge-ipc.md.
  - static/templates and static/run-defaults are the user project migration resource sources.
  - Workspace preset validation rejects generated Python cache; run creation copies snapshots and excludes source-only `.gitkeep` markers.
  - Process manager validates existing run snapshots, starts simulator children, and persists `run_status` lifecycle JSON.
  - Bridge tools must not dot-source root tools/common.ps1; go test enforces this ownership boundary.
verify:
  - cd Bridge; go test ./...
  - process lifecycle tests use the Go test binary as a fake simulator child and check `status.json`
  - Bridge/tools/build.ps1
  - PowerShell parse check for Bridge/tools script edits
related:
  - root-dev-workflow
  - contracts-shared-data
  - client-platform-execution
---
