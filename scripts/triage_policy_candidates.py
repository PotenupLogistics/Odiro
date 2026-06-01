from __future__ import annotations

import json
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_DIR = ROOT / "data" / "sources" / "review" / "candidates" / "korea"
TRIAGE_DIR = ROOT / "data" / "sources" / "review" / "triage"
TRIAGE_JSON_PATH = TRIAGE_DIR / "policy_candidate_triage.json"
TRIAGE_MD_PATH = TRIAGE_DIR / "policy_candidate_triage.md"
MANUAL_REVIEW_QUEUE_PATH = ROOT / "docs" / "MANUAL_REVIEW_QUEUE.md"
REVIEW_DIR = ROOT / "data" / "sources" / "review" / "korea"

MVP_CATEGORIES = {
    "speed_policy",
    "emergency_stop",
    "perception_requirement",
    "operator_control",
    "sidewalk_operation",
    "crosswalk_operation",
    "terrain_or_dynamic_safety",
}
MEDIUM_CATEGORIES = {"legal_background", "certification_process", "data_recording"}
HIGH_KEYWORDS = {
    "속도",
    "운행속도",
    "비상정지",
    "정지",
    "보행자",
    "보도",
    "횡단보도",
    "주변 인식",
    "관제",
    "장애물",
    "경사",
    "턱",
    "동적 특성",
}
CATEGORY_LINKS = {
    "speed_policy": {
        "situation": "PedestrianAhead",
        "action": "SlowDown, Stop",
        "params": ["maxSpeedKmh", "lowSpeedZoneSpeedKmh"],
    },
    "emergency_stop": {
        "situation": "ObstacleAhead",
        "action": "EmergencyStop, Stop",
        "params": ["emergencyStopEnabled", "minStopDistanceM"],
    },
    "perception_requirement": {
        "situation": "PedestrianAhead, ObstacleAhead, ApproachingObject",
        "action": "SlowDown, Stop, RequestOperator",
        "params": ["perceptionMinRangeM", "pedestrianDetectionRequired"],
    },
    "operator_control": {
        "situation": "ApproachingObject",
        "action": "RequestOperator, Stop",
        "params": ["operatorOverrideEnabled", "maxRemoteResponseSec"],
    },
    "sidewalk_operation": {
        "situation": "PedestrianAhead",
        "action": "YieldWait, SlowDown, Continue",
        "params": ["sidewalkAllowed", "pedestrianPriorityRequired"],
    },
    "crosswalk_operation": {
        "situation": "PedestrianAhead",
        "action": "Stop, YieldWait",
        "params": ["crosswalkStopRequired", "crosswalkMaxSpeedKmh"],
    },
    "terrain_or_dynamic_safety": {
        "situation": "FallOrTilt, TerrainRisk, ObstacleAhead",
        "action": "ReplanPath, LocalAvoidance, Stop",
        "params": ["maxSlopeDeg", "obstacleClearanceM"],
    },
    "legal_background": {
        "situation": "",
        "action": "",
        "params": ["legalBasisSourceIds"],
    },
    "certification_process": {
        "situation": "",
        "action": "RequestOperator",
        "params": ["certificationSourceIds"],
    },
    "data_recording": {
        "situation": "",
        "action": "",
        "params": ["recordTelemetry", "incidentLogRetentionDays"],
    },
}


def parse_candidate_row(line: str) -> dict | None:
    if not line.startswith("| CAND-"):
        return None
    columns = [column.strip() for column in line.strip().strip("|").split("|")]
    if len(columns) != 7:
        return None
    return {
        "candidateId": columns[0],
        "category": columns[1],
        "keyword": columns[2],
        "extractedText": columns[3].replace("｜", "|"),
        "processedSection": columns[4],
        "sourceLocationHint": columns[5],
        "reviewStatus": columns[6],
    }


def load_candidates() -> list[dict]:
    candidates: list[dict] = []
    for path in sorted(CANDIDATE_DIR.glob("KOR-*_policy_candidates.md")):
        source_id = path.name.split("_", 1)[0]
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            parsed = parse_candidate_row(line)
            if parsed:
                parsed["sourceId"] = source_id
                candidates.append(parsed)
    return candidates


def priority_for(candidate: dict) -> tuple[str, str]:
    category = candidate["category"]
    keyword = candidate["keyword"]
    text = candidate["extractedText"]

    if category in MVP_CATEGORIES and (
        keyword in HIGH_KEYWORDS or any(term in text for term in HIGH_KEYWORDS)
    ):
        return (
            "high",
            "MVP 필수 정책 카테고리와 candidate keyword/text가 직접 연결됨",
        )
    if category in MVP_CATEGORIES:
        return ("high", "MVP 필수 정책 카테고리에 속함")
    if category in MEDIUM_CATEGORIES:
        return (
            "medium",
            "법적 배경 또는 인증 절차 관련 후보이며 정책 파라미터 직접 변환은 후순위",
        )
    return ("low", "MVP 정책 판단과 직접 연결성이 낮거나 추가 원문 확인 전 활용도가 낮음")


def triage_candidate(candidate: dict) -> dict:
    priority, reason = priority_for(candidate)
    links = CATEGORY_LINKS.get(candidate["category"], {"situation": "", "action": "", "params": []})
    return {
        "candidateId": candidate["candidateId"],
        "sourceId": candidate["sourceId"],
        "category": candidate["category"],
        "priority": priority,
        "extractedText": candidate["extractedText"],
        "reasonForPriority": reason,
        "linkedMvpSituation": links["situation"],
        "linkedMvpAction": links["action"],
        "relatedPolicyParams": links["params"],
        "reviewStatus": "needs_pdf_check",
    }


