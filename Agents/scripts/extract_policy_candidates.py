from __future__ import annotations

import json
import re
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROCESSED_DIR = ROOT / "data" / "sources" / "processed" / "korea"
CANDIDATE_DIR = ROOT / "data" / "sources" / "review" / "candidates" / "korea"
INDEX_JSON_PATH = ROOT / "data" / "sources" / "review" / "candidates" / "policy_candidate_index.json"
INDEX_MD_PATH = ROOT / "data" / "sources" / "review" / "candidates" / "policy_candidate_index.md"
REVIEW_DIR = ROOT / "data" / "sources" / "review" / "korea"
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"

PROCESSED_FILES = {
    "KOR-001": "KOR-001_지능형로봇법.md",
    "KOR-002": "KOR-002_도로교통법_실외이동로봇.md",
    "KOR-003": "KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.md",
    "KOR-004": "KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.md",
    "KOR-005": "KOR-005_도로교통법_제2조_하위법령_운행기준_참고자료.md",
}
CANDIDATE_FILES = {
    source_id: f"{source_id}_policy_candidates.md" for source_id in PROCESSED_FILES
}

KEYWORD_TO_CATEGORY = {
    "실외이동로봇": "legal_background",
    "운행안전인증": "certification_process",
    "인증": "certification_process",
    "보도": "sidewalk_operation",
    "횡단보도": "crosswalk_operation",
    "보행자": "sidewalk_operation",
    "운행속도": "speed_policy",
    "속도": "speed_policy",
    "비상정지": "emergency_stop",
    "정지": "emergency_stop",
    "주변 인식": "perception_requirement",
    "장애물": "perception_requirement",
    "관제": "operator_control",
    "관제장치": "operator_control",
    "원격": "operator_control",
    "동적 특성": "terrain_or_dynamic_safety",
    "경사": "terrain_or_dynamic_safety",
    "턱": "terrain_or_dynamic_safety",
    "방수": "terrain_or_dynamic_safety",
    "주행": "terrain_or_dynamic_safety",
    "운행": "legal_background",
    "안전": "legal_background",
    "검사": "certification_process",
    "재검사": "certification_process",
    "질량": "terrain_or_dynamic_safety",
    "무게": "terrain_or_dynamic_safety",
}
MAX_CANDIDATES_PER_SOURCE = 80


def load_registry() -> dict[str, dict]:
    entries = json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))
    return {entry["sourceId"]: entry for entry in entries}


def current_section(lines: list[str], line_index: int) -> str:
    section = "unknown"
    for prior in range(line_index, -1, -1):
        if lines[prior].startswith("## "):
            return lines[prior].strip("# ").strip()
        if lines[prior].startswith("# "):
            section = lines[prior].strip("# ").strip()
            break
    return section


def extracted_text_section(text: str) -> tuple[int, str]:
    lines = text.splitlines()
    start = 0
    for index, line in enumerate(lines, start=1):
        if line.strip() == "## 7. 추출 원문 텍스트":
            start = index
            break

    if start == 0:
        return 1, text

    section_lines = lines[start:]
    if section_lines and section_lines[0].strip() == "":
        section_lines = section_lines[1:]
        start += 1
    if section_lines and section_lines[0].strip() == "```text":
        section_lines = section_lines[1:]
        start += 1
    if section_lines and section_lines[-1].strip() == "```":
        section_lines = section_lines[:-1]

    return start, "\n".join(section_lines)


def split_candidate_units(text: str, line_offset: int) -> list[tuple[int, str]]:
    units: list[tuple[int, str]] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        cleaned = line.strip()
        if not cleaned:
            continue
        if cleaned.startswith(("#", "| ---", "```")):
            continue
        if len(cleaned) < 12:
            continue
        units.append((line_number + line_offset - 1, cleaned))
    return units


def find_keyword(text: str) -> tuple[str, str] | None:
    compact = text.replace(" ", "")
    for keyword, category in KEYWORD_TO_CATEGORY.items():
        if keyword in text or keyword.replace(" ", "") in compact:
            return keyword, category
    return None


def trim_text(text: str, limit: int = 300) -> str:
    if len(text) <= limit:
        return text
    return text[:limit].rstrip() + "..."


def escape_md_cell(value: str) -> str:
    return str(value).replace("|", "｜").replace("\n", " ")


def extract_candidates(source_id: str, processed_path: Path) -> list[dict]:
    text = processed_path.read_text(encoding="utf-8-sig")
    lines = text.splitlines()
    raw_start_line, raw_text = extracted_text_section(text)
    candidates: list[dict] = []
    seen: set[str] = set()

    for line_number, unit in split_candidate_units(raw_text, raw_start_line):
        match = find_keyword(unit)
        if not match:
            continue

        keyword, category = match
        normalized = re.sub(r"\s+", " ", unit)
        dedupe_key = f"{category}:{keyword}:{normalized[:160]}"
        if dedupe_key in seen:
            continue
        seen.add(dedupe_key)

        candidate_number = len(candidates) + 1
        candidates.append(
            {
                "candidateId": f"CAND-{source_id}-{candidate_number:03d}",
                "category": category,
                "keyword": keyword,
                "extractedText": trim_text(normalized),
                "processedSection": current_section(lines, line_number - 1),
                "sourceLocationHint": f"processed markdown line {line_number}",
                "reviewStatus": "needs_pdf_check",
            }
        )

        if len(candidates) >= MAX_CANDIDATES_PER_SOURCE:
            break

    return candidates


