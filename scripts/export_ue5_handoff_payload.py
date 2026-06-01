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
        choices=["world_config", "episode_spec", "both"],
        default="world_config",
        help="Output world_config, episode_spec, or the full handoff response with both.",
    )
    parser.add_argument(
        "--include-diagnostics",
        action="store_true",
        help="Include diagnostics in the handoff response.",
    )
    parser.add_argument("--out", help="Optional output file path. No file is created without this option.")
    return parser


def _request_body(prompt: str, include_diagnostics: bool, response_format: str) -> dict[str, Any]:
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
            "constraints": {
                "unitSystem": "cm_kmh_sec_degree",
                "allowedMapTypes": ["Sidewalk", "Crosswalk"],
                "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
                "fixedPolicyId": "policy_v1_basic_safety",
                "defaultSeed": 1001,
                "requireValidation": True,
            },
        },
    }


def _write_optional(path_text: str | None, payload: dict[str, Any] | None) -> None:
    if not path_text:
        return
    write_json_report(path_text, payload)


def main() -> int:
    args = _parser().parse_args()
    client = TestClient(app)
    response = client.post(
        f"/api/v1/ue5/world-config/handoff?provider={args.provider}",
        json=_request_body(args.prompt, args.include_diagnostics, args.format),
    )
    handoff_payload = response.json()
    if args.world_config_only:
        output_payload = handoff_payload.get("worldConfig")
    elif args.format == "world_config":
        output_payload = handoff_payload.get("worldConfig")
    elif args.format == "episode_spec":
        output_payload = handoff_payload.get("episodeSpec")
    else:
        output_payload = handoff_payload

    _write_optional(args.out, output_payload)
    print(json.dumps(to_jsonable(output_payload), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
