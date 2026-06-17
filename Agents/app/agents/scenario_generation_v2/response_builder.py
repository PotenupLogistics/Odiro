from __future__ import annotations

from typing import Any

from app.models.scenario_generation_v2 import ScenarioGenerateV2Response, V2ValidationResult


class ResponseBuilder:
    """Constructs API responses without storage paths or run-generation fields."""

    def success(
        self,
        *,
        scenario_id: str,
        summary: str,
        scenario: dict[str, Any],
        validation: V2ValidationResult,
        assumptions: list[str],
        generation_mode: str = "deterministic",
    ) -> ScenarioGenerateV2Response:
        """Return a successful project scenario response."""
        return ScenarioGenerateV2Response(
            status="success",
            scenario_id=scenario_id,
            summary=summary,
            scenario=scenario,
            validation=validation,
            assumptions=assumptions,
            generation_mode=generation_mode,
        )

    def failed(self, *, summary: str, validation: V2ValidationResult) -> ScenarioGenerateV2Response:
        """Return a failed response that contains only deterministic validation details."""
        return ScenarioGenerateV2Response(
            status="failed",
            scenario_id=None,
            summary=summary,
            scenario=None,
            validation=validation,
            assumptions=[],
        )
