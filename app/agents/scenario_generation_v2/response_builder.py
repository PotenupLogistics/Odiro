from __future__ import annotations

from typing import Any

from app.models.scenario_generation_v2 import ScenarioGenerateV2Response, V2ValidationResult


class ResponseBuilder:
    def success(
        self,
        *,
        scenario_id: str,
        summary: str,
        scenario_template: dict[str, Any],
        validation: V2ValidationResult,
        assumptions: list[str],
        generation_mode: str = "deterministic",
    ) -> ScenarioGenerateV2Response:
        return ScenarioGenerateV2Response(
            status="success",
            scenario_id=scenario_id,
            summary=summary,
            scenario_template=scenario_template,
            validation=validation,
            assumptions=assumptions,
            generation_mode=generation_mode,
        )

    def failed(self, *, summary: str, validation: V2ValidationResult) -> ScenarioGenerateV2Response:
        return ScenarioGenerateV2Response(
            status="failed",
            scenario_id=None,
            summary=summary,
            scenario_template=None,
            validation=validation,
            assumptions=[],
        )
