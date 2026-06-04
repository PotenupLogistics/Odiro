from __future__ import annotations

import json
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAGE_HINTS_PATH = ROOT / "data" / "sources" / "review" / "high_priority" / "page_hints" / "high_priority_page_hints.json"
MANUAL_CONFIRMATION_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
TRIAGE_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.json"
PACK_DIR = ROOT / "data" / "sources" / "review" / "manual_review_pack"
EXECUTION_PLAN_PATH = ROOT / "docs" / "manual_review" / "MANUAL_REVIEW_EXECUTION_PLAN.md"
INPUT_GUIDE_PATH = ROOT / "docs" / "manual_review" / "MANUAL_CONFIRMATION_INPUT_GUIDE.md"
EXPECTED_SOURCE_IDS = ["KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"]
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


def escape_md_cell(value: object) -> str:
    if isinstance(value, list):
        value = ", ".join(str(item) for item in value)
    return str(value).replace("|", "｜").replace("\n", " ")


def trim_text(value: str, limit: int = 300) -> str:
    return value if len(value) <= limit else value[:limit].rstrip() + "..."


def load_data() -> tuple[dict, dict, dict]:
    page_hints = json.loads(PAGE_HINTS_PATH.read_text(encoding="utf-8-sig"))
    manual = json.loads(MANUAL_CONFIRMATION_PATH.read_text(encoding="utf-8-sig"))
    triage = json.loads(TRIAGE_PATH.read_text(encoding="utf-8-sig"))
    return page_hints, manual, triage


def write_source_packs(page_hints: dict, manual: dict) -> None:
    manual_by_id = {item["candidateId"]: item for item in manual["items"]}
    by_source: dict[str, list[dict]] = defaultdict(list)
    for hint in page_hints["items"]:
        merged = {**manual_by_id[hint["candidateId"]], **hint}
        by_source[hint["sourceId"]].append(merged)

    for source_id in EXPECTED_SOURCE_IDS:
        items = by_source[source_id]
        hint_counts = Counter(item["hintStatus"] for item in items)
        lines = [
            f"# {source_id} Manual Review Pack",
            "",
            "## 1. 검토 대상 문서",
            "",
            f"* sourceId: {source_id}",
            f"* sourceTitle: {SOURCE_TITLES[source_id]}",
            f"* rawPdfPath: {RAW_PATHS[source_id]}",
            f"* highPriorityCount: {len(items)}",
            f"* pageHintFoundCount: {hint_counts['found']}",
            f"* pageHintPartialCount: {hint_counts['partial']}",
            "",
            "## 2. 검토 방법",
            "",
            "1. rawPdfPath의 원본 PDF를 연다.",
            "2. 아래 후보의 page hint 페이지를 확인한다.",
            "3. extractedText가 원본 PDF에 실제로 존재하는지 확인한다.",
            "4. 정책으로 사용할 수 있는 문장인지 판단한다.",
            "5. manual_confirmation_results.json에 사람이 직접 다음 필드를 입력한다.",
            "",
            "   * manualReviewStatus: confirmed 또는 rejected",
            "   * rawPdfPage",
            "   * rawPdfSection",
            "   * confirmedText",
            "   * reviewer",
            "   * reviewedAt",
            "   * decisionReason",
            "   * nextAction",
            "",
            "## 3. 후보 목록",
            "",
            "| No | candidateId | category | hintStatus | pageHints | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | 검토 메모 |",
            "| -- | ----------- | -------- | ---------- | --------- | ------------- | ------------------ | --------------- | ------------------- | ----- |",
        ]
        for index, item in enumerate(items, start=1):
            pages = ", ".join(str(hint["pageNumber"]) for hint in item["pageHints"])
            lines.append(
                f"| {index} | {item['candidateId']} | {item['category']} | {item['hintStatus']} | {pages} | {escape_md_cell(trim_text(item['extractedText']))} | {escape_md_cell(item['linkedMvpSituation'])} | {escape_md_cell(item['linkedMvpAction'])} | {escape_md_cell(item['relatedPolicyParams'])} |  |"
            )
        lines.extend(
            [
                "",
                "주의:",
                "",
                "* pageHints는 확정 근거가 아니라 검토 힌트이다.",
                "* confirmed/rejected 판단은 사람이 수행한다.",
                "* extractedText가 긴 경우 300자 이내로만 표시한다.",
                "* 원문을 길게 복사하지 않는다.",
            ]
        )
        (PACK_DIR / f"{source_id}_review_pack.md").write_text(
            "\n".join(lines) + "\n",
            encoding="utf-8-sig",
        )


