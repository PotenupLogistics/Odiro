from __future__ import annotations

from typing import Any

from app.core.settings import Settings
from pydantic import BaseModel, ConfigDict, Field, field_validator


def _add_episode_count_schema_limits(schema: dict[str, Any]) -> None:
    episode_count = schema.get("properties", {}).get("episode_count")
    if not isinstance(episode_count, dict):
        return
    variants = episode_count.get("anyOf")
    candidates = variants if isinstance(variants, list) else [episode_count]
    for candidate in candidates:
        if isinstance(candidate, dict) and candidate.get("type") == "integer":
            candidate["maximum"] = Settings().scenarioEpisodeMaxCount


class ScenarioGenerateRequest(BaseModel):
    model_config = ConfigDict(extra="forbid", json_schema_extra=_add_episode_count_schema_limits)

    prompt: str = Field(min_length=1)
    episode_count: int | None = Field(default=None, ge=1, strict=True)

    @field_validator("prompt")
    @classmethod
    def reject_blank_prompt(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("prompt must not be empty.")
        return value

    @field_validator("episode_count")
    @classmethod
    def reject_episode_count_above_max(cls, value: int | None) -> int | None:
        if value is not None and value > Settings().scenarioEpisodeMaxCount:
            raise ValueError(f"episode_count must be <= {Settings().scenarioEpisodeMaxCount}.")
        return value