def count_by_source_and_priority(review_queue: list[dict]) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for item in review_queue:
        source = item["sourceId"]
        result.setdefault(source, {"high": 0, "medium": 0, "low": 0})
        result[source][item["priority"]] += 1
    return result


def write_triage_markdown(triage: dict) -> None:
    lines = [
        "# Policy Candidate Triage",
        "",
        f"* totalCandidates: {triage['totalCandidates']}",
        f"* high: {triage['byPriority']['high']}",
        f"* medium: {triage['byPriority']['medium']}",
        f"* low: {triage['byPriority']['low']}",
        "* 모든 reviewStatus: needs_pdf_check",
        "",
        "## Source별 우선순위 수",
        "",
        "| sourceId | high | medium | low |",
        "| --- | ---: | ---: | ---: |",
    ]
    for source_id, counts in triage["bySource"].items():
        lines.append(f"| {source_id} | {counts['high']} | {counts['medium']} | {counts['low']} |")

    lines.extend(
        [
            "",
            "## Review Queue",
            "",
            "| candidateId | sourceId | category | priority | reasonForPriority | reviewStatus |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
    )
    for item in triage["reviewQueue"]:
        lines.append(
            f"| {item['candidateId']} | {item['sourceId']} | {item['category']} | {item['priority']} | {item['reasonForPriority']} | {item['reviewStatus']} |"
        )
    TRIAGE_MD_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def write_manual_review_queue(review_queue: list[dict], by_source: dict[str, dict[str, int]]) -> None:
    high_items = [item for item in review_queue if item["priority"] == "high"]
    lines = [
        "# Manual Review Queue",
        "",
        "## 1. 목적",
        "",
        "201개 자동 후보 중 MVP 정책과 직접 연결되는 후보를 먼저 원본 PDF와 대조하기 위한 검토 큐이다.",
        "",
        "## 2. 검토 우선순위",
        "",
        "* High: 먼저 검토",
        "* Medium: 정책 설명/배경용으로 이후 검토",
        "* Low: 필요 시 후순위 검토",
        "",
        "## 3. High Priority 후보 목록",
        "",
        "| sourceId | candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for item in high_items:
        params = ", ".join(item["relatedPolicyParams"])
        text = item["extractedText"].replace("|", "｜")
        lines.append(
            f"| {item['sourceId']} | {item['candidateId']} | {item['category']} | {text} | {item['linkedMvpSituation']} | {item['linkedMvpAction']} | {params} |"
        )

    lines.extend(
        [
            "",
            "## 4. Source별 검토 권장 순서",
            "",
            "1. KOR-003 KIRIA 운행안전인증 가이드북",
            "2. KOR-004 운행안전인증 절차 및 기준 고시",
            "3. KOR-002 도로교통법",
            "4. KOR-005 도로교통법 제2조 하위법령",
            "5. KOR-001 지능형로봇법",
            "",
            "단, 이 순서는 검토 편의를 위한 제안이며, status를 변경하지 않는다.",
            "",
            "## 5. 수동 검토 체크 방법",
            "",
            "* 원본 PDF에서 candidate 문장이 실제로 존재하는지 확인",
            "* 페이지/조항 번호 확인",
            "* 정책으로 연결 가능한지 확인",
            "* confirmed/rejected 판단은 다음 단계에서 별도 작업으로 수행",
        ]
    )
    MANUAL_REVIEW_QUEUE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def update_checklists(by_source: dict[str, dict[str, int]]) -> None:
    for source_id, counts in by_source.items():
        path = REVIEW_DIR / f"{source_id}_review_checklist.md"
        content = path.read_text(encoding="utf-8-sig")
        marker = "## 7. Triage Summary"
        section = "\n".join(
            [
                "",
                marker,
                "",
                "* triageFilePath: data/sources/review/triage/policy_candidate_triage.json",
                f"* highPriorityCount: {counts['high']}",
                f"* mediumPriorityCount: {counts['medium']}",
                f"* lowPriorityCount: {counts['low']}",
                "* nextReviewAction: 원본 PDF 대조 필요",
                "",
            ]
        )
        if marker in content:
            content = content.split(marker, 1)[0].rstrip() + section
        else:
            content = content.rstrip() + section
        path.write_text(content, encoding="utf-8-sig")


def main() -> int:
    TRIAGE_DIR.mkdir(parents=True, exist_ok=True)
    candidates = load_candidates()
    review_queue = [triage_candidate(candidate) for candidate in candidates]
    by_priority = Counter(item["priority"] for item in review_queue)
    by_category = Counter(item["category"] for item in review_queue)
    by_source = count_by_source_and_priority(review_queue)
    triage = {
        "generatedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "totalCandidates": len(review_queue),
        "byPriority": {
            "high": by_priority["high"],
            "medium": by_priority["medium"],
            "low": by_priority["low"],
        },
        "byCategory": dict(sorted(by_category.items())),
        "bySource": dict(sorted(by_source.items())),
        "reviewQueue": review_queue,
    }
    TRIAGE_JSON_PATH.write_text(
        json.dumps(triage, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    write_triage_markdown(triage)
    write_manual_review_queue(review_queue, by_source)
    update_checklists(by_source)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
