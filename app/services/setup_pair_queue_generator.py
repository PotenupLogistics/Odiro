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
    for index, variant in enumerate(generate_episode_variants(base_world_config, count, base_seed)):
        pair_id = f"{scenario_id}_{index:03d}"
        episode_path = f"Json/Input/EpisodeSetup_{pair_id}.json"
        bot_path = f"Json/Input/DeliveryBotSetup_{pair_id}.json"
        episode_setup = convert_world_config_to_episode_setup(variant.world_config)
        delivery_bot_setup = convert_world_config_to_delivery_bot_setup(variant.world_config)
        episode_validation = validate_episode_setup(episode_setup)
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

    run_queue_path = f"Json/Input/EpisodeRunQueue_{scenario_id}.json"
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
