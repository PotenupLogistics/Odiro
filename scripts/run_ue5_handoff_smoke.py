from __future__ import annotations

import argparse
import json
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from fastapi.testclient import TestClient


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.main import app  # noqa: E402
from app.utils.handoff_response_summary import summarize_handoff_response  # noqa: E402
from app.utils.report_serialization import write_json_report  # noqa: E402


DEFAULT_PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Manual UE5 World Config handoff smoke runner. No report is created unless --report is set."
    )
    parser.add_argument("--provider", default="ollama", help="LLM provider. Defaults to ollama.")
    parser.add_argument("--prompt", default=DEFAULT_PROMPT, help="Natural-language scenario prompt.")
    parser.add_argument("--report", help="Optional JSON report path.")
    parser.add_argument("--print-world-config", action="store_true", help="Print worldConfig when handoff succeeds.")
    parser.add_argument("--dry-run", action="store_true", help="Print handoff request structure only; do not call generation.")
    parser.add_argument("--environment-sampling", action="store_true", help="Enable seed-based environment sampling constraints.")
    parser.add_argument("--scenario-type", default="generic_sidewalk", help="Environment sampling scenario type.")
    parser.add_argument("--seed", type=int, help="Environment sampling seed.")
    parser.add_argument("--fixed", action="append", default=[], help="Fixed environment parameter as key=value. Numeric values only.")
    return parser


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


def _environment_sampling_config(args: argparse.Namespace) -> dict[str, Any] | None:
    if not args.environment_sampling:
        return None
    return {
        "enabled": True,
        "seed": args.seed,
        "scenarioType": args.scenario_type,
        "fixedParameters": _parse_fixed(args.fixed),
    }


def _request_body(prompt: str, environment_sampling: dict[str, Any] | None = None) -> dict[str, Any]:
    constraints: dict[str, Any] = {
        "unitSystem": "cm_kmh_sec_degree",
        "allowedMapTypes": ["Sidewalk", "Crosswalk"],
        "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
        "fixedPolicyId": "policy_v1_basic_safety",
        "defaultSeed": 1001,
        "requireValidation": True,
    }
    if environment_sampling is not None:
        constraints["environmentSampling"] = environment_sampling
    return {
        "schemaVersion": "1.0",
        "requestId": "UE5-HANDOFF-SMOKE-001",
        "handoffTarget": "ue5",
        "includeDiagnostics": True,
        "generationRequest": {
            "schemaVersion": "1.0.0",
            "requestId": "GEN-UE5-HANDOFF-SMOKE-001",
            "generationType": "world_config",
            "targetContractType": "world_config",
            "prompt": prompt,
            "policyId": "policy_v1_basic_safety",
            "maxRepairAttempts": 1,
            "constraints": constraints,
        },
    }


def _ue5_readable_fields(world_config: dict[str, Any] | None) -> dict[str, bool]:
    payload = world_config or {}
    map_config = payload.get("map") if isinstance(payload.get("map"), dict) else {}
    robot = payload.get("robot") if isinstance(payload.get("robot"), dict) else {}
    runtime = payload.get("runtime") if isinstance(payload.get("runtime"), dict) else {}
    return {
        "map.lengthCm": "lengthCm" in map_config,
        "map.sidewalkWidthCm": "sidewalkWidthCm" in map_config,
        "robot.spawn": "spawn" in robot,
        "robot.goal": "goal" in robot,
        "obstacles": isinstance(payload.get("obstacles"), list),
        "pedestrians": isinstance(payload.get("pedestrians"), list),
        "runtime.maxDurationSec": "maxDurationSec" in runtime,
    }


def _report(provider: str, http_status: int, response_payload: dict[str, Any]) -> dict[str, Any]:
    summary = summarize_handoff_response(response_payload, http_status=http_status)
    post_processing = response_payload.get("postProcessing") or {}
    report = {
        "checkedAt": datetime.now(UTC).isoformat(),
        "provider": provider,
        "model": summary["model"],
        "httpStatus": summary["httpStatus"],
        "handoffSuccess": summary["success"],
        "worldConfigExists": summary["worldConfigExists"],
        "episodeSpecExists": summary["episodeSpecExists"],
        "effectiveResponseFormat": summary["effectiveResponseFormat"],
        "validationSummary": {
            "schemaValidationPassed": summary["schemaValidationPassed"],
            "scenarioReflectionPassed": summary["scenarioReflectionPassed"],
            "contractValidationPassed": summary["contractValidationPassed"],
        },
        "scenarioReflectionSummary": (response_payload.get("scenarioReflection") or {}).get("summary"),
        "postProcessingPatchTypes": summary["appliedPatches"],
        "summary": summary,
        "ue5ReadableFields": _ue5_readable_fields(response_payload.get("worldConfig")),
        "recommendedNextAction": (
            "UE5 can parse the handoff response in a controlled integration test."
            if response_payload.get("success")
            else "Do not hand off to UE5; inspect error and diagnostics."
        ),
    }
    return report


def _write_report(path_text: str | None, report: dict[str, Any]) -> None:
    if not path_text:
        return
    path = Path(path_text)
    path.parent.mkdir(parents=True, exist_ok=True)
    write_json_report(path, report)
    md_path = path.with_suffix(".md")
    lines = [
        "# UE5 World Config Handoff Smoke",
        "",
        f"- checkedAt: {report['checkedAt']}",
        f"- provider: {report['provider']}",
        f"- model: {report['model']}",
        f"- httpStatus: {report['httpStatus']}",
        f"- handoffSuccess: {report['handoffSuccess']}",
        f"- worldConfigExists: {report['worldConfigExists']}",
        f"- episodeSpecExists: {report['episodeSpecExists']}",
        f"- effectiveResponseFormat: {report['effectiveResponseFormat']}",
        f"- validationSummary: {report['validationSummary']}",
        f"- scenarioReflectionSummary: {report['scenarioReflectionSummary']}",
        f"- postProcessingPatchTypes: {', '.join(report['postProcessingPatchTypes'])}",
        f"- ue5ReadableFields: {report['ue5ReadableFields']}",
        f"- recommendedNextAction: {report['recommendedNextAction']}",
    ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = _parser().parse_args()
    try:
        environment_sampling = _environment_sampling_config(args)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    body = _request_body(args.prompt, environment_sampling)

    if args.dry_run:
        print("Dry run: handoff request structure only. Generation was not called.")
        print(json.dumps(body, ensure_ascii=False, indent=2))
        return 0

    client = TestClient(app)
    response = client.post(
        f"/api/v1/ue5/world-config/handoff?provider={args.provider}",
        json=body,
    )
    try:
        payload = response.json()
    except ValueError as exc:
        payload = {
            "success": False,
            "warnings": [{"code": "response_parse_failed", "message": str(exc)}],
        }
    report = _report(args.provider, response.status_code, payload)
    _write_report(args.report, report)

    print(f"handoffSuccess: {payload.get('success')}")
    print(f"worldConfigExists: {payload.get('worldConfig') is not None}")
    print(f"validationSummary: {payload.get('validation')}")
    print(f"scenarioReflectionSummary: {(payload.get('scenarioReflection') or {}).get('summary')}")
    print(f"postProcessingPatchTypes: {report['postProcessingPatchTypes']}")
    print(f"ue5ReadableFields: {report['ue5ReadableFields']}")
    if args.print_world_config and payload.get("worldConfig") is not None:
        print(json.dumps(payload["worldConfig"], ensure_ascii=False, indent=2))
    return 0 if response.status_code < 500 else 1


if __name__ == "__main__":
    raise SystemExit(main())