def build_candidate_markdown(
    source: dict,
    source_id: str,
    processed_path: Path,
    candidates: list[dict],
) -> str:
    categories_found = sorted({candidate["category"] for candidate in candidates})
    lines = [
        f"# {source_id} 정책 후보 검토 자료",
        "",
        "## 1. Source Metadata",
        "",
        f"* sourceId: {source_id}",
        f"* title: {source['title']}",
        f"* processedFilePath: {processed_path.relative_to(ROOT).as_posix()}",
        "* extractionStatus: partial",
        "* candidateStatus: needs_pdf_check",
        "* note: 이 문서는 policy card가 아니며, 원본 PDF 수동 대조 전 후보 자료이다.",
        "",
        "## 2. 후보 추출 요약",
        "",
        f"* 총 후보 수: {len(candidates)}",
        f"* 발견된 카테고리: {', '.join(categories_found) if categories_found else '없음'}",
        "* 수동 검토 필요 여부: yes",
        "",
        "## 3. 정책 후보 목록",
        "",
        "| candidateId | category | keyword | extractedText | processedSection | sourceLocationHint | reviewStatus |",
        "| ----------- | -------- | ------- | ------------- | ---------------- | ------------------ | ------------ |",
    ]

    if candidates:
        for candidate in candidates:
            lines.append(
                "| {candidateId} | {category} | {keyword} | {extractedText} | {processedSection} | {sourceLocationHint} | {reviewStatus} |".format(
                    candidateId=escape_md_cell(candidate["candidateId"]),
                    category=escape_md_cell(candidate["category"]),
                    keyword=escape_md_cell(candidate["keyword"]),
                    extractedText=escape_md_cell(candidate["extractedText"]),
                    processedSection=escape_md_cell(candidate["processedSection"]),
                    sourceLocationHint=escape_md_cell(candidate["sourceLocationHint"]),
                    reviewStatus=escape_md_cell(candidate["reviewStatus"]),
                )
            )

    lines.extend(
        [
            "",
            "## 4. 수동 검토 메모",
            "",
        ]
    )
    return "\n".join(lines)


def build_index_markdown(entries: list[dict]) -> str:
    total = sum(entry["candidateCount"] for entry in entries)
    lines = [
        "# Policy Candidate Index",
        "",
        f"- 총 후보 수: {total}",
        "- 모든 후보 reviewStatus: needs_pdf_check",
        "",
        "| sourceId | candidateFilePath | candidateCount | categoriesFound | needsPdfCheckCount | extractionStatus | reviewStatus |",
        "| --- | --- | ---: | --- | ---: | --- | --- |",
    ]
    for entry in entries:
        categories = ", ".join(entry["categoriesFound"]) if entry["categoriesFound"] else "없음"
        lines.append(
            f"| {entry['sourceId']} | `{entry['candidateFilePath']}` | {entry['candidateCount']} | {categories} | {entry['needsPdfCheckCount']} | {entry['extractionStatus']} | {entry['reviewStatus']} |"
        )
    return "\n".join(lines) + "\n"


def ensure_checklist_candidate_section(source_id: str, candidate_path: Path) -> None:
    checklist_path = REVIEW_DIR / f"{source_id}_review_checklist.md"
    content = checklist_path.read_text(encoding="utf-8-sig")
    marker = "## 6. 자동 추출된 정책 후보 파일"
    section = "\n".join(
        [
            "",
            marker,
            "",
            f"* candidateFilePath: {candidate_path.relative_to(ROOT).as_posix()}",
            "* candidateStatus: needs_pdf_check",
            "* 주의: 이 후보는 원본 PDF와 대조 전이며, policy knowledge card로 확정된 것이 아니다.",
            "",
        ]
    )

    if marker in content:
        content = content.split(marker, 1)[0].rstrip() + section
    else:
        content = content.rstrip() + section

    checklist_path.write_text(content, encoding="utf-8-sig")


def main() -> int:
    registry = load_registry()
    CANDIDATE_DIR.mkdir(parents=True, exist_ok=True)
    INDEX_JSON_PATH.parent.mkdir(parents=True, exist_ok=True)
    generated_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    index_entries: list[dict] = []

    for source_id, processed_filename in PROCESSED_FILES.items():
        processed_path = PROCESSED_DIR / processed_filename
        candidate_path = CANDIDATE_DIR / CANDIDATE_FILES[source_id]
        candidates = extract_candidates(source_id, processed_path)
        categories_found = sorted({candidate["category"] for candidate in candidates})

        candidate_path.write_text(
            build_candidate_markdown(registry[source_id], source_id, processed_path, candidates),
            encoding="utf-8-sig",
        )
        ensure_checklist_candidate_section(source_id, candidate_path)

        index_entries.append(
            {
                "sourceId": source_id,
                "candidateFilePath": candidate_path.relative_to(ROOT).as_posix(),
                "candidateCount": len(candidates),
                "categoriesFound": categories_found,
                "needsPdfCheckCount": len(candidates),
                "extractionStatus": "partial",
                "reviewStatus": "needs_pdf_check",
            }
        )

    index = {
        "generatedAt": generated_at,
        "note": "This is not a policy knowledge card file. All candidates require manual PDF verification.",
        "totalCandidateCount": sum(entry["candidateCount"] for entry in index_entries),
        "sources": index_entries,
    }
    INDEX_JSON_PATH.write_text(
        json.dumps(index, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    INDEX_MD_PATH.write_text(build_index_markdown(index_entries), encoding="utf-8-sig")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
