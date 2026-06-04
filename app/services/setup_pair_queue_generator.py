from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any

from app.core.settings import Settings
from app.models.delivery_bot_setup import DeliveryBotSetup
from app.models.episode_setup import EpisodeSetup, SetupValidationError, SetupValidationResult
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem
from app.services.delivery_bot_setup_validator import validate_delivery_bot_setup
from app.services.episode_setup_validator import validate_episode_setup
from app.services.episode_variation_generator import EpisodeVariant, generate_episode_variants
from app.services.run_queue_validator import validate_run_queue
from app.services.world_config_to_delivery_bot_setup_adapter import convert_world_config_to_delivery_bot_setup
from app.services.world_config_to_episode_setup_adapter import convert_world_config_to_episode_setup


POLICY_COMPARISON_SCENE_ID = "narrow_sidewalk"
POLICY_COMPARISON_EPISODE_PATH = "Json/Input/EpisodeSetup_narrow_sidewalk_fixed_center_block.json"
POLICY_COMPARISON_RUN_QUEUE_PATH = "Json/Input/EpisodeRunQueue_narrow_sidewalk_policy_comparison.json"
POLICY_FILE_NAMES = {
    "baseline": "DeliveryBotSetup_policy_000_baseline.json",
    "short_stop": "DeliveryBotSetup_policy_001_short_stop.json",
    "long_stop": "DeliveryBotSetup_policy_002_long_stop.json",
    "early_slowdown": "DeliveryBotSetup_policy_003_early_slowdown.json",
    "low_speed": "DeliveryBotSetup_policy_004_low_speed.json",
}


@dataclass(frozen=True)
class SetupPairQueueItem:
    pair_id: str
    variant: EpisodeVariant
    episode_setup: EpisodeSetup
    delivery_bot_setup: DeliveryBotSetup
    episode_setup_path: str
    delivery_bot_setup_path: str
    episode_setup_validation: SetupValidationResult
    delivery_bot_setup_validation: SetupValidationResult


@dataclass(frozen=True)
class SetupPairQueueResult:
    scenario_id: str
    request_id: str
    items: list[SetupPairQueueItem]
    run_queue: EpisodeRunQueue
    run_queue_path: str
    run_queue_validation: SetupValidationResult
    export_base_dir: str
    errors: list[str] = field(default_factory=list)

    @property
    def run_count(self) -> int:
        return len(self.items)


def _slug(value: Any) -> str:
    text = str(value or "scenario").strip().lower()
    text = re.sub(r"[^a-z0-9_]+", "_", text)
    text = re.sub(r"_+", "_", text).strip("_")
    return text or "scenario"


def _count_error(code: str, message: str) -> SetupValidationResult:
    return SetupValidationResult(valid=False, errors=[SetupValidationError(code=code, message=message)], warnings=[])


def _policy_pair_id(profile: str, index: int) -> str:
    return f"{POLICY_COMPARISON_SCENE_ID}_policy_{index:03d}_{profile}"


def _delivery_bot_setup_path(profile: str, index: int) -> str:
    filename = POLICY_FILE_NAMES.get(profile, f"DeliveryBotSetup_policy_{index:03d}_{profile}.json")
    return f"Json/Input/{filename}"


def _fixed_policy_scene_setup(base_world_config: dict[str, Any], base_seed: int) -> EpisodeSetup:
    scene_config = {
        **base_world_config,
        "scenarioId": "narrow_sidewalk_fixed_center_block",
        "seed": base_seed,
        "map": {"type": "Sidewalk", "lengthCm": 1400, "sidewalkWidthCm": 150},
        "robot": {
            "botId": "delivery_bot_01",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 1050, "y": 0, "z": 0},
        },
        "obstacles": [
            {
                "objectId": "obstacle_01",
                "type": "Obstacle",
                "position": {"x": 550, "y": 0, "z": 0},
                "blockingRatio": 0.6,
            }
        ],
        "pedestrians": [],
        "run": {"iteration_index": 0},
    }
    runtime = base_world_config.get("runtime") if isinstance(base_world_config.get("runtime"), dict) else {}
    scene_config["runtime"] = {"maxDurationSec": runtime.get("maxDurationSec", 60)}
    episode_setup = convert_world_config_to_episode_setup(scene_config)
    region = episode_setup.ground_model.regions[0]
    region.shape.center_xy_m = [5.0, 0.0]
    region.shape.size_m = [14.0, 1.5]
    return episode_setup


def generate_setup_pair_queue(
    base_world_config: dict[str, Any],
    episode_count: int | None = None,
    base_seed: int | None = None,
    request_id: str = "run-queue-export",
) -> SetupPairQueueResult:
    settings = Settings()
    count = episode_count if episode_count is not None else settings.scenarioEpisodeDefaultCount
    scenario_id = _slug(base_world_config.get("scenarioId") or base_world_config.get("worldId") or "scenario")
    if count < 1:
        validation = _count_error("invalid_episode_count", "episode_count must be at least 1.")
        return SetupPairQueueResult(scenario_id, request_id, [], EpisodeRunQueue(), "", validation, settings.runQueueExportBaseDir)
    if count > settings.scenarioEpisodeMaxCount:
        validation = _count_error("episode_count_too_large", f"episode_count must be <= {settings.scenarioEpisodeMaxCount}.")
        return SetupPairQueueResult(scenario_id, request_id, [], EpisodeRunQueue(), "", validation, settings.runQueueExportBaseDir)

    items: list[SetupPairQueueItem] = []
    runs: list[EpisodeRunQueueItem] = []
    seed_start = int(base_seed if base_seed is not None else base_world_config.get("seed", 0))
    fixed_episode_setup = _fixed_policy_scene_setup(base_world_config, seed_start)
    fixed_episode_validation = validate_episode_setup(fixed_episode_setup)
    for index, variant in enumerate(generate_episode_variants(base_world_config, count, base_seed)):
        profile = str(variant.world_config.get("environmentSampling", {}).get("deliveryBotPolicyProfile", "") or "baseline")
        pair_id = _policy_pair_id(profile, index)
        episode_path = POLICY_COMPARISON_EPISODE_PATH
        bot_path = _delivery_bot_setup_path(profile, index)
        episode_setup = fixed_episode_setup
        delivery_bot_setup = convert_world_config_to_delivery_bot_setup(variant.world_config)
        episode_validation = fixed_episode_validation
        bot_validation = validate_delivery_bot_setup(delivery_bot_setup)
        items.append(
            SetupPairQueueItem(
                pair_id=pair_id,
                variant=variant,
                episode_setup=episode_setup,
                delivery_bot_setup=delivery_bot_setup,
                episode_setup_path=episode_path,
                delivery_bot_setup_path=bot_path,
                episode_setup_validation=episode_validation,
                delivery_bot_setup_validation=bot_validation,
            )
        )
        runs.append(EpisodeRunQueueItem(pair_id=pair_id, episode_setup=episode_path, delivery_bot_setup=bot_path))

    run_queue_path = POLICY_COMPARISON_RUN_QUEUE_PATH
    queue = EpisodeRunQueue(runs=runs)
    validation = validate_run_queue(queue)
    return SetupPairQueueResult(
        scenario_id=scenario_id,
        request_id=request_id,
        items=items,
        run_queue=queue,
        run_queue_path=run_queue_path,
        run_queue_validation=validation,
        export_base_dir=settings.runQueueExportBaseDir,
    )
