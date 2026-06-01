# MANUAL_CONFIRMATION_CLI

## 목적

`scripts/manual_confirm.py`는 사람이 원본 PDF를 직접 확인한 뒤 high priority 후보의 `manualReviewStatus`를 안전하게 입력하기 위한 CLI다.

이 CLI는 자동 판단을 하지 않는다. policy knowledge card도 생성하지 않는다.

## 조회 명령

```bash
uv run python scripts/manual_confirm.py list
uv run python scripts/manual_confirm.py list --source KOR-003
uv run python scripts/manual_confirm.py list --status pending_manual_confirmation
uv run python scripts/manual_confirm.py show CAND-KOR-003-001
uv run python scripts/manual_confirm.py summary
```

조회 명령은 파일을 수정하지 않는다.

## Confirm 명령

```bash
uv run python scripts/manual_confirm.py confirm CAND-KOR-003-001 --page "p.12" --section "운행속도 항목" --text "원본 PDF에서 확인한 짧은 문장" --reviewer "사용자명" --reason "속도 정책 파라미터와 연결 가능" --next-action "create_policy_card"
```

`--yes`가 없으면 dry-run으로만 동작한다. 실제 저장하려면 `--yes`를 추가한다.

confirmed에는 `rawPdfPage` 또는 `rawPdfSection` 중 하나 이상, `confirmedText`, `reviewer`, `decisionReason`이 필요하다.

## Reject 명령

```bash
uv run python scripts/manual_confirm.py reject CAND-KOR-003-001 --reviewer "사용자명" --reason "원본 PDF에서 확인되지 않음" --next-action "exclude_from_policy_card" --yes
```

`--yes`가 없으면 dry-run으로만 동작한다. rejected에는 `reviewer`와 `decisionReason`이 필요하다.

## 주의

confirmed/rejected 판단은 사람이 원본 PDF를 보고 수행해야 한다. confirmed 후보가 생겨도 policy card 생성은 별도 단계에서 수행한다.
