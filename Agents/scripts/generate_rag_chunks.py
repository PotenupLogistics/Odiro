from __future__ import annotations

import argparse
import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
DEFAULT_OUTPUT_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
DEFAULT_REPORT_JSON_PATH = ROOT / "data" / "rag" / "policy_rag_chunks_report.json"
DEFAULT_REPORT_MD_PATH = ROOT / "data" / "rag" / "policy_rag_chunks_report.md"


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
        encoding="utf-8",
    )


def _as_list_text(value: Any) -> str:
    if isinstance(value, list):
        return ", ".join(str(item) for item in value)
    return str(value or "")


def build_chunk_text(card: dict[str, Any]) -> str:
    sections = [
        f"category: {card.get('category', '')}",
        f"principle: {card.get('principle', '')}",
        f"projectRule: {card.get('projectRule', '')}",
        f"evidenceText: {card.get('evidenceText', '')}",
        f"evidenceLocation: {card.get('evidenceLocation', '')}",
        f"relatedPolicyParams: {_as_list_text(card.get('relatedPolicyParams'))}",
        f"relatedRequestFields: {_as_list_text(card.get('relatedRequestFields'))}",
        f"relatedActions: {_as_list_text(card.get('relatedActions'))}",
        f"relatedMetrics: {_as_list_text(card.get('relatedMetrics'))}",
        f"caution: {card.get('caution', '')}",
    ]
    return "\n".join(section for section in sections if section.split(": ", 1)[-1])


def build_chunk(card: dict[str, Any]) -> dict[str, Any]:
    card_id = card["cardId"]
    return {
        "chunkId": f"CHUNK-{card_id}",
        "cardId": card_id,
        "chunkText": build_chunk_text(card),
        "metadata": {
            "sourceIds": card.get("sourceIds", []),
            "category": card.get("category", ""),
            "relatedPolicyParams": card.get("relatedPolicyParams", []),
            "relatedRequestFields": card.get("relatedRequestFields", []),
            "relatedActions": card.get("relatedActions", []),
            "relatedMetrics": card.get("relatedMetrics", []),
            "evidenceLocation": card.get("evidenceLocation", ""),
            "createdFromCandidateId": card.get("createdFromCandidateId", ""),
            "status": "confirmed_policy_card",
        },
    }


def _build_report(cards: list[dict[str, Any]], chunks: list[dict[str, Any]]) -> dict[str, Any]:
    missing_metadata_warnings: list[str] = []
    for chunk in chunks:
        metadata = chunk["metadata"]
        for field in ["sourceIds", "category", "relatedPolicyParams", "relatedActions", "evidenceLocation"]:
            value = metadata.get(field)
            if value in ("", [], None):
                missing_metadata_warnings.append(f"{chunk['chunkId']} missing {field}")
    return {
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "inputCardCount": len(cards),
        "generatedChunkCount": len(chunks),
        "chunksByCategory": dict(sorted(Counter(chunk["metadata"]["category"] for chunk in chunks).items())),
        "chunksBySource": dict(
            sorted(Counter(source_id for chunk in chunks for source_id in chunk["metadata"]["sourceIds"]).items())
        ),
        "missingMetadataWarnings": missing_metadata_warnings,
        "nextStepRecommendation": [
            "embedding index 생성 준비",
            "retrieval test 설계",
            "source document RAG는 별도 단계",
        ],
    }


def _write_report_md(path: Path, report: dict[str, Any]) -> None:
    lines = [
        "# Policy RAG Chunks Report",
        "",
        f"- generatedAt: {report['generatedAt']}",
        f"- inputCardCount: {report['inputCardCount']}",
        f"- generatedChunkCount: {report['generatedChunkCount']}",
        "",
        "## chunksByCategory",
    ]
    lines.extend([f"- {category}: {count}" for category, count in report["chunksByCategory"].items()])
    lines.extend(["", "## chunksBySource"])
    lines.extend([f"- {source_id}: {count}" for source_id, count in report["chunksBySource"].items()])
    lines.extend(["", "## nextStepRecommendation"])
    lines.extend([f"- {item}" for item in report["nextStepRecommendation"]])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_rag_chunks(
    input_path: Path,
    output_path: Path,
    report_json_path: Path,
    report_md_path: Path,
) -> dict[str, Any]:
    cards = _read_jsonl(input_path)
    chunks = [build_chunk(card) for card in cards]
    _write_jsonl(output_path, chunks)
    report = _build_report(cards, chunks)
    _write_json(report_json_path, report)
    _write_report_md(report_md_path, report)
    return report


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate RAG chunks from confirmed policy knowledge cards.")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT_PATH)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT_PATH)
    parser.add_argument("--report-json", type=Path, default=DEFAULT_REPORT_JSON_PATH)
    parser.add_argument("--report-md", type=Path, default=DEFAULT_REPORT_MD_PATH)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    report = generate_rag_chunks(args.input, args.output, args.report_json, args.report_md)
    print(f"Generated RAG chunks: {report['generatedChunkCount']}")
    print(f"Report written to: {args.report_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
