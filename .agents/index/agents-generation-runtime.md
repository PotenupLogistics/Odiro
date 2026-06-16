---
id: agents-generation-runtime
owner: Agents
paths:
  - Agents/README.md
  - Agents/app/api/**
  - Agents/app/models/**
  - Agents/app/services/*generation*
  - Agents/app/services/*world_config*
  - Agents/app/services/*episode*
  - Agents/app/services/*run_queue*
  - Agents/docs/README.md
  - Agents/docs/agents/**
  - Agents/docs/architecture/**
  - Agents/docs/development/**
  - Agents/docs/experiment/**
  - Agents/docs/json_contracts/**
  - Agents/docs/status/**
entry:
  - Agents/README.md
  - Agents/app/api/routes.py
  - Agents/app/models/world.py
  - Agents/app/models/episode_setup.py
  - Agents/app/models/delivery_bot_setup.py
  - Agents/app/models/run_queue.py
  - Agents/app/services/world_config_generation_orchestrator.py
  - Agents/app/services/world_config_to_episode_setup_adapter.py
  - Agents/app/services/world_config_to_delivery_bot_setup_adapter.py
  - Agents/app/services/run_queue_export_service.py
keep:
  - Do not re-expose legacy UE handoff routes unless intentionally restoring them.
  - Final user project contract should write project/run/episode artifacts, not wrapper-free RunQueue JSON.
verify:
  - focused pytest for touched route/model/service
  - contract validation when payload fields change
related:
  - agents-policy-rag-data
  - contracts-shared-data
  - client-simulation
---
