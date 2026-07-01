# Result Analysis V2 API Specification

이 문서는 `Agents`의 `/api/v2/analysis/run` public API 응답과 review artifact 저장 규격을 정의한다.

## 1. 목적

`/api/v2/analysis/run`은 기존 UserProject의 특정 run을 분석하고, UI 표시용 public 응답과 review 폴더의 상세 분석 파일을 생성한다.

중요한 전제:

- API는 `<UserProject>` 자체를 생성하지 않는다.
- 원본 `<project_path>/policy/`와 `<project_path>/scenario.json`은 수정하지 않는다.
- 정책/환경 수정 후보는 review 폴더 하위 복사본에만 생성한다.
- public API 응답은 UI 표시 필드만 포함한다.
- 상세 근거와 수정 후보 payload는 review JSON 파일에 저장한다.

## 2. Endpoint

| 항목 | 값 |
|---|---|
| Method | `POST` |
| Path | `/api/v2/analysis/run` |
| Content-Type | `application/json` |
| Success HTTP Status | `200` |

요청 형식 오류는 이 endpoint에서만 HTTP `400` + failed body를 사용한다. 다른 endpoint의 validation error는 FastAPI 기본 응답을 유지한다.

## 3. Request Body

```json
{
  "project_path": "C:/Users/user/Documents/OdiroProject7",
  "run_id": "000001",
  "prompt": "이 Run의 실패 원인과 개선 방향을 분석해줘"
}
```

| 필드 | 타입 | 필수 | 설명 |
|---|---:|---:|---|
| `project_path` | string | Y | 기존 UserProject root 경로 |
| `run_id` | string | Y | 6자리 run id |
| `prompt` | string | N | 선택 분석 관점. 공백이면 없는 값으로 처리 |

## 4. Success Response

```json
{
  "schema": "analysis_run_response_v2",
  "version": 2,
  "status": "success",
  "run_id": "000001",
  "review_id": "0001",
  "run_overview": {
    "total_play_time_s": 60.0,
    "success_rate": 0.0,
    "collision_count": 0,
    "episode_count": 1,
    "display": {
      "total_play_time": "01:00",
      "success_rate": "0%",
      "collision_count": "0회"
    }
  },
  "episodes": [
    {
      "episode_id": "000001",
      "duration_s": 60.0,
      "outcome": "failure",
      "display": {
        "duration": "60.0 s",
        "outcome": "실패"
      }
    }
  ],
  "analysis_scope": {
    "experiments_count": 1,
    "runs_count": 1,
    "episodes_count": 1
  },
  "summary": {
    "overall_judgement": "change_recommended",
    "message": "주행 정책 검토가 필요한 실패가 발생했습니다."
  },
  "metrics": {
    "success_count": 0,
    "failure_count": 1,
    "collision_count": 0,
    "static_obstacle_collision_count": 0,
    "pedestrian_collision_count": 0,
    "near_miss_count": 0,
    "repath_count": 0,
    "robot_tip_over_count": 0,
    "blocked_region_violation_count": 0,
    "penalty_region_violation_count": 0
  },
  "recommendation_type": "policy_review",
  "insights": [
    {
      "severity": "high",
      "title": "충돌 없이 제한 시간 초과",
      "description": "충돌은 발생하지 않았지만 목표 도달 실패, 제한 시간 초과, 또는 정체 신호가 확인되었습니다."
    }
  ],
  "patterns": [],
  "recommendations": [
    {
      "target": "policy",
      "priority": "high",
      "title": "정체와 제한 시간 초과 대응 정책 검토",
      "reason": "제한 시간 초과 또는 정체가 확인되어 감속, 정지, 재경로 탐색 조건을 검토할 필요가 있습니다.",
      "recommendation": "반복 실패 구간에서 속도, 경로 추종 허용 오차, 전방 주시 거리, 조향 변화량을 보수적으로 조정해 동일 조건에서 재실행하는 것을 권장합니다."
    }
  ],
  "warnings": []
}
```

