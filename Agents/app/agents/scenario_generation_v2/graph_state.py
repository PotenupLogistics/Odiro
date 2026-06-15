from __future__ import annotations

from typing import Any, TypedDict


class ScenarioGenerationGraphStateV2(TypedDict, total=False):
    request: Any
    normalized_prompt: str
    intent: dict[str, Any]
    scenario_type: str
    plan: Any
    scenario_template: dict[str, Any]
    validation: Any
    response: Any
    warnings: list[str]
