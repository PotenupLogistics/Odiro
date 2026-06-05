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
    analyze_and_recommend,
    analyze_episode_and_recommend,
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "UE5 주행 로그 또는 EpisodeEvaluationReport를 분석해 "
            "DeliveryBotSetup 파라미터 조정을 추천합니다."
        )
    )
    subparsers = parser.add_subparsers(dest="mode", help="분석 모드")

    # 기존 MeasurementLog 모드 (하위호환)
    log_p = subparsers.add_parser("log", help="MeasurementLog_*.jsonl 분석 (기존)")
    log_p.add_argument("--log-path", required=True, help="MeasurementLog_*.jsonl 파일 경로")
    log_p.add_argument("--output-path", default=None, help="결과 JSON 저장 경로")
    log_p.add_argument("--provider", default="ollama", choices=[p.value for p in LlmProvider])
    log_p.add_argument("--fallback-only", action="store_true")

    # 신규 EpisodeEvaluationReport 모드
    ep_p = subparsers.add_parser("episode", help="EpisodeEvaluationReport.json 분석 (신규)")
    ep_p.add_argument("--report-path", required=True, help="EpisodeEvaluationReport JSON 경로")
    ep_p.add_argument("--bot-setup-path", default=None, help="현재 DeliveryBotSetup JSON 경로 (없으면 기본값)")
    ep_p.add_argument("--output-path", default=None, help="추천 결과 JSON 저장 경로")
    ep_p.add_argument("--next-setup-path", default=None, help="다음 실험용 DeliveryBotSetup 저장 경로")
    ep_p.add_argument("--provider", default="ollama", choices=[p.value for p in LlmProvider])
    ep_p.add_argument("--fallback-only", action="store_true")

    return parser


def _legacy_parser() -> argparse.ArgumentParser:
    """기존 --log-path 단독 실행 호환용."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--log-path", default=None)
    parser.add_argument("--output-path", default=None)
    parser.add_argument("--provider", default="ollama")
    parser.add_argument("--fallback-only", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    # 기존 --log-path 단독 사용 호환
    if argv is None:
        import sys as _sys
        argv = _sys.argv[1:]
    if argv and argv[0].startswith("--"):
        legacy_args, _ = _legacy_parser().parse_known_args(argv)
        if legacy_args.log_path:
            provider = LlmProvider(legacy_args.provider)
            result = analyze_and_recommend(
                log_path=legacy_args.log_path,
                provider=provider,
                fallback_only=legacy_args.fallback_only,
            )
            payload = result.model_dump(mode="json")
            text = json.dumps(payload, ensure_ascii=False, indent=2)
            if legacy_args.output_path:
                output = Path(legacy_args.output_path)
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_text(text, encoding="utf-8")
                print(f"추천 결과 저장: {output}")
            else:
                print(text)
            return 0

    args = _parser().parse_args(argv)
    provider = LlmProvider(args.provider)

    if args.mode == "log" or (hasattr(args, "log_path") and args.log_path):
        result = analyze_and_recommend(
            log_path=args.log_path,
            provider=provider,
            fallback_only=args.fallback_only,
        )
        payload = result.model_dump(mode="json")

    elif args.mode == "episode":
        result = analyze_episode_and_recommend(
            report_path=args.report_path,
            bot_setup_path=args.bot_setup_path,
            provider=provider,
            fallback_only=args.fallback_only,
        )
        payload = result.model_dump(mode="json")

        if args.next_setup_path and result.nextBotSetup:
            next_path = Path(args.next_setup_path)
            next_path.parent.mkdir(parents=True, exist_ok=True)
            next_path.write_text(
                json.dumps(result.nextBotSetup, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
            print(f"다음 실험용 DeliveryBotSetup 저장: {next_path}")
    else:
        _parser().print_help()
        return 1

    text = json.dumps(payload, ensure_ascii=False, indent=2)
    if args.output_path:
        output = Path(args.output_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        print(f"추천 결과 저장: {output}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
