from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.core.settings import Settings  # noqa: E402
from app.models.generation import WorldConfigGenerationRequest  # noqa: E402
from app.models.llm import LlmProvider  # noqa: E402
from app.services.world_config_generation_orchestrator import generate_world_config  # noqa: E402
from app.services.run_queue_export_service import export_run_queue_package  # noqa: E402
from app.services.setup_pair_queue_generator import generate_setup_pair_queue  # noqa: E402
from app.utils.report_serialization import to_jsonable  # noqa: E402
from app.utils.run_queue_summary import summarize_run_queue_result  # noqa: E402


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Export a UE contract RunQueue package to a local ignored directory.")
    parser.add_argument("--request-json", help="Optional request JSON file containing prompt/worldConfig.")
    parser.add_argument("--prompt", help="Natural-language scenario prompt.")
    parser.add_argument("--provider", choices=["openai"], default="openai", help="Live prompt provider. Defaults to openai.")
    parser.add_argument("--scenario-type", default="obstacle_ahead", help="Environment sampling scenario type.")
    parser.add_argument("--fixed", action="append", default=[], help="Fixed environment parameter as key=value. Numeric values only.")
    parser.add_argument("--episode-count", type=int, help="Episode pair count. Defaults to settings.")
    parser.add_argument("--base-seed", type=int, help="Base seed for deterministic variants.")
    parser.add_argument("--output-dir", help="Optional export root. Defaults to data/run_queue_exports/<timestamp>_<requestId>.")
    parser.add_argument("--dry-run", action="store_true", help="Build and validate the package without writing files.")
    return parser


def _load_request_json(path_text: str) -> dict:
    payload = json.loads(Path(path_text).read_text(encoding="utf-8"))
    if isinstance(payload.get("worldConfig"), dict):
        return payload["worldConfig"]
    if isinstance(payload.get("world_config"), dict):
        return payload["world_config"]
    return payload


def _parse_fixed(values: list[str]) -> dict[str, int | float]:
    fixed: dict[str, int | float] = {}
    for value in values:
        if "=" not in value:
            raise ValueError("--fixed must use key=value format.")
        key, raw = value.split("=", 1)
        normalized = raw.strip().lower()
        if normalized in {"low", "middle", "high"}:
            raise ValueError("low/middle/high labels are not allowed for --fixed.")
        fixed[key] = float(raw) if "." in raw else int(raw)
    return fixed


def _generation_request(args: argparse.Namespace) -> WorldConfigGenerationRequest:
    fixed = _parse_fixed(args.fixed)
    seed = args.base_seed if args.base_seed is not None else 1001
    return WorldConfigGenerationRequest(
        schemaVersion="1.0.0",
        requestId="GEN-UE5-RUN-QUEUE-EXPORT-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=args.prompt,
        policyId="policy_v1_basic_safety",
        maxRepairAttempts=0,
        constraints={
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["Sidewalk", "Crosswalk"],
            "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
            "fixedPolicyId": "policy_v1_basic_safety",
            "defaultSeed": seed,
            "requireValidation": True,
            "environmentSampling": {
                "enabled": True,
                "seed": seed,
                "scenarioType": args.scenario_type,
                "fixedParameters": fixed,
            },
        },
    )


def _generate_live_world_config(args: argparse.Namespace) -> tuple[dict[str, Any] | None, dict[str, Any]]:
    settings = Settings(llmAllowOpenaiFallback=False, llmProviderChain=["openai"], llmMaxTotalAttempts=1)
    request = _generation_request(args)
    result = generate_world_config(
        request,
        provider=LlmProvider.openai,
        settings=settings,
        allow_fallback=False,
    )
    metadata = {
        "openaiCallCount": len(result.attempts),
        "providerUsed": "openai",
        "fallbackUsed": bool(result.fallbackTrace),
        "fallbackTrace": [trace.model_dump(mode="json") for trace in result.fallbackTrace],
        "generationSuccess": result.success,
        "generationErrorCode": result.error.code if result.error else None,
        "generationErrorMessage": result.error.message if result.error else None,
        "attemptPromptTypes": [attempt.promptType for attempt in result.attempts],
    }
    return result.generatedPayload, metadata


def main() -> int:
    args = _parser().parse_args()
    live_metadata: dict[str, Any] = {
        "openaiCallCount": 0,
        "providerUsed": None,
        "fallbackUsed": False,
        "fallbackTrace": [],
    }
    if args.request_json:
        world_config = _load_request_json(args.request_json)
    elif args.prompt:
        try:
            world_config, live_metadata = _generate_live_world_config(args)
        except ValueError as exc:
            print(str(exc), file=sys.stderr)
            return 2
        if world_config is None:
            print(json.dumps(to_jsonable(live_metadata), ensure_ascii=False, indent=2), file=sys.stderr)
            return 1
    else:
        print("--request-json or --prompt is required.", file=sys.stderr)
        return 2

    queue = generate_setup_pair_queue(
        world_config,
        episode_count=args.episode_count,
        base_seed=args.base_seed,
        request_id="UE5-RUN-QUEUE-EXPORT-001",
    )
    if args.dry_run:
        summary = summarize_run_queue_result(queue, export_path=None)
        summary.update(live_metadata)
        summary["fakeModeUsed"] = False
        summary["dryRunUsed"] = True
        print(json.dumps(to_jsonable(summary), ensure_ascii=False, indent=2))
        return 0 if queue.run_queue_validation.valid else 1

    export = export_run_queue_package(queue, output_dir=args.output_dir)
    summary = summarize_run_queue_result(queue, export_path=str(export.export_root) if export.exported else None)
    summary.update(live_metadata)
    summary["fakeModeUsed"] = False
    summary["dryRunUsed"] = False
    print(
        json.dumps(
            to_jsonable(summary),
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0 if export.exported else 1


if __name__ == "__main__":
    raise SystemExit(main())