### 4.1 Public Response Fields

| 필드 | 타입 | 설명 |
|---|---:|---|
| `schema` | string | 고정값 `analysis_run_response_v2` |
| `version` | number | 고정값 `2` |
| `status` | string | `success` 또는 `failed` |
| `run_id` | string | 요청한 run id. 안전하게 알 수 없으면 생략 가능 |
| `review_id` | string | 생성된 review id. review를 만들지 않으면 생략 |
| `run_overview` | object | 상단 카드 UI 표시용 집계 |
| `episodes` | array | 에피소드 리플레이 카드 목록 |
| `analysis_scope` | object | 분석 범위 |
| `summary` | object | 한 줄 요약과 종합 판단 |
| `metrics` | object | 주요 집계 지표 |
| `recommendation_type` | string | 추천 유형 |
| `insights` | array | UI 표시용 핵심 분석 포인트. 최대 3개 |
| `patterns` | array | 반복 패턴 |
| `recommendations` | array | UI 표시용 추천 텍스트 |
| `warnings` | array[string] | 비치명 경고 |

`response_model_exclude_none=True`를 적용하므로 값이 없는 optional field는 응답에서 생략된다.

## 5. 제거된 Public Response Fields

아래 필드는 public API 응답에 포함하지 않는다.

| 필드 | 저장 위치 |
|---|---|
| `analysis_text` | 저장하지 않음. public 요약은 `summary`, `insights`, `recommendations` 사용 |
| `analysis_mode` | 내부 graph state에서만 사용 |
| `modified_policy_json` | `recommendations.json.modified_policy_json` |
| `modified_environment_json` | `recommendations.json.modified_environment_json` |
| `recommendations[].id` | `recommendations.json.recommendations[].id` |
| `recommendations[].proposed_change` | `recommendations.json.recommendations[].proposed_change` |

아래 review artifact path 필드는 public API 응답에 다시 추가하지 않는다.

```text
review_dir
status_path
request_path
report_path
manifest_path
recommendations_path
rag_evidence_path
```

## 6. run_overview와 metrics 계산 기준

`run_overview`, `episodes`, public `metrics.success_count/failure_count/collision_count`는 `runs/<run_id>/summary.json`의 `rows[]`를 기준으로 계산한다. UE의 `ProjectRunResultDashboard.cpp::IsSuccessRow` 기준과 맞춘다.

| 값 | 계산 기준 |
|---|---|
| `episode_count` | `summary.json rows[]` 개수 |
| `total_play_time_s` | `rows[].duration_s` 합산. 없으면 `rows[].metrics.duration_s` fallback |
| `success_count` | UE 성공 판정 row 수 |
| `failure_count` | `episode_count - success_count` |
| `success_rate` | episode가 있으면 `success_count / episode_count`, 없으면 `0.0` |
| `collision_count` | `blocked_region_collision_count + pedestrian_collision_count + static_obstacle_collision_count` 합산 |

성공 row는 아래 중 하나를 만족한다.

- `outcome == "Success"`
- `metrics.goal_reached > 0`

단, 아래 즉시 GoalReached row는 성공 집계에서 제외하고 public `episodes[].outcome`도 `failure`로 표시한다.

- `terminal_reason == "GoalReached"`이고 `duration_s <= 0.1`
- 또는 start/goal이 같은 segment이고 아래 조건이 참인 경우

```text
sqrt((start.along_m - goal.along_m)^2 + (start.offset_m - goal.offset_m)^2) <= goal_threshold_m + 0.001
```

위치 계산에 필요한 값이 없으면 `duration_s <= 0.1` 기준만 적용한다.

표시 문자열:

