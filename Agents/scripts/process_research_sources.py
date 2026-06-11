from __future__ import annotations

import json
import re
from datetime import datetime, timezone
from pathlib import Path

from pypdf import PdfReader


ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
RAW_RSR_001_PATH = ROOT / "data" / "sources" / "raw" / "research" / "RSR-001_METRANS_Sidewalk_ADR_Interactions.pdf"
PROCESSED_DIR = ROOT / "data" / "sources" / "processed" / "research"
PROCESSED_RSR_001_PATH = PROCESSED_DIR / "RSR-001_METRANS_Sidewalk_ADR_Interactions.md"
REVIEW_DIR = ROOT / "data" / "sources" / "review" / "research"
REVIEW_STATUS_PATH = ROOT / "data" / "sources" / "review" / "research_review_status.json"
REPORT_JSON_PATH = ROOT / "data" / "sources" / "processed" / "research_processing_report.json"
REPORT_MD_PATH = ROOT / "data" / "sources" / "processed" / "research_processing_report.md"
EXPECTED_RSR_IDS = ["RSR-001", "RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"]

KEYWORDS = [
    "sidewalk autonomous delivery robot",
    "pedestrian",
    "bicyclist",
    "interaction",
    "PET",
    "conflict",
    "near-miss",
    "near miss",
    "safety",
    "observation",
    "delivery robot",
]
CONNECTABLE_ELEMENTS = {
    "pedestrian_interaction": ["pedestrian", "interaction"],
    "near_miss_metric": ["near-miss", "near miss"],
    "pet_metric": ["PET"],
    "delivery_robot_scenario": ["delivery robot", "sidewalk autonomous delivery robot"],
    "comfort_and_safety": ["comfort", "safety"],
    "scenario_design": ["scenario", "observation"],
    "evaluation_metric": ["metric", "PET", "near miss"],
}


def load_registry() -> dict[str, dict]:
    entries = json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))
    return {entry["sourceId"]: entry for entry in entries}


def normalize_text(text: str) -> str:
    text = text.replace("\x00", "")
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def extract_pdf_text(path: Path) -> tuple[str, int, list[str]]:
    reader = PdfReader(str(path))
    page_texts: list[str] = []
    errors: list[str] = []

    for index, page in enumerate(reader.pages, start=1):
        try:
            page_texts.append(page.extract_text() or "")
        except Exception as exc:
            errors.append(f"page {index}: {type(exc).__name__}: {exc}")
            page_texts.append("")

    return normalize_text("\n\n".join(page_texts)), len(reader.pages), errors


def extraction_status(text: str, errors: list[str]) -> tuple[str, str]:
    if errors and not text:
        return "failed", "; ".join(errors)
    if len(text) < 500:
        return "needs_manual_review", "Extracted text is too short; manual source review is required."
    return "partial", "Text was extracted, but tables, figures, page references, and layout require manual PDF review."


def find_keywords(text: str) -> list[str]:
    lower_text = text.lower()
    return [
        keyword
        for keyword in KEYWORDS
        if keyword.lower() in lower_text
    ]


def snippet_for_terms(text: str, terms: list[str]) -> str:
    compact = text.replace("\n", " ")
    lower_compact = compact.lower()
    for term in terms:
        idx = lower_compact.find(term.lower())
        if idx != -1:
            start = max(0, idx - 80)
            end = min(len(compact), idx + len(term) + 160)
            return compact[start:end].strip()
    return "확인 필요"


def escape_md_cell(value: str) -> str:
    return str(value).replace("|", "｜").replace("\n", " ")


