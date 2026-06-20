---
id: agents-generation-runtime
owner: Agents
paths:
  - Agents/README.md
  - Agents/app/api/**
  - Agents/app/models/**
  - Agents/app/agents/common/**
  - Agents/app/agents/scenario_generation_v2/**
  - Agents/app/agents/result_analysis_v2/**
  - Agents/app/services/*generation*
  - Agents/app/services/*world_config*
  - Agents/app/services/*episode*
  - Agents/app/services/*run_queue*
  - Agents/docs/README.md
  - Agents/docs/agents/**
  - Agents/docs/architecture/**
  - Agents/docs/development/**
  - Agents/docs/environment/**
  - Agents/docs/experiment/**
  - Agents/docs/handoff/**
  - Agents/docs/json_contracts/**
  - Agents/docs/api/V2_AGENT_APIS.md
  - Agents/docs/providers/**
  - Agents/docs/research/**
  - Agents/docs/status/**
  - docs/specs/simulation-interface.md
  - contracts/specs/user-project-data.md
entry:
  - Agents/README.md
  - Agents/app/api/routes.py
  - Agents/app/models/world.py
  - Agents/app/models/episode_setup.py
  - Agents/app/models/delivery_bot_setup.py
  - Agents/app/models/run_queue.py
  - Agents/app/agents/common/spec_context_loader.py
  - Agents/app/agents/scenario_generation_v2/agent.py
  - Agents/app/agents/scenario_generation_v2/graph_runner.py
  - Agents/app/agents/result_analysis_v2/agent.py
  - Agents/app/agents/result_analysis_v2/graph_runner.py
  - Agents/app/services/world_config_generation_orchestrator.py
  - Agents/app/services/world_config_to_episode_setup_adapter.py
  - Agents/app/services/world_config_to_delivery_bot_setup_adapter.py
  - Agents/app/services/run_queue_export_service.py
  - Agents/docs/environment
  - Agents/docs/environment/environment-catalog.md
  - Agents/docs/handoff
  - Agents/docs/providers
  - Agents/docs/research
  - contracts/specs/user-project-data.md
keep:
  - Do not re-expose legacy UE handoff routes unless intentionally restoring them.
  - Final user project contract writes project/run/episode artifacts, not wrapper-free RunQueue JSON.
  - Current Scenario/Episode/Run terminology belongs in docs/specs/simulation-interface.md; archived terminology notes are legacy UE handoff context only.
  - Agents/docs/environment/environment-catalog.md is a temporary AI-side LLM catalog copied from Unreal context until a contracts/specs environment catalog becomes canonical.
  - V2 Agent LLM spec context is loaded only from the SpecContextLoader allowlist; do not add Client/Json/environment-catalog.md or archive paths to runtime prompts.
  - Public scenario generation must not call legacy RunQueue export tooling; `/api/v1/scenarios/generate` returns `410 RUN_QUEUE_REMOVED`.
  - Agents/app/services/scenario_generation_service.py is legacy tooling, not the active v2 scenario path.
  - RunQueue export services, models, and tests are retained only as legacy tooling, not public API or user project runtime.
verify:
  - focused pytest for touched route/model/service
  - contract validation when payload fields change
  - result analysis review artifact tests when `/api/v2/analysis/run` storage behavior changes
  - v2 spec context prompt tests when LLM prompt context allowlist or injection changes
related:
  - agents-policy-rag-data
  - contracts-shared-data
  - client-simulation
---
