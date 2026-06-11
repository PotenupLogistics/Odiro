from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.models.environment import EnvironmentSamplingRequest  # noqa: E402
from app.services.environment_parameter_sampler import (  # noqa: E402
    sample_environment_parameters,
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Sample a seed-deterministic numeric environment parameter set. "
            "No file is created unless --report is provided."
        )
    )
    parser.add_argument("--seed", required=True, type=int, help="Deterministic seed.")
    parser.add_argument(
        "--scenario-type",
        required=True,
        choices=[
            "narrow_sidewalk_kickboard_crossing",
            "obstacle_ahead",
            "pedestrian_crossing",
            "terrain_risk",
            "generic_sidewalk",
        ],
        help="Scenario type used for sampling tendencies.",
    )
    parser.add_argument(
        "--request-id",
        default="ENV-SAMPLE-CLI-001",
        help="Request id included in the result.",
    )
    parser.add_argument(
        "--fixed",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Fixed numeric parameter. Can be provided more than once.",
    )
    parser.add_argument("--json", action="store_true", help="Print full result as JSON.")
    parser.add_argument("--no-labels", action="store_true", help="Omit label hints.")
    parser.add_argument(
        "--report",
        help="Optional report output path. No file is created without this option.",
    )
    return parser


def _parse_fixed(items: list[str]) -> dict[str, Any]:
    fixed: dict[str, Any] = {}
    for item in items:
        if "=" not in item:
            raise ValueError(f"--fixed must use key=value format: {item}")
        key, value = item.split("=", 1)
        fixed[key] = value
    return fixed


def _print_text(result: dict[str, Any]) -> None:
    print(f"requestId: {result['requestId']}")
    print(f"seed: {result['seed']}")
    print(f"scenarioType: {result['scenarioType']}")
    print("parameters:")
    for key, value in result["parameters"].items():
        print(f"  {key}: {value}")
    if result["labelHints"]:
        print("labelHints:")
        for key, value in result["labelHints"].items():
            print(f"  {key}: {value}")
    if result["warnings"]:
        print("warnings:")
        for warning in result["warnings"]:
            print(f"  {warning['code']}: {warning['message']}")


def _write_report(path_text: str | None, payload: dict[str, Any]) -> None:
    if not path_text:
        return
    path = Path(path_text)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    args = _parser().parse_args()
    try:
        request = EnvironmentSamplingRequest(
            requestId=args.request_id,
            seed=args.seed,
            scenarioType=args.scenario_type,
            fixedParameters=_parse_fixed(args.fixed),
            includeLabels=not args.no_labels,
        )
        result = sample_environment_parameters(request)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    payload = result.model_dump(mode="json")
    _write_report(args.report, payload)
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        _print_text(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
