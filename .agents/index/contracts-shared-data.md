---
id: contracts-shared-data
owner: Root
paths:
  - contracts/**
  - contracts/specs/**
  - contracts/specs/bridge-ipc.md
entry:
  - contracts/schemas
  - contracts/specs
  - contracts/examples
  - contracts/specs/bridge-ipc.md
keep:
  - contracts/specs is the human-readable truth for shared payload/API/file formats.
  - docs/specs is for repository/product structure, rules, and requirements.
  - Bridge IPC uses local portless transports; message compatibility belongs in contracts/specs/bridge-ipc.md.
verify:
  - consuming project schema validation
  - contract specs vs generated examples before implementation changes
  - compatibility fixtures when two components consume the same payload
related:
  - client-simulation
  - client-delivery-bot-policy
  - agents-generation-runtime
---
