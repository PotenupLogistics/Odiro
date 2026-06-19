---
id: contracts-shared-data
owner: Root
paths:
  - Client/Docs/README.md
  - Client/Docs/JSON_Guide/**
  - Client/Docs/specs/**
  - contracts/**
  - contracts/specs/**
  - contracts/specs/bridge-ipc.md
  - docs/specs/**
  - static/project-templates/**
entry:
  - Client/Docs/README.md
  - Client/Docs/JSON_Guide
  - Client/Docs/specs
  - contracts/schemas
  - contracts/specs
  - contracts/examples
  - contracts/specs/bridge-ipc.md
  - contracts/specs/user-project-data.md
  - docs/specs
  - static/project-templates
keep:
  - contracts/specs is the human-readable truth for shared payload/API/file formats.
  - User project setting/profile/scenario/policy, run artifact, project template, and run default file shapes belong in contracts/specs/user-project-data.md; Bridge IPC method shapes belong in contracts/specs/bridge-ipc.md.
  - Client/Docs/Data was removed; do not recreate it as a shared file contract source.
  - Client/Docs/JSON_Guide is legacy Client input/output reference and must not define new shared file contracts.
  - Client/Docs/specs is legacy Client interface context; final product structure and execution flow belong in docs/specs.
  - docs/specs is for repository/product structure, rules, and requirements.
  - Bridge IPC uses local portless transports; message compatibility belongs in contracts/specs/bridge-ipc.md.
  - `RunQueue` and `DeliveryBotSetup` specs are legacy guides; new shared file contracts belong in user-project-data.md.
verify:
  - consuming project schema validation
  - contract specs vs generated examples before implementation changes
  - compatibility fixtures when two components consume the same payload
related:
  - client-simulation
  - client-delivery-bot-policy
  - agents-generation-runtime
---
