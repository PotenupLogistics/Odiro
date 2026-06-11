from __future__ import annotations

from typing import Any

from app.services.run_queue_validator import FORBIDDEN_RUN_QUEUE_FIELDS
from app.services.setup_pair_queue_generator import SetupPairQueueResult


FORBIDDEN_EPISODE_SETUP_FIELDS = {"success", "responseFormat", "diagnostics", "setupPairs", "validation", "trace"}
FORBIDDEN_DELIVERY_BOT_SETUP_FIELDS = {"success", "responseFormat", "diagnostics", "setupPairs", "validation", "trace"}


def _root_forbidden_fields(payload: dict[str, Any], forbidden: set[str]) -> list[str]:
    return sorted(field for field in forbidden if field in payload)


def summarize_run_queue_result(queue: SetupPairQueueResult, export_path: str | None = None) -> dict[str, Any]:
    run_queue_payload = queue.run_queue.model_dump(mode="json", by_alias=True)
    episode_payloads = [item.episode_setup.model_dump(mode="json", by_alias=True) for item in queue.items]
    bot_payloads = [item.delivery_bot_setup.model_dump(mode="json", by_alias=True) for item in queue.items]
    return {
        "runQueueExists": bool(queue.run_queue.runs),
        "runCount": len(queue.run_queue.runs),
        "pairIds": [run.pair_id for run in queue.run_queue.runs],
        "episodeSetupCount": len(episode_payloads),
        "deliveryBotSetupCount": len(bot_payloads),
        "allEpisodeSetupValidationPassed": all(item.episode_setup_validation.valid for item in queue.items),
        "allDeliveryBotSetupValidationPassed": all(item.delivery_bot_setup_validation.valid for item in queue.items),
        "runQueueValidationPassed": queue.run_queue_validation.valid,
        "exportPath": export_path,
        "forbiddenWrapperFieldsInRunQueue": _root_forbidden_fields(run_queue_payload, FORBIDDEN_RUN_QUEUE_FIELDS),
        "forbiddenFieldsInEpisodeSetup": sorted(
            {field for payload in episode_payloads for field in _root_forbidden_fields(payload, FORBIDDEN_EPISODE_SETUP_FIELDS)}
        ),
        "forbiddenFieldsInDeliveryBotSetup": sorted(
            {field for payload in bot_payloads for field in _root_forbidden_fields(payload, FORBIDDEN_DELIVERY_BOT_SETUP_FIELDS)}
        ),
    }
