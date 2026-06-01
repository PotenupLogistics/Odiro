from __future__ import annotations

import json
import re
from datetime import datetime, timezone
from pathlib import Path

from pypdf import PdfReader


ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
PROCESSED_DIR = ROOT / "data" / "sources" / "processed" / "korea"
REPORT_JSON = ROOT / "data" / "sources" / "processed" / "source_processing_report.json"
REPORT_MD = ROOT / "data" / "sources" / "processed" / "source_processing_report.md"
EXPECTED_SOURCE_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}

KEYWORDS = [
    "실외이동로봇",
    "운행안전인증",
    "운행속도",
    "비상정지",
    "주변 인식",
    "주변인식",
    "관제장치",
    "횡단보도",
    "보도 통행",
    "보도",
    "도로교통법",
    "지능형 로봇",
    "지능형로봇",
]
POLICY_MAP = {
    "speed_policy": ["운행속도", "속도"],
    "emergency_stop": ["비상정지", "정지"],
    "perception_requirement": ["주변 인식", "주변인식", "인지", "감지"],
    "operator_control": ["관제장치", "관제", "원격"],
    "sidewalk_operation": ["보도 통행", "보도"],
    "crosswalk_operation": ["횡단보도"],
    "terrain_or_dynamic_safety": ["경사", "장애물", "노면", "동적"],
    "data_recording": ["기록", "저장", "영상"],
}


def normalize_text(text: str) -> str:
    text = text.replace("\x00", "")
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def extract_pdf(path: Path) -> tuple[str, int, list[str]]:
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


def contains_term(text: str, term: str) -> bool:
    return term in text or term.replace(" ", "") in text.replace(" ", "")


def snippets_for_terms(text: str, terms: list[str], limit: int = 1) -> list[str]:
    snippets: list[str] = []
    compact = text.replace("\n", " ")

    for term in terms:
        idx = compact.find(term)
        if idx == -1 and " " in term:
            idx = compact.replace(" ", "").find(term.replace(" ", ""))
            search_text = compact.replace(" ", "")
        else:
            search_text = compact

        if idx != -1:
            start = max(0, idx - 80)
            end = min(len(search_text), idx + len(term) + 120)
            snippets.append(search_text[start:end].strip())

        if len(snippets) >= limit:
            break

    return snippets


def extraction_status(text: str, errors: list[str]) -> tuple[str, str]:
    if errors and not text:
        return "failed", "; ".join(errors)
    if len(text) < 500:
        return "needs_manual_review", "추출 텍스트 길이가 짧아 PDF 원문 수동 검토가 필요합니다."
    return "partial", "텍스트는 추출되었으나 표, 이미지, 조항 구조는 수동 대조가 필요합니다."


def build_overview(text: str, found_keywords: list[str]) -> str:
    if not text:
        return "텍스트 추출 결과로 문서 내용을 확인할 수 없습니다. 확인 필요."
    if found_keywords:
        keywords = ", ".join(found_keywords[:5])
        return f"추출 텍스트에서 '{keywords}' 키워드가 확인됩니다. 세부 조항과 표 내용은 원문 대조가 필요합니다."
    return "텍스트는 추출되었지만 프로젝트 관련 키워드 확인은 제한적입니다. 확인 필요."


def build_policy_rows(text: str) -> list[tuple[str, str, str, str]]:
    rows: list[tuple[str, str, str, str]] = []
    for policy, terms in POLICY_MAP.items():
        snippets = snippets_for_terms(text, terms)
        if snippets:
            rows.append((policy, snippets[0], policy, "검토 필요"))

    if not rows:
        rows.append(
            (
                "확인 필요",
                "추출 텍스트에서 정책 연결 내용을 안정적으로 확인하지 못했습니다.",
                "확인 필요",
                "검토 필요",
            )
        )

    return rows


