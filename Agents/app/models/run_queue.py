from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


class EpisodeRunQueueItem(BaseModel):
    model_config = ConfigDict(extra="forbid")

    pair_id: str
    episode_setup: str
    delivery_bot_setup: str


class EpisodeRunQueue(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["episode_run_queue"] = Field(default="episode_run_queue", alias="schema")
    version: Literal[1] = 1
    runs: list[EpisodeRunQueueItem] = Field(default_factory=list)

    @property
    def schema(self) -> str:
        return self.schema_