def build_processed_markdown(
    source: dict,
    processed_at: str,
    text: str,
    page_count: int,
    status: str,
    note: str,
    found_keywords: list[str],
) -> str:
    rows = []
    for element, terms in CONNECTABLE_ELEMENTS.items():
        snippet = snippet_for_terms(text, terms)
        if snippet != "확인 필요":
            rows.append((element, snippet, element, "검토 필요"))
    if not rows:
        rows.append(("확인 필요", "확인 필요", "확인 필요", "검토 필요"))

    candidate_lines = [
        f"- sourceId: RSR-001 / 후보: {row[2]} / 상태: 검토 필요"
        for row in rows
    ]

    overview = "확인 필요"
    if found_keywords:
        overview = (
            "PDF 추출 텍스트에서 "
            + ", ".join(found_keywords[:6])
            + " 키워드가 확인됩니다. 세부 의미와 페이지 근거는 원본 PDF 대조가 필요합니다."
        )

    lines = [
        "# RSR-001 METRANS Sidewalk ADR Interactions",
        "",
        "## 1. Source Metadata",
        "",
        "* sourceId: RSR-001",
        f"* title: {source['title']}",
        f"* originalFilePath: {source['filePath']}",
        f"* processedAt: {processed_at}",
        f"* extractionStatus: {status}",
        "* extractionMethod: pypdf PdfReader.extract_text",
        f"* notes: {note}",
        "",
        "## 2. 문서 개요",
        "",
        overview,
        "",
        "## 3. 프로젝트 관련 키워드",
        "",
    ]
    lines.extend([f"* {keyword}" for keyword in found_keywords] or ["* 확인 필요"])
    lines.extend(
        [
            "",
            "## 4. 정책/실험 설계와 연결될 수 있는 내용",
            "",
            "| 항목 | 문서에서 확인된 내용 요약 | 연결 가능 정책/실험 요소 | 확인 상태 |",
            "| -- | -------------- | -------------- | ----- |",
        ]
    )
    for item, summary, element, review_status in rows:
        lines.append(
            f"| {escape_md_cell(item)} | {escape_md_cell(summary)} | {escape_md_cell(element)} | {escape_md_cell(review_status)} |"
        )

    lines.extend(
        [
            "",
            "## 5. 추후 정책/실험 카드 후보",
            "",
            *candidate_lines,
            "",
            "## 6. 수동 검토 필요 사항",
            "",
            f"* PDF page count from extractor: {page_count}",
            "* 표, 이미지, 수식, 알고리즘 구조는 원본 PDF 대조 필요",
            "* 페이지 번호와 인용 가능한 위치 확인 필요",
            "",
            "## 7. 추출 원문 텍스트",
            "",
            "```text",
            text or "확인 필요",
            "```",
            "",
        ]
    )
    return "\n".join(lines)


