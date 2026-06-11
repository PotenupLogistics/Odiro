from __future__ import annotations

import json
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRIAGE_JSON_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.json"
HIGH_PRIORITY_DIR = ROOT / "data" / "sources" / "review" / "high_priority"
QUEUE_JSON_PATH = HIGH_PRIORITY_DIR / "high_priority_review_queue.json"
QUEUE_MD_PATH = HIGH_PRIORITY_DIR / "high_priority_review_queue.md"
MANUAL_REVIEW_QUEUE_PATH = ROOT / "docs" / "manual_review" / "MANUAL_REVIEW_QUEUE.md"
EXPECTED_SOURCE_IDS = ["KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"]
EXPECTED_HIGH_COUNT = 43
SOURCE_TITLES = {
    "KOR-001": "지능형 로봇 개발 및 보급 촉진법",
    "KOR-002": "도로교통법 실외이동로봇 관련 법률",
    "KOR-003": "KIRIA 실외이동로봇 운행안전인증 가이드북",
    "KOR-004": "실외이동로봇 운행안전인증 절차 및 기준 등에 관한 고시",
    "KOR-005": "도로교통법 제2조 하위법령 운행기준 참고자료",
}
RAW_PATHS = {
    "KOR-001": "data/sources/raw/korea/KOR-001_지능형로봇법.pdf",
    "KOR-002": "data/sources/raw/korea/KOR-002_도로교통법_실외이동로봇.pdf",
    "KOR-003": "data/sources/raw/korea/KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.pdf",
    "KOR-004": "data/sources/raw/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.pdf",
    "KOR-005": "data/sources/raw/korea/KOR-005_도로교통법_제2조_하위법령_운행기준_참고자료.pdf",
}


def load_high_items() -> list[dict]:
    triage = json.loads(TRIAGE_JSON_PATH.read_text(encoding="utf-8-sig"))
    return [item for item in triage["reviewQueue"] if item["priority"] == "high"]


def queue_item(item: dict) -> dict:
    return {
        "candidateId": item["candidateId"],
        "sourceId": item["sourceId"],
        "category": item["category"],
        "priority": item["priority"],
        "extractedText": item["extractedText"],
        "linkedMvpSituation": item["linkedMvpSituation"],
        "linkedMvpAction": item["linkedMvpAction"],
        "relatedPolicyParams": item["relatedPolicyParams"],
        "reasonForPriority": item["reasonForPriority"],
        "manualReviewStatus": "pending_manual_confirmation",
        "rawPdfPage": "",
        "rawPdfSection": "",
        "confirmedText": "",
        "reviewer": "",
        "reviewedAt": "",
        "decisionReason": "",
        "nextAction": "needs_pdf_check",
    }


def escape_md_cell(value: object) -> str:
    if isinstance(value, list):
        value = ", ".join(str(item) for item in value)
    return str(value).replace("|", "｜").replace("\n", " ")


def build_queue_json(items: list[dict]) -> dict:
    status_counts = Counter(item["manualReviewStatus"] for item in items)
    return {
        "generatedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "totalHighPriorityCandidates": len(items),
        "reviewStatusSummary": dict(sorted(status_counts.items())),
        "items": items,
    }


