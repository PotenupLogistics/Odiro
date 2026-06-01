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
from app.utils.report_serialization import to_jsonable, write_json_report  # noqa: E402


DEFAULT_PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run a manual UE5 EpisodeSpec controlled smoke. No report is created unless --report is set."
    )
    parser.add_argument("--provider", default="ollama")
    parser.add_argument("--response-format", choices=["episode_spec", "both"], default="episode_spec")
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--report")
    parser.add_argument("--print-episode-spec", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser


def _request_body(prompt: str, response_format: str) -> dict[str, Any]:
    return {
        "schemaVersion": "1.0",
        "requestId": "UE5-EPISODE-CONTROLLED-SMOKE-001",
        "handoffTarget": "ue5",
        "includeDiagnostics": True,
        "responseFormat": response_format,
        "generationRequest": {
            "schemaVersion": "1.0.0",
            "requestId": "GEN-UE5-EPISODE-CONTROLLED-SMOKE-001",
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


def _report(provider: str, response_format: str, status_code: int, payload: dict[str, Any]) -> dict[str, Any]:
    summary = summarize_handoff_response(payload, http_status=status_code)
    reflection = payload.get("episodeScenarioReflection") or {}
    episode = payload.get("episodeSpec") or {}
    actors = episode.get("actors") or {}
    return {
        "checkedAt": datetime.now(UTC).isoformat(),
        "provider": provider,
        "model": summary["model"],
        "responseFormat": response_format,
        "effectiveResponseFormat": summary["effectiveResponseFormat"],
        "httpStatus": status_code,
        "handoffSuccess": summary["success"],
        "worldConfigExists": summary["worldConfigExists"],
        "episodeSpecExists": summary["episodeSpecExists"],
        "episodeValidationPassed": summary["episodeValidationPassed"],
        "episodeScenarioReflectionPassed": summary["episodeScenarioReflectionPassed"],
        "staticObstacleCount": reflection.get("staticObstacleCount", summary["staticObstacleCount"]),
        "hasKickboardSemantic": reflection.get("hasKickboardSemantic", False),
        "hasBlockingRatio": reflection.get("hasBlockingRatio", False),
        "pedestrianCount": reflection.get("pedestrianCount", summary["pedestrianCount"]),
        "pathCount": reflection.get("pathCount", summary["pathCount"]),
        "summary": summary,
        "pedestrianPathLinked": reflection.get("pedestrianPathLinked", False),
        "hasCrossingPedestrian": reflection.get("hasCrossingPedestrian", False),
        "sidewalkWidthM": reflection.get("sidewalkWidthM"),
        "conversionWarnings": payload.get("conversionWarnings") or [],
        "episodeScenarioIssues": reflection.get("issues") or [],
        "ueCompilerReadiness": reflection.get("ueCompilerReadiness", False),
        "recommendedNextAction": (
            "Proceed to UE controlled integration."
            if payload.get("success") and reflection.get("ueCompilerReadiness")
            else "Do not proceed to UE controlled integration until scenario reflection passes."
        ),
    }


def _write_report(path_text: str | None, report: dict[str, Any]) -> None:
    if not path_text:
        return
    path = Path(path_text)
    path.parent.mkdir(parents=True, exist_ok=True)
    write_json_report(path, report)
    md_path = path.with_suffix(".md")
    lines = [
        "# UE5 EpisodeSpec Controlled Scenario Smoke",
        "",
        f"- checkedAt: {report['checkedAt']}",
        f"- provider: {report['provider']}",
        f"- model: {report['model']}",
        f"- responseFormat: {report['responseFormat']}",
        f"- handoffSuccess: {report['handoffSuccess']}",
        f"- episodeSpecExists: {report['episodeSpecExists']}",
        f"- episodeValidationPassed: {report['episodeValidationPassed']}",
        f"- episodeScenarioReflectionPassed: {report['episodeScenarioReflectionPassed']}",
        f"- staticObstacleCount: {report['staticObstacleCount']}",
        f"- hasKickboardSemantic: {report['hasKickboardSemantic']}",
        f"- hasBlockingRatio: {report['hasBlockingRatio']}",
        f"- pedestrianCount: {report['pedestrianCount']}",
        f"- pathCount: {report['pathCount']}",
        f"- pedestrianPathLinked: {report['pedestrianPathLinked']}",
        f"- hasCrossingPedestrian: {report['hasCrossingPedestrian']}",
        f"- sidewalkWidthM: {report['sidewalkWidthM']}",
        f"- ueCompilerReadiness: {report['ueCompilerReadiness']}",
        f"- recommendedNextAction: {report['recommendedNextAction']}",
    ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = _parser().parse_args()
    body = _request_body(args.prompt, args.response_format)
    if args.dry_run:
        print("Dry run: controlled smoke request only. Generation was not called.")
        print(json.dumps(body, ensure_ascii=False, indent=2))
        return 0

    client = TestClient(app)
    response = client.post(
        f"/api/v1/ue5/world-config/handoff?provider={args.provider}&responseFormat={args.response_format}",
        json=body,
    )
    try:
        payload = response.json()
    except ValueError as exc:
        payload = {
            "success": False,
            "warnings": [{"code": "response_parse_failed", "message": str(exc)}],
        }
    report = _report(args.provider, args.response_format, response.status_code, payload)
    _write_report(args.report, report)
    print(json.dumps(to_jsonable(report), ensure_ascii=False, indent=2))
    if args.print_episode_spec and payload.get("episodeSpec") is not None:
        print(json.dumps(payload["episodeSpec"], ensure_ascii=False, indent=2))
    return 0 if response.status_code < 500 else 1


if __name__ == "__main__":
    raise SystemExit(main())
