from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass, field
from typing import Any

from app.core.settings import Settings
from app.services.delivery_bot_setup_variation_policy import delivery_bot_tuning_for_episode


@dataclass(frozen=True)
class EpisodeVariant:
    world_config: dict[str, Any]
    changed_parameters: list[str] = field(default_factory=list)


def _fixed_parameters(world_config: dict[str, Any]) -> dict[str, Any]:
    environment_sampling = world_config.get("environmentSampling")
    if not isinstance(environment_sampling, dict):
        constraints = world_config.get("constraints")
        environment_sampling = constraints.get("environmentSampling") if isinstance(constraints, dict) else {}
    fixed = environment_sampling.get("fixedParameters") if isinstance(environment_sampling, dict) else {}
    return fixed if isinstance(fixed, dict) else {}


def _set_runtime_iteration(config: dict[str, Any], index: int) -> None:
    run = config.get("run") if isinstance(config.get("run"), dict) else {}
    run["iteration_index"] = index
    config["run"] = run


def _set_seed(config: dict[str, Any], seed: int) -> None:
    config["seed"] = seed


def _apply_delivery_bot_tuning(config: dict[str, Any], index: int, fixed: dict[str, Any], changes: list[str]) -> None:
    tuning = delivery_bot_tuning_for_episode(index, fixed_parameters=fixed, scenario_intent=str(config.get("scenarioId", "")))
    environment_sampling = config.get("environmentSampling")
    if not isinstance(environment_sampling, dict):
        environment_sampling = {}
        config["environmentSampling"] = environment_sampling
    environment_sampling["deliveryBotPolicyProfile"] = tuning.profile
    for key, value in tuning.values.items():
        environment_sampling[key] = value
    changes.extend(tuning.changed_parameters)


def generate_episode_variants(
    base_world_config: dict[str, Any],
    episode_count: int | None = None,
    base_seed: int | None = None,
) -> list[EpisodeVariant]:
    settings = Settings()
    count = episode_count if episode_count is not None else settings.scenarioEpisodeDefaultCount
    seed_start = int(base_seed if base_seed is not None else base_world_config.get("seed", 0))
    fixed = _fixed_parameters(base_world_config)
    variants: list[EpisodeVariant] = []

    for index in range(count):
        config = deepcopy(base_world_config)
        changes: list[str] = []
        _set_seed(config, seed_start + index)
        _set_runtime_iteration(config, index)
        _apply_delivery_bot_tuning(config, index, fixed, changes)
        variants.append(EpisodeVariant(world_config=config, changed_parameters=changes))
    return variants
