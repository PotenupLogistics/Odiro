from __future__ import annotations

from typing import Any

from pydantic import ValidationError

from app.models.episode_setup import SetupValidationError, SetupValidationResult, SetupValidationWarning
from app.models.run_queue import EpisodeRunQueue


FORBIDDEN_RUN_QUEUE_FIELDS = {
    "success",
    "responseFormat",
    "diagnostics",
    "setupPairs",
    "episodeSetup",
    "deliveryBotSetup",
    "validation",
    "trace",
}


def _error(code: str, message: str, path: str | None = None) -> SetupValidationError:
    return SetupValidationError(code=code, message=message, path=path)


def _validate_raw(payload: dict[str, Any], errors: list[SetupValidationError]) -> None:
    for field in FORBIDDEN_RUN_QUEUE_FIELDS:
        if field in payload:
            errors.append(_error("forbidden_root_field", f"{field} must not be output in EpisodeRunQueue.", field))

    if payload.get("schema") != "episode_run_queue":
        errors.append(_error("invalid_schema", "schema must be episode_run_queue.", "schema"))
    if payload.get("version") != 1:
        errors.append(_error("invalid_version", "version must be 1.", "version"))


def validate_run_queue(run_queue: EpisodeRunQueue | dict[str, Any]) -> SetupValidationResult:
    errors: list[SetupValidationError] = []
    warnings: list[SetupValidationWarning] = []
    if isinstance(run_queue, dict):
        _validate_raw(run_queue, errors)
    try:
        queue = run_queue if isinstance(run_queue, EpisodeRunQueue) else EpisodeRunQueue.model_validate(run_queue)
    except ValidationError as exc:
        errors.extend(
            _error("model_validation_error", error["msg"], ".".join(str(part) for part in error["loc"]))
            for error in exc.errors()
        )
        return SetupValidationResult(valid=False, errors=errors, warnings=warnings)

    if not queue.runs:
        errors.append(_error("empty_runs", "runs must contain at least one run.", "runs"))

    seen_pair_ids: set[str] = set()
    for index, run in enumerate(queue.runs):
        if run.pair_id in seen_pair_ids:
            errors.append(_error("duplicate_pair_id", f"Duplicate pair_id: {run.pair_id}", f"runs[{index}].pair_id"))
        seen_pair_ids.add(run.pair_id)
        if not run.episode_setup.startswith("Json/Input/"):
            errors.append(
                _error(
                    "invalid_episode_setup_path",
                    "episode_setup must start with Json/Input/.",
                    f"runs[{index}].episode_setup",
                )
            )
        if not run.delivery_bot_setup.startswith("Json/Input/"):
            errors.append(
                _error(
                    "invalid_delivery_bot_setup_path",
                    "delivery_bot_setup must start with Json/Input/.",
                    f"runs[{index}].delivery_bot_setup",
                )
            )

    return SetupValidationResult(valid=not errors, errors=errors, warnings=warnings)