def escape_md_cell(value: str) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def build_markdown(
    source: dict,
    processed_at: str,
    status: str,
    method: str,
    note: str,
    text: str,
    found_keywords: list[str],
) -> str:
    rows = build_policy_rows(text)
    candidate_lines = [
        f"- sourceId: {source['sourceId']} / 후보: {row[2]} / 상태: 검토 필요"
        for row in rows
    ]
    manual_review = [note]

    if status in {"partial", "failed", "needs_manual_review"}:
        manual_review.append("표, 이미지, 페이지별 조항 구조는 원본 PDF와 대조 필요")
    if not text:
        manual_review.append("텍스트 추출 실패로 원문 확인 필요")

    lines = [
        f"# {source['title']}",
        "",
        "## 1. Source Metadata",
        "",
        f"sourceId: {source['sourceId']}",
        f"title: {source['title']}",
        f"originalFilePath: {source['filePath']}",
        f"processedAt: {processed_at}",
        f"extractionStatus: {status}",
        f"extractionMethod: {method}",
        f"notes: {note}",
        "",
        "## 2. 문서 개요",
        "",
        build_overview(text, found_keywords),
        "",
        "## 3. 프로젝트 관련 키워드",
        "",
    ]
    lines.extend([f"- {keyword}" for keyword in found_keywords] or ["- 확인 필요"])
    lines.extend(
        [
            "",
            "## 4. 정책 설계와 연결될 수 있는 내용",
            "",
            "| 항목 | 문서에서 확인된 내용 요약 | 연결 가능 정책 | 확인 상태 |",
            "| --- | --- | --- | --- |",
        ]
    )

    for item, summary, policy, review_status in rows:
        lines.append(
            f"| {escape_md_cell(item)} | {escape_md_cell(summary)} | {escape_md_cell(policy)} | {escape_md_cell(review_status)} |"
        )

    lines.extend(
        [
            "",
            "## 5. 추후 정책 카드 후보",
            "",
            *candidate_lines,
            "",
            "## 6. 수동 검토 필요 사항",
            "",
        ]
    )
    lines.extend([f"- {item}" for item in manual_review])
    lines.extend(
        [
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


def main() -> int:
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    processed_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    report_entries: list[dict] = []

    for source in registry:
        source_id = source["sourceId"]
        if source_id not in EXPECTED_SOURCE_IDS:
            continue

        source_path = ROOT / source["filePath"]
        text, page_count, errors = extract_pdf(source_path)
        found_keywords = [keyword for keyword in KEYWORDS if contains_term(text, keyword)]
        status, note = extraction_status(text, errors)
        method = "pypdf PdfReader.extract_text"
        output_path = PROCESSED_DIR / f"{source_path.stem}.md"

        output_path.write_text(
            build_markdown(source, processed_at, status, method, note, text, found_keywords),
            encoding="utf-8-sig",
        )

        report_entries.append(
            {
                "sourceId": source_id,
                "title": source["title"],
                "originalFilePath": source["filePath"],
                "processedFilePath": output_path.relative_to(ROOT).as_posix(),
                "extractionStatus": status,
                "extractionMethod": method,
                "processedAt": processed_at,
                "pageCount": page_count,
                "extractedTextLength": len(text),
                "needsManualReview": status in {"partial", "failed", "needs_manual_review"},
                "keywordsFound": found_keywords,
                "failureOrPartialReason": note,
            }
        )

    report = {
        "processedAt": processed_at,
        "totalSourceCount": len(report_entries),
        "sources": report_entries,
    }
    REPORT_JSON.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )

    lines = [
        "# Source Processing Report",
        "",
        f"- 총 처리 대상 문서 수: {len(report_entries)}",
        "",
        "| sourceId | processed file path | extractionStatus | extracted text length | needs_manual_review | keywords found | failure/partial reason |",
        "| --- | --- | --- | ---: | --- | --- | --- |",
    ]

    for entry in report_entries:
        keywords = ", ".join(entry["keywordsFound"]) if entry["keywordsFound"] else "확인 필요"
        lines.append(
            f"| {entry['sourceId']} | `{entry['processedFilePath']}` | {entry['extractionStatus']} | {entry['extractedTextLength']} | {entry['needsManualReview']} | {keywords} | {escape_md_cell(entry['failureOrPartialReason'])} |"
        )

    REPORT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
