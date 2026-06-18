from __future__ import annotations

import argparse
import json
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.agents.common.json_response_parser import parse_json_response  # noqa: E402
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2  # noqa: E402
from app.agents.scenario_generation_v2.scenario_template_schema import project_scenario_v1_response_schema  # noqa: E402
from app.agents.scenario_generation_v2.template_validator import TemplateValidator  # noqa: E402
from app.core.settings import Settings  # noqa: E402
from app.models.llm import LlmGenerationRequest  # noqa: E402
from app.models.llm import LlmProvider  # noqa: E402
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request  # noqa: E402
from app.services.llm_openai_client import OpenAILlmClient  # noqa: E402
from app.utils.report_serialization import to_jsonable, write_json_report  # noqa: E402


class TrackingOpenAIJsonClient:
    """Calls OpenAI for scenario JSON while preserving provider response metadata."""

    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.client = OpenAILlmClient(settings=settings)
        self.calls: list[dict[str, Any]] = []

    def generate_json(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        response_name: str,
        response_schema: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        """Generate JSON and store response id/model metadata for smoke reporting."""
        schema = response_schema or project_scenario_v1_response_schema()
        request = LlmGenerationRequest(
            provider=LlmProvider.openai,
            model=self.settings.openaiModel,
            systemPrompt=system_prompt,
            userPrompt=user_prompt,
            temperature=self.settings.openaiTemperature,
            maxTokens=self.settings.openaiMaxTokens,
            responseFormat="json_object",
            responseJsonSchema=schema.get("schema") if isinstance(schema.get("schema"), dict) else None,
            responseSchemaName=schema.get("name") if isinstance(schema.get("name"), str) else None,
            responseSchemaStrict=bool(schema.get("strict", False)),
            requestId=response_name,
            timeoutSec=self.settings.openaiTimeoutSec,
        )
        response = self.client.generate(request)
        raw_payload = _parse_raw_payload(response.rawContent)
        self.calls.append(
            {
                "requestId": response.requestId,
                "responseId": raw_payload.get("id") if isinstance(raw_payload.get("id"), str) else None,
                "provider": response.provider.value,
                "model": response.model,
                "success": response.success,
                "usage": response.usage.model_dump(mode="json") if response.usage else None,
                "error": response.error.model_dump(mode="json") if response.error else None,
            }
        )
        if not response.success or response.content is None:
            message = response.error.message if response.error else "LLM generation failed."
            raise ValueError(message)
        return parse_json_response(response.content)


def _parse_raw_payload(raw_content: str | None) -> dict[str, Any]:
    """Parse provider raw JSON for smoke metadata without failing the run."""
    if not raw_content:
        return {}
    try:
        payload = json.loads(raw_content)
    except json.JSONDecodeError:
        return {}
    return payload if isinstance(payload, dict) else {}


def _parser() -> argparse.ArgumentParser:
    """Build CLI arguments for manual OpenAI v2 scenario smoke runs."""
    parser = argparse.ArgumentParser(
        description=(
            "Run a manual OpenAI /api/v2/scenarios/generate scenario smoke. "
            "Dry-run does not call OpenAI. No report is created unless --report is provided."
        )
    )
    parser.add_argument("--prompt", required=True)
    parser.add_argument("--model")
    parser.add_argument("--report")
    parser.add_argument("--print-payload", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser


def _write_report(path_text: str | None, payload: dict[str, Any]) -> None:
    """Write an optional smoke report without creating artifacts by default."""
    if not path_text:
        return
    write_json_report(path_text, payload)


def _forbidden_external_fields() -> set[str]:
    """Return fields that must stay out of the external scenario body."""
    return {
        "analysis",
        "analysis_result",
        "episode_count",
        "generation_mode",
        "project_path",
        "run_id",
        "scenario_path",
        "seed",
        "status",
        "summary",
        "template_id",
        "validation",
    }


def _validate_external_body(scenario: dict[str, Any]) -> list[str]:
    """Validate smoke output against the public alpha scenario body contract."""
    messages: list[str] = []
    validation = TemplateValidator().validate(scenario)
    messages.extend(f"{issue.field}: {issue.message}" for issue in validation.errors)
    forbidden = _forbidden_external_fields().intersection(scenario)
    if forbidden:
        messages.append(f"forbidden root fields: {sorted(forbidden)}")
    if scenario.get("pedestrians", {}).get("background", {}).get("count") != 0:
        messages.append("pedestrians.background.count must be 0 for alpha smoke.")
    if scenario.get("pedestrians", {}).get("encounters") != []:
        messages.append("pedestrians.encounters must be [] for alpha smoke.")
    return messages


def main() -> int:
    """Run dry or live OpenAI smoke validation for v2 scenario generation."""
    args = _parser().parse_args()
    settings = Settings(_env_file=".env")
    settings.llmProvider = LlmProvider.openai
    settings.llmProviderChain = ["openai"]
    settings.v2AgentLlmEnabled = True
    if args.model:
        settings.openaiModel = args.model

    if args.dry_run:
        payload = {
            "checkedAt": datetime.now(UTC).isoformat(),
            "dryRun": True,
            "openaiCalled": False,
            "model": settings.openaiModel,
            "prompt": args.prompt,
            "endpoint": "/api/v2/scenarios/generate",
        }
        _write_report(args.report, payload)
        print(json.dumps(to_jsonable(payload), ensure_ascii=False, indent=2))
        return 0

    tracking_client = TrackingOpenAIJsonClient(settings)
    runner = ScenarioGenerationGraphRunnerV2(settings=settings, llm_client=tracking_client)
    response = runner.run(ScenarioGenerateV2Request(prompt=args.prompt))
    scenario = response.scenario or {}
    errors = _validate_external_body(scenario)
    fallback_used = any(
        "fallback" in str(warning.message).lower()
        for warning in response.validation.warnings
    )
    payload = {
        "checkedAt": datetime.now(UTC).isoformat(),
        "dryRun": False,
        "openaiCalled": True,
        "model": settings.openaiModel,
        "openaiCalls": tracking_client.calls,
        "openaiResponseId": next((call.get("responseId") for call in tracking_client.calls if call.get("responseId")), None),
        "generationMode": response.generation_mode,
        "fallbackUsed": fallback_used,
        "valid": not errors,
        "errors": errors,
        "scenarioId": scenario.get("scenario_id"),
        "obstacleCount": len(scenario.get("obstacles", {}).get("placements", [])) if isinstance(scenario, dict) else None,
        "pedestrianBackgroundCount": (
            scenario.get("pedestrians", {}).get("background", {}).get("count") if isinstance(scenario, dict) else None
        ),
        "pedestrianEncounterCount": (
            len(scenario.get("pedestrians", {}).get("encounters", [])) if isinstance(scenario, dict) else None
        ),
        "robotStartType": scenario.get("robot", {}).get("start", {}).get("type") if isinstance(scenario, dict) else None,
        "robotGoalType": scenario.get("robot", {}).get("goal", {}).get("type") if isinstance(scenario, dict) else None,
        "scenario": scenario,
    }
    _write_report(args.report, payload)
    if args.print_payload:
        print(json.dumps(to_jsonable(scenario), ensure_ascii=False, indent=2))
    else:
        summary_payload = {key: value for key, value in payload.items() if key != "scenario"}
        print(json.dumps(to_jsonable(summary_payload), ensure_ascii=False, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
