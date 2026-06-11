from __future__ import annotations

import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HIGH_PRIORITY_QUEUE_PATH = ROOT / "data" / "sources" / "review" / "high_priority" / "high_priority_review_queue.json"
CONFIRMED_DIR = ROOT / "data" / "sources" / "review" / "confirmed"
RESULTS_JSON_PATH = CONFIRMED_DIR / "manual_confirmation_results.json"
RESULTS_MD_PATH = CONFIRMED_DIR / "manual_confirmation_results.md"


def escape_md_cell(value: object) -> str:
    return str(value).replace("|", "｜").replace("\n", " ")


def build_items() -> list[dict]:
    queue = json.loads(HIGH_PRIORITY_QUEUE_PATH.read_text(encoding="utf-8-sig"))
    items = []
    for item in queue["items"]:
        items.append(
            {
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
        )
    return items


def write_markdown(items: list[dict]) -> None:
    counts = Counter(item["manualReviewStatus"] for item in items)
    lines = [
        "# Manual Confirmation Results",
        "",
        "## 1. 목적",
        "",
        "High priority 후보 43개의 수동 검토 결과를 기록하는 작업 문서이다.",
        "",
        "## 2. 현재 상태",
        "",
        f"* totalHighPriorityCandidates: {len(items)}",
        f"* pending: {counts['pending_manual_confirmation']}",
        f"* confirmed: {counts['confirmed']}",
        f"* rejected: {counts['rejected']}",
        "",
        "## 3. 검토 대상 목록",
        "",
        "| No | sourceId | candidateId | category | extractedText | rawPdfPage | rawPdfSection | confirmedText | manualReviewStatus | decisionReason |",
        "| -- | -------- | ----------- | -------- | ------------- | ---------- | ------------- | ------------- | ------------------ | -------------- |",
    ]
    for index, item in enumerate(items, start=1):
        lines.append(
            f"| {index} | {item['sourceId']} | {item['candidateId']} | {item['category']} | {escape_md_cell(item['extractedText'])} |  |  |  | {item['manualReviewStatus']} |  |"
        )
    RESULTS_MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def main() -> int:
    CONFIRMED_DIR.mkdir(parents=True, exist_ok=True)
    items = build_items()
    counts = Counter(item["manualReviewStatus"] for item in items)
    payload = {
        "generatedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "totalHighPriorityCandidates": len(items),
        "statusSummary": {
            "pending_manual_confirmation": counts["pending_manual_confirmation"],
            "confirmed": counts["confirmed"],
            "rejected": counts["rejected"],
        },
        "items": items,
    }
    RESULTS_JSON_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    write_markdown(items)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
