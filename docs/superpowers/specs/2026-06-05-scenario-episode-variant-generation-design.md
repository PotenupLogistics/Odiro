# Scenario Episode Variant Generation Design

## Goal

Keep the public scenario generation API input unchanged while making each generated queue item use a different episode environment and a different robot policy.

The request body remains:

```json
{
  "prompt": "user natural language scenario",
  "episode_count": 5
}
```

`episode_count` behavior stays unchanged. If the caller provides it, generate that many queue items. If it is omitted, use `Settings().scenarioEpisodeDefaultCount`. Existing maximum count validation remains in place.

## Current Problem

Two internal defaults make the generated queue too fixed:

- `app/services/scenario_generation_service.py` always sends `environmentSampling.scenarioType="obstacle_ahead"` and `seed=1001` to world config generation.
- `app/services/setup_pair_queue_generator.py` ignores the generated scenario environment for EpisodeSetup and replaces it with `_fixed_policy_scene_setup()`, a fixed narrow sidewalk center-blocking scene.

As a result, even when the API returns 5 queue items, the episode environment is effectively fixed. Robot policy variation also repeats baseline profiles for early items, so policies are not all distinct.

## Intended Behavior

For a single natural language prompt, the backend should generate variants of the same scenario intent.

Example: if the prompt says an obstacle blocks the robot path, every queue item should still be an obstacle-blocking scenario. However, each item should vary numeric environment parameters such as sidewalk width, blocking ratio, obstacle offset, pedestrian count/speed, terrain risk, runtime limit, and seed where applicable.

Each queue item should also use a distinct DeliveryBot policy profile. The default 5 queue items should not produce three baseline robot policies.

## Scope

Public API input parameters do not change:

- Do not add `scenario_type`.
- Do not add `seed`.
- Do not add `fixed_parameters`.
- Keep `prompt` and optional `episode_count` only.

Both endpoints use the same internal generation service and should change together:

- `POST /api/v1/scenarios/generate`
- `POST /api/v1/scenarios/generate-artifacts`

## Design

### 1. Natural Language To Base Sampling Context

Use the existing `extract_scenario_intent(prompt)` rule-based extractor to infer a base scenario type.

Suggested mapping:

- Kickboard plus narrow sidewalk or crossing hints -> `narrow_sidewalk_kickboard_crossing`
- Obstacle or path-blocking hints -> `obstacle_ahead`
- Pedestrian crossing hints -> `pedestrian_crossing`
- Terrain hints -> `terrain_risk`
- Otherwise -> `generic_sidewalk`

Build a deterministic base seed from the prompt text. The same prompt should produce the same default queue, while different prompts can produce different variants.

If the prompt explicitly includes supported numeric values, pass those as fixed parameters so user-specified values remain stable across all variants. Examples:

- `sidewalkWidthCm`
- `obstacleBlockingRatio`

### 2. Generate One Base WorldConfig

Keep world config generation as a single base generation step. The base request should use the inferred sampling context instead of hard-coded `obstacle_ahead` and `1001`.

This keeps LLM cost and latency bounded while still giving all variants the same scenario intent.

### 3. Generate Episode Variants

Change `generate_episode_variants()` so it varies both environment and robot policy.

For each index from `0` to `count - 1`:

- Use `seed = base_seed + index`.
- Sample environment parameters with the inferred `scenarioType`.
- Preserve fixed parameters extracted from the prompt.
- Patch the variant WorldConfig with sampled environment values.
- Set `seed` and `run.iteration_index`.
- Apply a policy profile for the same index.

Explicit user constraints should win over variation. For example, if the prompt says `보도 폭은 120cm`, all variants should keep `sidewalkWidthCm=120`, while other environment parameters can still vary.

### 4. Generate Pair Per Variant

Change `generate_setup_pair_queue()` so it no longer uses `_fixed_policy_scene_setup()` for normal scenario generation.

Instead, for each `EpisodeVariant`:

- Convert `variant.world_config` to `EpisodeSetup`.
- Convert `variant.world_config` to `DeliveryBotSetup`.
- Validate both.
- Add a RunQueue item pointing to that variant's JSON paths.

RunQueue paths should be scenario-based and index-based, for example:

- `Json/Input/EpisodeRunQueue_obstacle_ahead.json`
- `Json/Input/EpisodeSetup_obstacle_ahead_000.json`
- `Json/Input/DeliveryBotSetup_obstacle_ahead_000_baseline.json`

The zip endpoint should naturally include the same distinct generated artifacts because it already serializes queue items.

### 5. Distinct Robot Policies

Update `delivery_bot_tuning_for_episode()` so the first five default profiles are distinct.

Suggested profiles:

- `baseline`
- `cautious_lidar`
- `slow_safe`
- `wide_detection`
- `fast_reactive`

Each profile should produce a different DeliveryBotSetup through environment sampling fields consumed by `convert_world_config_to_delivery_bot_setup()`.

For counts greater than the number of defined profiles, cycle profiles or fall back deterministically, but file names and seeds should still remain unique by index.

## Testing

Update or add focused tests for:

- `ScenarioGenerateRequest` still exposes only `prompt` and optional `episode_count`.
- Scenario generation request no longer hard-codes `scenarioType="obstacle_ahead"` for every prompt.
- Default count still comes from settings when `episode_count` is omitted.
- Provided `episode_count` controls the number of generated queue items.
- Queue items have distinct EpisodeSetup payloads.
- Queue items have distinct DeliveryBotSetup policy profiles.
- Explicit prompt numeric values remain fixed across variants.
- `/api/v1/scenarios/generate-artifacts` zip includes distinct EpisodeSetup and DeliveryBotSetup JSON files.

## Out Of Scope

- No API request schema change.
- No new public endpoint.
- No per-request user override fields for seed or scenario type.
- No batch DOE matrix feature.
- No extra LLM call per episode variant.
