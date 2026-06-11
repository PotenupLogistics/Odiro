from __future__ import annotations

import json
from typing import Any

from app.models.scenario import ScenarioReflectionResult


def _format_issues(result: ScenarioReflectionResult) -> str:
    if not result.issues:
        return "- None"
    lines: list[str] = []
    for issue in result.issues:
        lines.extend(
            [
                f"- requirementId: {issue.requirementId}",
                f"  issueType: {issue.issueType}",
                f"  expectedPath: {issue.expectedPath}",
                f"  expectedValueHint: {issue.expectedValueHint}",
                f"  actualValueSummary: {issue.actualValueSummary}",
                f"  repairInstruction: {issue.repairInstruction}",
            ]
        )
    return "\n".join(lines)


def build_scenario_repair_prompt(
    previous_json: dict[str, Any],
    scenario_reflection_result: ScenarioReflectionResult,
    output_contract: str,
) -> str:
    return f"""The previous World Config JSON schema validation already passed.
Preserve the valid JSON structure and only repair missing semantic scenario requirements.

Previous JSON:
{json.dumps(previous_json, ensure_ascii=False, indent=2)}

Missing scenario requirements:
{_format_issues(scenario_reflection_result)}

Required scenario repair instructions:
- If Kickboard is required, add an obstacle with type "Kickboard".
- If path blocking is required, set the obstacle blockingRatio greater than 0.
- If pedestrian crossing is required, add at least one pedestrian with behavior "Crossing".
- Pedestrian crossing maps to pedestrians[].behavior.
- If narrow sidewalk is required, keep map.sidewalkWidthCm in a narrow sidewalk range.
- Do not add extra keys outside the schema.
- Preserve required fields already present in the previous JSON.
- Return one corrected JSON object only.
- Do not include markdown, comments, explanations, or extra keys.

{output_contract}
"""
