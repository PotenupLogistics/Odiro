from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.models.llm import LlmProvider  # noqa: E402
from app.services.policy_recommendation_orchestrator import (  # noqa: E402
    analyze_full_setup_and_recommend,
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "UE5 주행 로그 또는 EpisodeEvaluationReport를 분석해 "
            "DeliveryBotSetup 파라미터 조정을 추천합니다."
        )
    )
    subparsers = parser.add_subparsers(dest="mode", help="분석 모드")

    # 통합 분석 모드 (외부입력 4개 + policy_server.py 내부 참조)
    full_p = subparsers.add_parser(
        "full",
        help="통합 분석: EvaluationReport + MeasurementLog + EpisodeSetup + DeliveryBotSetup (+ policy_server.py)",
    )
    full_p.add_argument("--evaluation-report", required=True, help="EpisodeEvaluationReport JSON 경로")
    full_p.add_argument("--measurement-log", required=True, help="MeasurementLog_*.jsonl 경로")
    full_p.add_argument("--episode-setup", required=True, help="EpisodeSetup JSON 경로")
    full_p.add_argument("--bot-setup", required=True, help="DeliveryBotSetup JSON 경로")
    full_p.add_argument("--policy-server", default=None, help="policy_server.py 경로 (선택)")
    full_p.add_argument("--output", default=None, help="통합 추천 결과 JSON 저장 경로")
    full_p.add_argument("--provider", default="openai", choices=[p.value for p in LlmProvider])
    full_p.add_argument("--fallback-only", action="store_true")

    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)

    if args.mode != "full":
        _parser().print_help()
        return 1

    provider = LlmProvider(args.provider)
    result = analyze_full_setup_and_recommend(
        evaluation_report_path=args.evaluation_report,
        measurement_log_path=args.measurement_log,
        episode_setup_path=args.episode_setup,
        bot_setup_path=args.bot_setup,
        policy_server_path=args.policy_server,
        provider=provider,
        fallback_only=args.fallback_only,
        output_path=args.output,
    )
    payload = result.model_dump(mode="json")
    text = json.dumps(payload, ensure_ascii=False, indent=2)
    if args.output:
        print(f"통합 추천 결과 저장: {args.output}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