def write_execution_plan(page_hints: dict, manual: dict) -> None:
    hint_summary = page_hints["hintSummary"]
    status_counts = Counter(item["manualReviewStatus"] for item in manual["items"])
    lines = [
        "# Manual Review Execution Plan",
        "",
        "## 1. 현재 상태",
        "",
        f"* high priority candidate: {page_hints['totalCandidates']}",
        f"* page hint found: {hint_summary['found']}",
        f"* page hint partial: {hint_summary['partial']}",
        f"* confirmed: {status_counts['confirmed']}",
        f"* rejected: {status_counts['rejected']}",
        f"* pending: {status_counts['pending_manual_confirmation']}",
        "",
        "## 2. 검토 권장 순서",
        "",
        "1. KOR-003 KIRIA 운행안전인증 가이드북",
        "2. KOR-004 운행안전인증 절차 및 기준 고시",
        "3. KOR-002 도로교통법",
        "4. KOR-001 지능형로봇법",
        "5. KOR-005 도로교통법 제2조 하위법령",
        "",
        "## 3. 먼저 확인할 정책 카테고리",
        "",
        "* emergency_stop",
        "* speed_policy",
        "* perception_requirement",
        "* operator_control",
        "* sidewalk_operation",
        "* terrain_or_dynamic_safety",
        "",
        "## 4. confirmed 처리 기준",
        "",
        "confirmed는 아래 조건을 모두 만족할 때만 가능하다.",
        "",
        "* 원본 PDF에서 후보 내용 확인",
        "* page 또는 조항 위치 확인",
        "* confirmedText 작성",
        "* 프로젝트 정책 파라미터 또는 행동과 연결 가능",
        "* decisionReason 작성",
        "",
        "## 5. rejected 처리 기준",
        "",
        "rejected는 아래 중 하나에 해당할 때 가능하다.",
        "",
        "* 원본 PDF에서 찾을 수 없음",
        "* OCR/추출 오류",
        "* 정책과 직접 관련 없음",
        "* 중복 후보",
        "* 문맥상 잘못 추출됨",
        "",
        "## 6. 수동 입력 위치",
        "",
        "* data/sources/review/confirmed/manual_confirmation_results.json",
        "  또는",
        "* data/sources/review/confirmed/manual_confirmation_results.md",
        "",
        "## 7. 수동 검토 후 해야 할 일",
        "",
        "1. manual_confirmation_results.json 수정",
        "2. uv run python -m harness.checks.check_all 실행",
        "3. uv run pytest 실행",
        "4. confirmed 후보가 생기면 다음 단계에서 policy knowledge card 생성",
    ]
    EXECUTION_PLAN_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def write_input_guide() -> None:
    lines = [
        "# Manual Confirmation Input Guide",
        "",
        "## 수정해야 하는 필드",
        "",
        "* manualReviewStatus",
        "* rawPdfPage",
        "* rawPdfSection",
        "* confirmedText",
        "* reviewer",
        "* reviewedAt",
        "* decisionReason",
        "* nextAction",
        "",
        "## confirmed 예시",
        "",
        "```json",
        "{",
        '  "manualReviewStatus": "confirmed",',
        '  "rawPdfPage": "p.12",',
        '  "rawPdfSection": "제X조 또는 표 제목",',
        '  "confirmedText": "원본 PDF에서 확인한 짧은 문장",',
        '  "reviewer": "검토자 이름",',
        '  "reviewedAt": "YYYY-MM-DD",',
        '  "decisionReason": "정책 파라미터 또는 액션과 연결 가능하기 때문",',
        '  "nextAction": "create_policy_card"',
        "}",
        "```",
        "",
        "## rejected 예시",
        "",
        "```json",
        "{",
        '  "manualReviewStatus": "rejected",',
        '  "decisionReason": "원본 PDF에서 확인되지 않거나 정책과 직접 관련이 약함",',
        '  "nextAction": "exclude_from_policy_card"',
        "}",
        "```",
        "",
        "## pending 상태 유지 예시",
        "",
        "```json",
        "{",
        '  "manualReviewStatus": "pending_manual_confirmation",',
        '  "rawPdfPage": "",',
        '  "rawPdfSection": "",',
        '  "confirmedText": "",',
        '  "reviewer": "",',
        '  "reviewedAt": "",',
        '  "decisionReason": "",',
        '  "nextAction": "needs_pdf_check"',
        "}",
        "```",
        "",
        "예시는 가상의 형식 예시이며 실제 후보 상태를 변경하지 않는다.",
    ]
    INPUT_GUIDE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")


def main() -> int:
    PACK_DIR.mkdir(parents=True, exist_ok=True)
    page_hints, manual, triage = load_data()
    _ = triage
    write_source_packs(page_hints, manual)
    write_execution_plan(page_hints, manual)
    write_input_guide()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
