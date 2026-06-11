from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.models.rag import RagSearchQuery
from app.services.policy_rag_retriever import search_policy_chunks


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Search deterministic policy RAG chunks.")
    parser.add_argument("--query", default="", help="Keyword query.")
    parser.add_argument("--top-k", type=int, default=5)
    parser.add_argument("--category", action="append", dest="categoryFilter")
    parser.add_argument("--action", action="append", dest="actionFilter")
    parser.add_argument("--param", action="append", dest="policyParamFilter")
    parser.add_argument("--source", action="append", dest="sourceFilter")
    parser.add_argument("--json", action="store_true", help="Print JSON output.")
    parser.add_argument("--report", help="Optional path to write a retrieval report JSON.")
    return parser


def _write_report(path: str, result: object) -> None:
    payload = {
        "query": result.query.query,
        "filters": {
            "categoryFilter": result.query.categoryFilter,
            "actionFilter": result.query.actionFilter,
            "policyParamFilter": result.query.policyParamFilter,
            "sourceFilter": result.query.sourceFilter,
        },
        "topK": result.query.topK,
        "resultCount": result.resultCount,
        "results": [item.model_dump(mode="json") for item in result.results],
        "searchedAt": datetime.now(timezone.utc).isoformat(),
    }
    report_path = Path(path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _print_text(result: object) -> None:
    print(f"resultCount: {result.resultCount}")
    for item in result.results:
        snippet = " ".join(item.chunkText.split())[:240]
        print(f"- {item.chunkId}")
        print(f"  category: {item.metadata.category}")
        print(f"  score: {item.score}")
        print(f"  evidenceLocation: {item.metadata.evidenceLocation}")
        print(f"  relatedActions: {', '.join(item.metadata.relatedActions)}")
        print(f"  relatedPolicyParams: {', '.join(item.metadata.relatedPolicyParams)}")
        print(f"  chunkText: {snippet}")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    query = RagSearchQuery(
        query=args.query,
        topK=args.top_k,
        categoryFilter=args.categoryFilter,
        actionFilter=args.actionFilter,
        policyParamFilter=args.policyParamFilter,
        sourceFilter=args.sourceFilter,
    )
    result = search_policy_chunks(query)
    if args.json:
        print(json.dumps(result.model_dump(mode="json"), ensure_ascii=False, indent=2))
    else:
        _print_text(result)
    if args.report:
        _write_report(args.report, result)
        print(f"Report written to: {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
