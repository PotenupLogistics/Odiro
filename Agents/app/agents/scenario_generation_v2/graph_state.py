from __future__ import annotations

from typing import Any, TypedDict


class ScenarioGenerationGraphStateV2(TypedDict, total=False):
    """Prompt-only LangGraph state for project scenario v1 generation."""

    request: dict[str, Any]
    prompt: str | None
    interpreted_intent: Any
    selected_pattern: str | None
    llm_template_candidate: dict[str, Any] | None
    llm_validation: Any
    llm_warnings: list[Any]
    scenario: dict[str, Any] | None
    validation: Any
    diagnostics: list[dict[str, Any]]
    repair_events: list[dict[str, Any]]
    repair_count: int
    status: str | None
    summary: str | None
    assumptions: list[str]
    response: Any
    output: Any
