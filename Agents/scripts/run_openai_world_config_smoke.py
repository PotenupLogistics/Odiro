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

from app.core.settings import Settings  # noqa: E402
from app.models.generation import WorldConfigGenerationConstraints, WorldConfigGenerationRequest  # noqa: E402
from app.models.llm import LlmProvider  # noqa: E402
from app.services.llm_provider_policy import get_provider_chain, get_provider_status  # noqa: E402
from app.services.world_config_generation_orchestrator import generate_world_config  # noqa: E402
from app.services.world_config_prompt_builder import build_world_config_prompt_package  # noqa: E402
from app.utils.report_serialization import to_jsonable, write_json_report  # noqa: E402


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Run a manual OpenAI WorldConfig smoke. Dry-run does not call OpenAI. "
            "No report is created unless --report is provided."
        )
    )
    parser.add_argument("--prompt", required=True)
    parser.add_argument("--model")
    parser.add_argument("--max-repair-attempts", type=int)
    parser.add_argument("--report")
    parser.add_argument("--print-payload", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser


def _request(prompt: str, max_repair_attempts: int) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0.0",
        requestId="OPENAI-WORLD-CONFIG-SMOKE-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=prompt,
        policyId="policy_v1_basic_safety",
        maxRepairAttempts=max_repair_attempts,
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["Sidewalk", "Crosswalk"],
            allowedObjectTypes=["Pedestrian", "Kickboard", "Obstacle"],
            fixedPolicyId="policy_v1_basic_safety",
            defaultSeed=1001,
            requireValidation=True,
        ),
    )


def _write_report(path_text: str | None, payload: dict[str, Any]) -> None:
    if not path_text:
        return
    write_json_report(path_text, payload)


def main() -> int:
    args = _parser().parse_args()
    settings = Settings(_env_file=".env")
    settings.llmProvider = LlmProvider.openai
    settings.llmProviderChain = ["openai"]
    if args.model:
        settings.openaiModel = args.model
    if args.max_repair_attempts is not None:
        settings.openaiMaxRepairAttempts = args.max_repair_attempts
    request = _request(args.prompt, args.max_repair_attempts or settings.openaiMaxRepairAttempts)

    if args.dry_run:
        package = build_world_config_prompt_package(request)
        payload = {
            "checkedAt": datetime.now(UTC).isoformat(),
            "dryRun": True,
            "providerChain": [provider.value for provider in get_provider_chain(settings)],
            "providerStatuses": [
                get_provider_status(provider, settings).model_dump(mode="json")
                for provider in get_provider_chain(settings)
            ],
            "requestId": request.requestId,
            "model": settings.openaiModel,
            "retrievedContextCount": len(package.retrievedContexts),
            "warnings": package.warnings,
            "openaiCalled": False,
        }
        _write_report(args.report, payload)
        print(json.dumps(to_jsonable(payload), ensure_ascii=False, indent=2))
        return 0

    result = generate_world_config(
        request,
        provider=LlmProvider.openai,
        settings=settings,
    )
    payload = result.model_dump(mode="json")
    _write_report(args.report, payload)
    if args.print_payload:
        print(json.dumps(to_jsonable(payload), ensure_ascii=False, indent=2))
    else:
        print(
            json.dumps(
                {
                    "requestId": result.requestId,
                    "success": result.success,
                    "error": result.error.model_dump(mode="json") if result.error else None,
                    "fallbackTrace": [trace.model_dump(mode="json") for trace in result.fallbackTrace],
                },
                ensure_ascii=False,
                indent=2,
            )
        )
    return 0 if result.success else 1


if __name__ == "__main__":
    raise SystemExit(main())