- `display.total_play_time`: 합산 초를 반올림해 1시간 미만 `MM:SS`, 1시간 이상 `H:MM:SS`
- `display.success_rate`: 비음수 percent에 대해 `floor(value + 0.5)` 후 `0~100` clamp, `%` 포함
- `display.collision_count`: `회` 포함
- `episodes[].display.duration`: `"{duration_s:.1f} s"`
- `episodes[].display.outcome`: `성공` 또는 `실패`

## 7. recommendation_type

| 값 | 의미 |
|---|---|
| `environment_review` | 환경/시나리오 수정 검토 필요 |
| `policy_review` | 주행 정책 수정 검토 필요 |
| `none` | 별도 수정 추천 없음 |
| `insufficient_data` | 판단할 수 있는 데이터 부족 |

`setup_failed`, `unknown`은 `recommendation_type`에 넣지 않는다. SetupFailed는 report finding으로 기록하며, setup-only run은 후보 artifact 없이 `recommendation_type: "none"`과 `summary.overall_judgement: "change_recommended"`를 사용할 수 있다.

## 8. Insufficient Data와 Failed Response

run directory가 없거나 `summary.json rows[]` 기반 public 분석을 만들 수 없으면 API 처리 실패가 아니라 `status: "success"`, `recommendation_type: "insufficient_data"`로 응답한다.

```json
{
  "schema": "analysis_run_response_v2",
  "version": 2,
  "status": "success",
  "run_id": "000001",
  "summary": {
    "overall_judgement": "insufficient_data",
    "message": "분석할 수 있는 주행 로그가 부족합니다."
  },
  "recommendation_type": "insufficient_data",
  "insights": [],
  "patterns": [],
  "recommendations": [],
  "warnings": []
}
```

API가 응답 생성을 정상적으로 완료하지 못한 경우만 `status: "failed"`를 사용한다.

```json
{
  "schema": "analysis_run_response_v2",
  "version": 2,
  "status": "failed",
  "run_id": "000001",
  "error": {
    "code": "ANALYSIS_PROCESSING_FAILED",
    "message": "분석 결과를 생성하는 중 오류가 발생했습니다.",
    "phase": "build_response"
  },
  "warnings": []
}
```

validation 실패 예:

```json
{
  "schema": "analysis_run_response_v2",
  "version": 2,
  "status": "failed",
  "error": {
    "code": "INVALID_ANALYSIS_REQUEST",
    "message": "분석 요청 형식이 올바르지 않습니다.",
    "phase": "request_validation"
  },
  "warnings": []
}
```

validation failed body의 `run_id`는 JSON body에서 안전하게 string으로 읽을 수 있을 때만 포함한다.

## 9. Review Output Structure

run directory가 존재하면 API는 아래 경로를 생성한다.

```text
<project_path>/runs/<run_id>/review/<review_id>/
  status.json
  request.json
  report.json
  recommendations.json
  manifest.json
  policy/             # policy_review일 때만 생성
  scenario.json       # environment_review일 때만 생성
```

Result analysis v2 내부 file-based RAG context는 public API 응답과 review artifact 구조에 저장하지 않는다. `rag_evidence.json`, `review/internal/`, 기타 RAG debug artifact 파일은 생성하지 않는다.

## 10. report.json

`report.json`은 상세 분석 근거를 저장한다.

```json
{
  "summary": {},
  "metrics": {},
  "data_coverage": {},
  "insights": [
    {
      "id": "INS-001",
      "type": "timeout_without_collision",
      "severity": "high",
      "title": "충돌 없이 제한 시간 초과",
      "detail": "충돌은 발생하지 않았지만 목표 도달 실패, 제한 시간 초과, 또는 정체 신호가 확인되었습니다.",
      "description": "충돌은 발생하지 않았지만 목표 도달 실패, 제한 시간 초과, 또는 정체 신호가 확인되었습니다.",
      "related_episode_ids": ["000001"],
      "evidence_ids": ["EV-0001"]
    }
  ],
  "findings": [],
  "evidence": [],
  "patterns": []
}
```

