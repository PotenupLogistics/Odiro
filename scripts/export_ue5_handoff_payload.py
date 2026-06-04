from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from fastapi.testclient import TestClient


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.main import app  # noqa: E402
from app.utils.report_serialization import to_jsonable, write_json_report  # noqa: E402


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Export a UE5 handoff response or worldConfig. "
            "No file is created unless --out is provided."
        )
    )
    parser.add_argument("--prompt", required=True, help="Natural-language scenario prompt.")
    parser.add_argument("--provider", default="ollama", help="Provider name. Defaults to ollama.")
    parser.add_argument(
        "--world-config-only",
        action="store_true",
        help="Print or write only worldConfig instead of the full handoff response.",
    )
    parser.add_argument(
        "--format",
        choices=["world_config", "episode_spec", "both", "setup_pair"],
        default="world_config",
        help="Output world_config, episode_spec, setup_pair, or the full handoff response with both.",
    )
    parser.add_argument(
        "--include-diagnostics",
        action="store_true",
        help="Include diagnostics in the handoff response.",
    )
    parser.add_argument("--out", help="Optional output file path. No file is created without this option.")
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


def _request_body(
    prompt: str,
    include_diagnostics: bool,
    response_format: str,
    environment_sampling: dict[str, Any] | None = None,
) -> dict[str, Any]:
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
        "requestId": "UE5-HANDOFF-EXPORT-001",
        "handoffTarget": "ue5",
        "includeDiagnostics": include_diagnostics,
        "responseFormat": response_format,
        "generationRequest": {
            "schemaVersion": "1.0.0",
            "requestId": "GEN-UE5-HANDOFF-EXPORT-001",
            "generationType": "world_config",
            "targetContractType": "world_config",
            "prompt": prompt,
            "policyId": "policy_v1_basic_safety",
            "maxRepairAttempts": 1,
            "constraints": constraints,
        },
    }


def _write_optional(path_text: str | None, payload: dict[str, Any] | None) -> None:
    if not path_text:
        return
    write_json_report(path_text, payload)


def main() -> int:
    args = _parser().parse_args()
    client = TestClient(app)
    try:
        environment_sampling = _environment_sampling_config(args)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    response = client.post(
        f"/api/v1/ue5/world-config/handoff?provider={args.provider}",
        json=_request_body(args.prompt, args.include_diagnostics, args.format, environment_sampling),
    )
    handoff_payload = response.json()
    if args.world_config_only:
        output_payload = handoff_payload.get("worldConfig")
    elif args.format == "world_config":
        output_payload = handoff_payload.get("worldConfig")
    elif args.format == "episode_spec":
        output_payload = handoff_payload.get("episodeSpec")
    elif args.format == "setup_pair":
        output_payload = {
            "episodeSetup": handoff_payload.get("episodeSetup"),
            "deliveryBotSetup": handoff_payload.get("deliveryBotSetup"),
        }
    else:
        output_payload = handoff_payload

    _write_optional(args.out, output_payload)
    print(json.dumps(to_jsonable(output_payload), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
