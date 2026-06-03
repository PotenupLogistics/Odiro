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
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze a UE5 measurement log JSONL and recommend policy parameter "
            "adjustments. RAG+LLM 주도, RAG/LLM 실패 시 규칙 fallback."
        )
    )
    parser.add_argument(
        "--log-path",
        required=True,
        help="MeasurementLog_*.jsonl 파일 경로",
    )
    parser.add_argument(
        "--output-path",
        default=None,
        help="추천 JSON 결과 저장 경로. 미지정 시 stdout으로 출력.",
    )
    parser.add_argument(
        "--provider",
        default="ollama",
        choices=[p.value for p in LlmProvider],
        help="LLM 제공자. 기본 ollama.",
    )
    parser.add_argument(
        "--fallback-only",
        action="store_true",
        help="LLM/RAG 우회하고 규칙 기반 추천만 수행",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)

    provider = LlmProvider(args.provider)
    result = analyze_and_recommend(
        log_path=args.log_path,
        provider=provider,
        fallback_only=args.fallback_only,
    )

    payload = result.model_dump(mode="json")
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
