# Agent Source Index

Agent-only source index for entry points, ownership, guardrails, and focused verification.

## Purpose
- Route source reading to the smallest useful area before broad navigation.
- Keep ownership, entry points, guardrails, and focused checks close to the paths they affect.
- Treat source code, contracts, specs, tests, and build files as authoritative when they differ from this index.

## Reading Algorithm
- Start here, then open only cards whose `paths` or `workflows` match the touched path or requested workflow.
- Read each matching card's `entry` groups in order.
- Follow `links` only when canonical docs/specs are needed for the task.
- Use `related` as a navigation hint when the change crosses area boundaries.

## Active Card Format
- Root: `.agents/index/README.md`
- Cards: `.agents/index/cards/<area>.yaml`
- Card fields, in order: `id`, `owner`, `description`, `paths`, optional `workflows`, `entry`, `guardrails`, `verify`, `links`, optional `related`.
- `entry` contains ordered read groups with `id`, `description`, optional `needs`, and `read`.
- `verify` contains grouped checks with `when` and `run`.

## Maintenance
- Update cards when paths, entry points, ownership, guardrails, or verification flow changes.
- Keep cards as structured navigation data; move long explanations to canonical docs/specs and link them.
- Preserve this YAML card layout unless an explicit migration replaces it.

## Legacy Migration
- Legacy root `INDEX.md` moved to this README.
- Legacy root card files `<area>.md` moved to `cards/<area>.yaml`.

## Cards
- [`agent-context`](cards/agent-context.yaml): root agent rules and source index ownership
- [`agents-generation-runtime`](cards/agents-generation-runtime.yaml): FastAPI generation, WorldConfig, user project migration boundary
- [`agents-policy-rag-data`](cards/agents-policy-rag-data.yaml): policy RAG, policy cards, source/review data
- [`agents-tooling-harness`](cards/agents-tooling-harness.yaml): Agents scripts, harness, pytest
- [`bridge-host`](cards/bridge-host.yaml): Go host process, portless IPC, Bridge tooling
- [`client-delivery-bot-policy`](cards/client-delivery-bot-policy.yaml): DeliveryBot movement, grid, policy HTTP
- [`client-platform-execution`](cards/client-platform-execution.yaml): MainMenu, launcher, process status, AI analysis
- [`client-runtime-foundation`](cards/client-runtime-foundation.yaml): Unreal project config, targets, module deps, assets
- [`client-simulation`](cards/client-simulation.yaml): Scenario/Episode runtime, scenario_sample generation, reports
- [`contracts-shared-data`](cards/contracts-shared-data.yaml): shared schemas, specs, payload/API/file contracts
- [`root-dev-workflow`](cards/root-dev-workflow.yaml): root setup/build/run/dev, hooks, repo tools
