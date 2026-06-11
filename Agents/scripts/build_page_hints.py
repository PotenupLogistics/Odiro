from __future__ import annotations

import json
import sys
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from harness.utils.pdf_page_hint_finder import extract_pages, find_page_hints_in_pages
MANUAL_CONFIRMATION_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
PAGE_HINTS_DIR = ROOT / "data" / "sources" / "review" / "high_priority" / "page_hints"
PAGE_HINTS_JSON_PATH = PAGE_HINTS_DIR / "high_priority_page_hints.json"
PAGE_HINTS_MD_PATH = PAGE_HINTS_DIR / "high_priority_page_hints.md"
EXPECTED_SOURCE_IDS = ["KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"]
PDF_PATHS = {
    "KOR-001": ROOT / "data" / "sources" / "raw" / "korea" / "KOR-001_지능형로봇법.pdf",
    "KOR-002": ROOT / "data" / "sources" / "raw" / "korea" / "KOR-002_도로교통법_실외이동로봇.pdf",
    "KOR-003": ROOT / "data" / "sources" / "raw" / "korea" / "KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.pdf",
    "KOR-004": ROOT / "data" / "sources" / "raw" / "korea" / "KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.pdf",
    "KOR-005": ROOT / "data" / "sources" / "raw" / "korea" / "KOR-005_도로교통법_제2조_하위법령_운행기준_참고자료.pdf",
}


def escape_md_cell(value: object) -> str:
    return str(value).replace("|", "｜").replace("\n", " ")


def load_candidates() -> list[dict]:
    payload = json.loads(MANUAL_CONFIRMATION_PATH.read_text(encoding="utf-8-sig"))
    return payload["items"]


def build_items(candidates: list[dict]) -> list[dict]:
    items: list[dict] = []
    page_cache = {
        source_id: extract_pages(pdf_path)
        for source_id, pdf_path in PDF_PATHS.items()
        if pdf_path.exists()
    }
    for candidate in candidates:
        hints = find_page_hints_in_pages(
            page_cache[candidate["sourceId"]],
            candidate["extractedText"],
            candidate["category"],
        )
        items.append(
            {
                "candidateId": candidate["candidateId"],
                "sourceId": candidate["sourceId"],
                "category": candidate["category"],
                "extractedText": candidate["extractedText"],
                "hintStatus": hints["hintStatus"],
                "pageHints": hints["pageHints"],
                "note": hints["note"],
            }
        )
    return items


def write_index_markdown(items: list[dict], summary: dict) -> None:
    lines = [
        "# High Priority Page Hints",
        "",
        f"* totalCandidates: {len(items)}",
        f"* found: {summary['found']}",
        f"* partial: {summary['partial']}",
        f"* notFound: {summary['notFound']}",
        f"* needsManualPageSearch: {summary['needsManualPageSearch']}",
        "",
        "pageNumber는 확정 페이지가 아니라 원본 PDF 수동 검토를 위한 힌트다.",
        "",
        "| candidateId | sourceId | category | hintStatus | pageHints | note |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for item in items:
        page_hints = ", ".join(str(hint["pageNumber"]) for hint in item["pageHints"])
        lines.append(
            f"| {item['candidateId']} | {item['sourceId']} | {item['category']} | {item['hintStatus']} | {page_hints} | {escape_md_cell(item['note'])} |"
        )
    PAGE_HINTS_MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def write_source_markdown(items: list[dict]) -> None:
    by_source: dict[str, list[dict]] = defaultdict(list)
    for item in items:
        by_source[item["sourceId"]].append(item)

    for source_id in EXPECTED_SOURCE_IDS:
        lines = [
            f"# {source_id} Page Hints for High Priority Candidates",
            "",
            "## 1. 목적",
            "",
            "이 문서는 high priority candidate를 원본 PDF에서 찾기 위한 페이지 힌트이다.",
            "confirmed/rejected 판단이 아니며, policy card도 아니다.",
            "",
            "## 2. 후보별 page hint",
            "",
            "| candidateId | category | hintStatus | pageHints | confidence | extractedText |",
            "| ----------- | -------- | ---------- | --------- | ---------- | ------------- |",
        ]
        for item in by_source[source_id]:
            page_hints = ", ".join(str(hint["pageNumber"]) for hint in item["pageHints"])
            confidence = ", ".join(hint["confidence"] for hint in item["pageHints"])
            lines.append(
                f"| {item['candidateId']} | {item['category']} | {item['hintStatus']} | {page_hints} | {confidence} | {escape_md_cell(item['extractedText'])} |"
            )
        (PAGE_HINTS_DIR / f"{source_id}_page_hints.md").write_text(
            "\n".join(lines) + "\n",
            encoding="utf-8-sig",
        )


def main() -> int:
    PAGE_HINTS_DIR.mkdir(parents=True, exist_ok=True)
    candidates = load_candidates()
    items = build_items(candidates)
    counts = Counter(item["hintStatus"] for item in items)
    summary = {
        "found": counts["found"],
        "partial": counts["partial"],
        "notFound": counts["not_found"],
        "needsManualPageSearch": counts["needs_manual_page_search"],
    }
    payload = {
        "generatedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "totalCandidates": len(items),
        "hintSummary": summary,
        "items": items,
    }
    PAGE_HINTS_JSON_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    write_index_markdown(items, summary)
    write_source_markdown(items)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
