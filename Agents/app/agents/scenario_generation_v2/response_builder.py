from __future__ import annotations

from typing import Any

from app.models.scenario_generation_v2 import ScenarioGenerateV2Response, V2ValidationResult


class ResponseBuilder:
    """Constructs API responses without storage paths or run-generation fields."""

    def success(
        self,
        *,
        template_id: str,
        summary: str,
        template: dict[str, Any],
        validation: V2ValidationResult,
        assumptions: list[str],
        generation_mode: str = "deterministic",
    ) -> ScenarioGenerateV2Response:
        """Return a successful scenario_template response."""
        return ScenarioGenerateV2Response(
            status="success",
            template_id=template_id,
            summary=summary,
            template=template,
            validation=validation,
            assumptions=assumptions,
            generation_mode=generation_mode,
        )

    def failed(self, *, summary: str, validation: V2ValidationResult) -> ScenarioGenerateV2Response:
        """Return a failed response that contains only deterministic validation details."""
        return ScenarioGenerateV2Response(
            status="failed",
            template_id=None,
            summary=summary,
            template=None,
            validation=validation,
            assumptions=[],
        )