`report.json.insights[]`는 public `insights[]`보다 상세한 내부 필드를 가질 수 있다.

## 11. recommendations.json

`recommendations.json`은 추천 유형 결정 이유, 상세 추천, 수정 후보 payload, 생성 artifact 상태를 저장한다.

```json
{
  "recommendation_type": "policy_review",
  "reason": "주행 정책 검토가 필요한 실패 근거가 확인되었습니다.",
  "evidence_ids": ["EV-0001"],
  "recommendations": [
    {
      "id": "REC-001",
      "target": "policy",
      "priority": "high",
      "title": "정체와 제한 시간 초과 대응 정책 검토",
      "reason": "제한 시간 초과 또는 정체가 확인되어 감속, 정지, 재경로 탐색 조건을 검토할 필요가 있습니다.",
      "recommendation": "반복 실패 구간에서 속도, 경로 추종 허용 오차, 전방 주시 거리, 조향 변화량을 보수적으로 조정해 동일 조건에서 재실행하는 것을 권장합니다.",
      "proposed_change": {
        "type": "policy_parameter_adjustment",
        "content": {
          "followSpeedKmh_max": 3.5
        }
      }
    }
  ],
  "modified_policy_json": [
    {
      "source_recommendation_id": "REC-001",
      "target": "policy",
      "content": {
        "followSpeedKmh_max": 3.5
      }
    }
  ],
  "modified_environment_json": [],
  "artifacts": {
    "policy": {
      "generated": true,
      "path": "runs/000001/review/0001/policy"
    },
    "environment": {
      "generated": false,
      "path": null
    }
  },
  "artifact_warnings": []
}
```

## 12. status.json, request.json, manifest.json

`status.json`과 `request.json`의 필드는 기존 lifecycle 계약을 유지한다.

`manifest.json`은 아래 필드를 포함한다.

| 필드 | 설명 |
|---|---|
| `review_id` | 현재 review id |
| `run_id` | 분석 대상 run id |
| `generated_files` | 생성된 review 파일 목록 |
| `source_run_files` | 분석에 사용한 원본 run 파일 목록 |
| `based_on_review_id` | 직전 completed review id |
| `snapshot_hashes` | 현재 run snapshot hash |
| `comparison` | 이전 run 비교 결과 |
| `artifacts` | `recommendations.json.artifacts`와 같은 후보 artifact 상태 |

`manifest.json` 구조는 RAG 작업으로 확장하지 않는다. `rag_diagnostic`, raw chunk, source, retrieval score/chunk score, matched_fields, chunk_id, card_id, `rag_evidence.json` 경로는 저장하지 않는다. RAG route와 diagnostic은 `ResultAnalysisGraphRunnerV2` 내부 state 또는 테스트용 `last_state`에서만 확인한다.

## 13. Candidate Artifacts

### Policy Candidate

조건:

- `recommendation_type == "policy_review"`
- `<project_path>/policy/`가 존재

생성 위치:

```text
<project_path>/runs/<run_id>/review/<review_id>/policy/
```

원본 policy는 수정하지 않는다. `__pycache__`, `*.pyc`, `.DS_Store`, symlink는 복사 제외한다.

### Environment Candidate

조건:

- `recommendation_type == "environment_review"`
- `<project_path>/scenario.json`이 존재

생성 위치:

```text
<project_path>/runs/<run_id>/review/<review_id>/scenario.json
```

원본 scenario는 수정하지 않는다. 후보 원본은 root `<project_path>/scenario.json`이며 snapshot/episode scenario를 사용하지 않는다.

## 14. Verification

권장 검증:

```bash
cd Agents
uv run pytest tests/test_v2_analysis_run_api.py tests/test_v2_result_analysis_graph_runner.py tests/test_v2_analysis_recommendation_artifacts.py -q
uv run pytest -q
uv run python -m harness.checks.check_contract_validation
```