def checklist_markdown(source: dict, processed_path: str, mode: str) -> str:
    source_id = source["sourceId"]
    raw_path = source["filePath"] if mode == "local_pdf" else ""
    notes = "" if mode == "local_pdf" else "URL-only; raw content not collected yet"
    lines = [
        f"# {source_id} 수동 검토 체크리스트",
        "",
        "## 1. Source Metadata",
        "",
        f"* sourceId: {source_id}",
        f"* title: {source['title']}",
        f"* url: {source['url']}",
        f"* rawFilePath: {raw_path}",
        f"* processedFilePath: {processed_path}",
        "* sourceStatus: to_review",
        "* reviewStatus: not_started",
        f"* notes: {notes}",
        "",
        "## 2. 원본 확보 상태",
        "",
        "* [ ] 원본 PDF 또는 공식 HTML 내용을 확보했다",
        "* [ ] 원본 링크가 접근 가능한지 확인했다",
        "* [ ] 원본 내용을 processed Markdown으로 변환했다",
        "* [ ] 표/이미지/수식/알고리즘 구조가 필요한지 확인했다",
        "* [ ] 프로젝트 적용 범위를 확인했다",
        "",
        "## 3. 프로젝트 활용 검토 항목",
        "",
        "| 카테고리 | 원본 위치/페이지/URL 위치 | 확인한 내용 | 프로젝트 적용 | 검토 상태 |",
        "| --- | --- | --- | --- | --- |",
        "| scenario_design |  |  |  | not_checked |",
        "| evaluation_metric |  |  |  | not_checked |",
        "| experiment_automation |  |  |  | not_checked |",
        "| world_config_generation |  |  |  | not_checked |",
        "| parameter_sampling |  |  |  | not_checked |",
        "| llm_automation |  |  |  | not_checked |",
        "| policy_evaluation |  |  |  | not_checked |",
        "",
        "## 4. 카드 생성 전 확인사항",
        "",
        "* [ ] sourceId가 registry와 일치한다",
        "* [ ] 원본과 processed 내용을 대조했다",
        "* [ ] 프로젝트에 맞는 축소 적용 범위를 적었다",
        "* [ ] 원문을 그대로 복사하지 않고 요약 카드로 변환할 준비가 됐다",
        "* [ ] policy card 또는 experiment knowledge card 생성 가능 여부를 판단했다",
        "",
        "## 5. 수동 검토 메모",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    registry = load_registry()
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    REVIEW_DIR.mkdir(parents=True, exist_ok=True)
    processed_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()

    text, page_count, errors = extract_pdf_text(RAW_RSR_001_PATH)
    status, note = extraction_status(text, errors)
    found_keywords = find_keywords(text)
    PROCESSED_RSR_001_PATH.write_text(
        build_processed_markdown(
            registry["RSR-001"],
            processed_at,
            text,
            page_count,
            status,
            note,
            found_keywords,
        ),
        encoding="utf-8-sig",
    )

    review_entries: list[dict] = []
    report_entries: list[dict] = []
    for source_id in EXPECTED_RSR_IDS:
        source = registry[source_id]
        mode = "local_pdf" if source_id == "RSR-001" else "url_only"
        processed_path = (
            PROCESSED_RSR_001_PATH.relative_to(ROOT).as_posix()
            if source_id == "RSR-001"
            else ""
        )
        raw_path = source["filePath"] if source_id == "RSR-001" else ""
        checklist_path = REVIEW_DIR / f"{source_id}_review_checklist.md"
        checklist_path.write_text(
            checklist_markdown(source, processed_path, mode),
            encoding="utf-8-sig",
        )
        notes = (
            "RSR-001 local PDF processed; manual source comparison required"
            if source_id == "RSR-001"
            else "URL-only source; manual content collection required before processing"
        )
        extraction = status if source_id == "RSR-001" else "not_processed"
        review_entries.append(
            {
                "sourceId": source_id,
                "reviewChecklistPath": checklist_path.relative_to(ROOT).as_posix(),
                "rawFilePath": raw_path,
                "processedFilePath": processed_path,
                "reviewStatus": "not_started",
                "sourceMode": mode,
                "notes": notes,
            }
        )
        report_entries.append(
            {
                "sourceId": source_id,
                "sourceMode": mode,
                "processedFilePath": processed_path,
                "extractionStatus": extraction,
                "manualReviewRequired": True,
                "notes": notes,
            }
        )

    REVIEW_STATUS_PATH.write_text(
        json.dumps(review_entries, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )

    report = {
        "processedAt": processed_at,
        "totalRsrSourceCount": len(report_entries),
        "localPdfSourceCount": 1,
        "urlOnlySourceCount": 5,
        "processedCompletedSources": ["RSR-001"],
        "processedPendingSources": ["RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"],
        "sources": report_entries,
    }
    REPORT_JSON_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    lines = [
        "# Research Processing Report",
        "",
        f"* RSR source 총 개수: {report['totalRsrSourceCount']}",
        f"* local_pdf source 수: {report['localPdfSourceCount']}",
        f"* url_only source 수: {report['urlOnlySourceCount']}",
        f"* processed 완료 source: {', '.join(report['processedCompletedSources'])}",
        f"* processed 대기 source: {', '.join(report['processedPendingSources'])}",
        "",
        "| sourceId | sourceMode | extractionStatus | manualReviewRequired | processedFilePath |",
        "| --- | --- | --- | --- | --- |",
    ]
    for entry in report_entries:
        lines.append(
            f"| {entry['sourceId']} | {entry['sourceMode']} | {entry['extractionStatus']} | {entry['manualReviewRequired']} | {entry['processedFilePath']} |"
        )
    REPORT_MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