def write_queue_markdown(items: list[dict]) -> None:
    lines = [
        "# High Priority Manual Review Queue",
        "",
        "## 1. 목적",
        "",
        "MVP 정책과 직접 연결되는 High priority 후보 43개를 원본 PDF와 수동 대조하기 위한 검토 문서이다.",
        "",
        "## 2. 검토 원칙",
        "",
        "* 이 문서는 policy card가 아니다.",
        "* 모든 항목은 원본 PDF 대조 전이다.",
        "* 원본 PDF에서 실제 문장을 확인한 뒤에만 confirmed 처리할 수 있다.",
        "* confirmed 처리는 다음 단계에서 별도 작업으로 수행한다.",
        "* 현재 상태는 모두 pending_manual_confirmation이다.",
        "",
        "## 3. Source별 검토 순서",
        "",
        "검토 권장 순서:",
        "",
        "1. KOR-003 KIRIA 운행안전인증 가이드북",
        "2. KOR-004 운행안전인증 절차 및 기준 고시",
        "3. KOR-002 도로교통법",
        "4. KOR-001 지능형로봇법",
        "5. KOR-005 도로교통법 제2조 하위법령",
        "",
        "## 4. High Priority 후보 목록",
        "",
        "| No | sourceId | candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | manualReviewStatus |",
        "| -- | -------- | ----------- | -------- | ------------- | ------------------ | --------------- | ------------------- | ------------------ |",
    ]
    for index, item in enumerate(items, start=1):
        lines.append(
            f"| {index} | {item['sourceId']} | {item['candidateId']} | {item['category']} | {escape_md_cell(item['extractedText'])} | {escape_md_cell(item['linkedMvpSituation'])} | {escape_md_cell(item['linkedMvpAction'])} | {escape_md_cell(item['relatedPolicyParams'])} | {item['manualReviewStatus']} |"
        )
    QUEUE_MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def write_source_review_files(items: list[dict]) -> dict[str, int]:
    by_source: dict[str, list[dict]] = defaultdict(list)
    for item in items:
        by_source[item["sourceId"]].append(item)

    counts: dict[str, int] = {}
    for source_id in EXPECTED_SOURCE_IDS:
        source_items = by_source[source_id]
        counts[source_id] = len(source_items)
        lines = [
            f"# {source_id} High Priority Review",
            "",
            "## 1. Source Metadata",
            "",
            f"* sourceId: {source_id}",
            f"* sourceTitle: {SOURCE_TITLES[source_id]}",
            f"* rawFilePath: {RAW_PATHS[source_id]}",
            f"* candidateCount: {len(source_items)}",
            "* reviewStatus: pending_manual_confirmation",
            "",
            "## 2. 검토 대상 후보",
            "",
            "| candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | rawPdfPage | rawPdfSection | confirmedText | manualReviewStatus |",
            "| ----------- | -------- | ------------- | ------------------ | --------------- | ------------------- | ---------- | ------------- | ------------- | ------------------ |",
        ]
        for item in source_items:
            lines.append(
                f"| {item['candidateId']} | {item['category']} | {escape_md_cell(item['extractedText'])} | {escape_md_cell(item['linkedMvpSituation'])} | {escape_md_cell(item['linkedMvpAction'])} | {escape_md_cell(item['relatedPolicyParams'])} |  |  |  | {item['manualReviewStatus']} |"
            )
        lines.extend(["", "## 3. 수동 검토 메모", ""])
        (HIGH_PRIORITY_DIR / f"{source_id}_high_priority_review.md").write_text(
            "\n".join(lines),
            encoding="utf-8-sig",
        )
    return counts


def update_manual_review_queue_doc(counts: dict[str, int]) -> None:
    content = MANUAL_REVIEW_QUEUE_PATH.read_text(encoding="utf-8-sig")
    marker = "## 6. High Priority Review Workspace"
    section = [
        "",
        marker,
        "",
        "* highPriorityReviewQueueJson: data/sources/review/high_priority/high_priority_review_queue.json",
        "* highPriorityReviewQueueMarkdown: data/sources/review/high_priority/high_priority_review_queue.md",
        "",
        "### Source별 high priority review 파일",
        "",
    ]
    for source_id in EXPECTED_SOURCE_IDS:
        section.append(
            f"* {source_id}: data/sources/review/high_priority/{source_id}_high_priority_review.md ({counts[source_id]} candidates)"
        )
    section.extend(
        [
            "",
            "### 사람이 작성해야 하는 필드",
            "",
            "* rawPdfPage",
            "* rawPdfSection",
            "* confirmedText",
            "* manualReviewStatus",
            "* decisionReason",
            "",
            "### 다음 단계",
            "",
            "1. 사람이 원본 PDF와 대조",
            "2. pending_manual_confirmation → confirmed 또는 rejected로 변경",
            "3. confirmed 후보만 policy knowledge card 생성 대상으로 사용",
            "",
        ]
    )
    if marker in content:
        content = content.split(marker, 1)[0].rstrip() + "\n".join(section)
    else:
        content = content.rstrip() + "\n".join(section)
    MANUAL_REVIEW_QUEUE_PATH.write_text(content, encoding="utf-8-sig")


def main() -> int:
    HIGH_PRIORITY_DIR.mkdir(parents=True, exist_ok=True)
    raw_high_items = load_high_items()
    items = [queue_item(item) for item in raw_high_items]
    queue = build_queue_json(items)
    if len(items) != EXPECTED_HIGH_COUNT:
        queue["warnings"] = [f"Expected {EXPECTED_HIGH_COUNT} high priority candidates, got {len(items)}."]
    QUEUE_JSON_PATH.write_text(
        json.dumps(queue, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    write_queue_markdown(items)
    counts = write_source_review_files(items)
    update_manual_review_queue_doc(counts)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
