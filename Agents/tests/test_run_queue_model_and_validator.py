from __future__ import annotations

from app.models.run_queue import EpisodeRunQueue
from app.services.run_queue_validator import validate_run_queue


def _valid_queue() -> dict:
    return {
        "schema": "episode_run_queue",
        "version": 1,
        "runs": [
            {
                "pair_id": "obstacle_ahead_000",
                "episode_setup": "Json/Input/EpisodeSetup_obstacle_ahead_000.json",
                "delivery_bot_setup": "Json/Input/DeliveryBotSetup_obstacle_ahead_000.json",
            }
        ],
    }


def test_run_queue_model_accepts_contract_shape_only() -> None:
    queue = EpisodeRunQueue.model_validate(_valid_queue())

    assert queue.schema == "episode_run_queue"
    assert queue.version == 1
    assert queue.runs[0].pair_id == "obstacle_ahead_000"
    assert set(queue.model_dump(mode="json", by_alias=True)) == {"schema", "version", "runs"}


def test_run_queue_validator_rejects_wrapper_fields() -> None:
    payload = _valid_queue()
    payload["success"] = True
    payload["diagnostics"] = {}
    payload["setupPairs"] = []

    result = validate_run_queue(payload)

    assert result.valid is False
    codes = {error.code for error in result.errors}
    assert "forbidden_root_field" in codes
    assert "model_validation_error" in codes


def test_run_queue_validator_rejects_duplicate_pair_ids_and_non_ue_paths() -> None:
    payload = _valid_queue()
    payload["runs"].append(
        {
            "pair_id": "obstacle_ahead_000",
            "episode_setup": "EpisodeSetup_obstacle_ahead_001.json",
            "delivery_bot_setup": "data/DeliveryBotSetup_obstacle_ahead_001.json",
        }
    )

    result = validate_run_queue(payload)

    assert result.valid is False
    codes = {error.code for error in result.errors}
    assert "duplicate_pair_id" in codes
    assert "invalid_episode_setup_path" in codes
    assert "invalid_delivery_bot_setup_path" in codes


def test_run_queue_validator_requires_non_empty_runs() -> None:
    result = validate_run_queue({"schema": "episode_run_queue", "version": 1, "runs": []})

    assert result.valid is False
    assert {error.code for error in result.errors} == {"empty_runs"}
