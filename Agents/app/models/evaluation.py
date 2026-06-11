from __future__ import annotations

from pydantic import BaseModel, ConfigDict


class EvaluationSpec(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    evaluationId: str
    baseScore: float
    weights: dict[str, float]
    penalties: dict[str, float]
    bonuses: dict[str, float]
    requiredMetrics: list[str]
