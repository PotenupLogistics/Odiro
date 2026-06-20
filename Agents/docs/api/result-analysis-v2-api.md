# Result Analysis V2 API Specification

이 문서는 `Agents` 결과 분석 에이전트의 `/api/v2/analysis/run` API와, API 실행 후 생성되는 review artifact 파일 규격을 정의한다.

## 1. 목적

`/api/v2/analysis/run`은 기존 UserProject의 특정 run 로그를 분석해 실패 원인, 근거, 추천 유형, 추천 후보 artifact를 생성한다.

중요한 전제:

- API는 `<UserProject>` 자체를 생성하지 않는다.
- `<UserProject>`는 request body의 `project_path`로 전달받는 기존 사용자 프로젝트 경로다.
- API가 새로 생성하는 것은 `<project_path>/runs/<run_id>/review/<review_id>/` 하위의 분석 결과 파일뿐이다.
- 원본 `<project_path>/policy/`와 `<project_path>/scenario.json`은 절대 수정하지 않는다.
- 정책/환경 추천 후보는 review 폴더 하위의 복사본에만 반영한다.

## 2. Endpoint

| 항목 | 값 |
|---|---|
| Method | `POST` |
| Path | `/api/v2/analysis/run` |
| Content-Type | `application/json` |
| Success HTTP Status | `200` |

분석 데이터가 부족하거나 run directory가 없어도, 현재 계약에서는 HTTP `200`으로 응답하고 `summary.overall_judgement` 및 `recommendation_type`을 `insufficient_data` 계열로 표현한다.

## 3. Request Body

```json
{
  "project_path": "C:/Users/user/Downloads/testset_1 (1)",
  "run_id": "000001",
  "prompt": "이 Run의 실패 원인과 정책 수정 또는 환경 수정 필요 여부를 분석해줘"
}
```

### 3.1 Request Fields

| 필드 | 타입 | 필수 | 설명 |
|---|---:|---:|---|
| `project_path` | string | Y | 기존 UserProject root 경로. 이 경로 하위의 `runs/<run_id>`를 분석한다. |
| `run_id` | string | Y | 분석할 run id. 예: `000001`. |
| `prompt` | string | N | 사용자가 원하는 분석 관점. 예: 장애물 중심, 정책 중심 등. 없으면 기본 분석을 수행한다. |

## 4. UserProject Input Structure

API는 아래 구조가 이미 존재한다고 가정한다.

```text
<UserProject>/
  setting.json
  profile.json
  scenario.json
  policy/
    __init__.py
    ...
  runs/
    <RunId>/
      snapshot/
        setting.json
        profile.json
        scenario.json
        policy/
      summary.json
      episodes/
        <EpisodeId>/
          scenario.json
          result.json
          events.jsonl
          actions.jsonl
          trace.jsonl
```

### 4.1 분석 입력 우선순위

| 입력 파일 | 역할 |
|---|---|
| `runs/<run_id>/episodes/<episode_id>/result.json` | episode별 성공/실패, terminal reason, metrics의 주요 source of truth. |
| `runs/<run_id>/episodes/<episode_id>/events.jsonl` | PascalCase event type 기반의 보조 근거. |
| `runs/<run_id>/summary.json` | episode result가 부족할 때 fallback 집계로 사용 가능. |
| `<project_path>/scenario.json` | 환경 추천 후보 artifact의 원본. snapshot/episode scenario가 아니다. |
| `<project_path>/policy/` | 정책 추천 후보 artifact의 원본. snapshot policy가 아니다. |

## 5. Review Output Structure

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

### 5.1 Review Id

| 규칙 | 설명 |
|---|---|
| 형식 | `0001`, `0002`, `0003`처럼 4자리 zero-padding 문자열 |
| 증가 방식 | 같은 run을 다시 분석하면 기존 review를 덮어쓰지 않고 다음 번호를 생성 |
| 이전 review 연결 | 직전 completed review가 있으면 `manifest.json.based_on_review_id`에 기록 |
| request 저장 | `request.json`에는 `based_on_review_id`를 저장하지 않음 |

## 6. API Response Body

API response는 review JSON 파일과 일부 값이 다르다. 특히 `analysis_text`는 API response 전용이며 review 파일에는 저장하지 않는다.

```json
{
  "review_id": "0002",
  "run_id": "000005",
  "analysis_mode": "llm",
  "summary": {
    "overall_judgement": "change_recommended",
    "message": "환경 또는 장애물 관련 충돌 근거가 확인되어 환경 검토가 필요합니다."
  },
  "metrics": {
    "success_count": 1,
    "failure_count": 3,
    "collision_count": 152,
    "static_obstacle_collision_count": 152,
    "pedestrian_collision_count": 0,
    "blocked_region_violation_count": 0,
    "penalty_region_violation_count": 0,
    "near_miss_count": 1
  },
  "recommendation_type": "environment_review",
  "recommendations": [
    {
      "id": "REC-001",
      "target": "environment",
      "priority": "high",
      "title": "정적 장애물 배치와 통로 폭 검토",
      "reason": "정적 장애물 충돌이 반복되어 현재 장애물 배치 또는 유효 통로 폭이 주행에 충분하지 않을 가능성이 있습니다.",
      "recommendation": "장애물 배치가 주행 경로를 과도하게 막지 않도록 최소 통로 폭을 늘리고, blocking 배치를 비활성화한 환경 수정 후보로 재실행해 충돌과 Timeout이 줄어드는지 확인하세요.",
      "proposed_change": {
        "type": "environment_scenario_adjustment",
        "content": {
          "disable_allow_blocking": true,
          "increase_min_clear_width_m": true,
          "increase_walkway_width_m": true,
          "reason": "static_obstacle_collision_repeated"
        }
      }
    }
  ],
  "analysis_text": "[결과 요약]\n총 4개 episode 중 성공 1회, 실패 3회가 확인되었습니다.\n\n[주요 근거]\n정적 장애물 충돌이 반복되었습니다.\n\n[판단]\n환경 또는 장애물 배치 검토가 필요합니다.\n\n[추천]\nreview 폴더에 환경 수정 후보가 생성되었습니다.",
  "warnings": [
    "skipped large file: runs\\000005\\episodes\\000001\\actions.jsonl"
  ]
}
```

### 6.1 API Response Fields

| 필드 | 타입 | 설명 |
|---|---:|---|
| `review_id` | string \| null | 생성된 review id. run directory가 없어서 review를 만들지 않으면 null일 수 있다. |
| `run_id` | string | 요청한 run id. |
| `analysis_mode` | string | 분석 모드. 예: `llm`, `fallback`. LLM 호출 성공 여부와 fallback 여부를 UI에서 구분할 때 사용한다. |
| `summary` | object | 전체 판단 요약. |
| `metrics` | object | episode result/events에서 집계한 주요 수치. |
| `recommendation_type` | string | 추천 유형. `policy_review`, `environment_review`, `none`, `insufficient_data`. |
| `recommendations` | array | 사람이 읽을 수 있는 추천 item 목록. `policy_review`/`environment_review`일 때 최소 1개 생성한다. |
| `analysis_text` | string | UI 표시용 자연어 보고서. API response에만 포함하며 review JSON에는 저장하지 않는다. |
| `warnings` | array[string] | 분석 중 발생한 비치명 warning. 예: large actions file skip, 일부 파일 누락. |

## 7. Summary Object

```json
{
  "overall_judgement": "change_recommended",
  "message": "환경 또는 장애물 관련 충돌 근거가 확인되어 환경 검토가 필요합니다."
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `overall_judgement` | string | 전체 판단. |
| `message` | string | 사용자가 읽는 한국어 요약 문장. 최종 `recommendation_type`과 primary finding 기준으로 생성한다. |

### 7.1 overall_judgement Values

| 값 | 설명 |
|---|---|
| `change_recommended` | 정책 또는 환경 수정 검토가 필요하다. |
| `no_change_recommended` | 의미 있는 실패 근거가 없어 별도 수정 추천이 없다. |
| `insufficient_data` | 분석 가능한 로그가 부족하다. |

## 8. Metrics Object

metrics는 episode result/events를 정규화해 집계한 수치다.

| 필드 | 타입 | 설명 |
|---|---:|---|
| `success_count` | integer | 성공 episode 수. |
| `failure_count` | integer | 실패 episode 수. |
| `collision_count` | integer | 충돌 총합. 보통 정적 장애물 충돌, 보행자 충돌 등을 합산한다. |
| `static_obstacle_collision_count` | integer | 정적 장애물 충돌 횟수. |
| `pedestrian_collision_count` | integer | 보행자 충돌 횟수. |
| `blocked_region_violation_count` | integer | blocked region 침범/충돌 횟수. |
| `penalty_region_violation_count` | integer | penalty region 침범 횟수. |
| `near_miss_count` | integer | 보행자 또는 장애물 근접 위험 횟수. |

구현에 따라 추가 metric이 포함될 수 있다. 클라이언트는 모르는 필드를 무시할 수 있어야 한다.

## 9. recommendation_type

| 값 | 설명 | artifact 생성 |
|---|---|---|
| `environment_review` | 정적 장애물 충돌 또는 blocked region 등 환경/배치 근거가 확인됨 | `review/<id>/scenario.json` |
| `policy_review` | 주행 판단, 경로 추종, near miss, penalty, timeout, stuck 등 정책 검토 근거가 확인됨 | `review/<id>/policy/` |
| `none` | 분석은 가능하지만 수정 추천을 뒷받침할 근거가 없음 | 없음 |
| `insufficient_data` | 분석 가능한 result/events/summary가 부족함 | 없음 |

### 9.1 판단 우선순위

| 조건 | 추천 유형 |
|---|---|
| 분석 가능한 result 파일도 없고 usable summary도 없음 | `insufficient_data` |
| evidence-backed finding 없음 | `none` |
| `static_obstacle_collision` 또는 `blocked_region_collision` 존재 | `environment_review` 우선 |
| pedestrian collision만 있고 static/blocked 환경 근거가 없음 | 현재 정책 안전 대응 성격으로 `policy_review` |
| `penalty_region_violation`, `timeout`, `goal_not_reached`, `near_miss`, `policy_decision_error`, `stuck` 등 존재 | `policy_review` |

## 10. Recommendations Array

```json
{
  "id": "REC-001",
  "target": "policy",
  "priority": "high",
  "title": "위험 영역 회피와 경로 경계 유지 조건 검토",
  "reason": "패널티 구역 침범이 확인되어 경로 추종 경계와 위험 영역 회피 조건을 보수적으로 조정할 필요가 있습니다.",
  "recommendation": "반복 실패 구간에서 속도, 경로 추종 허용 오차, look-ahead 거리, 조향 변화량을 보수적으로 조정해 동일 조건에서 재실행하는 것을 권장합니다.",
  "proposed_change": {
    "type": "policy_parameter_adjustment",
    "content": {
      "followSpeedKmh_max": 3.5,
      "maxPathErrorM_max": 0.8,
      "lookAheadDistanceM_max": 1.0,
      "pathSmoothingDistanceM_max": 0.25,
      "maxSteeringDelta_max": 0.06
    }
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `id` | string | 추천 식별자. 예: `REC-001`, `REC-LLM-001`. |
| `target` | string | 추천 대상. `policy` 또는 `environment`. |
| `priority` | string | 추천 우선순위. 현재 rule-based 추천은 주로 `high`를 사용한다. |
| `title` | string | 추천 제목. UI 카드 제목으로 사용 가능하다. |
| `reason` | string | 추천 이유. 사용자 표시용 한국어 문장. |
| `recommendation` | string | 사람이 읽을 수 있는 구체적인 추천 설명. |
| `proposed_change` | object | 후보 artifact 생성 로직이 참고할 수 있는 구조화된 수정 제안. |
| `proposed_change.type` | string | 수정 제안 유형. 예: `policy_parameter_adjustment`, `environment_scenario_adjustment`. |
| `proposed_change.content` | object | 실제 수정 후보 payload. 반드시 object여야 validator를 통과한다. |

### 10.1 Environment Recommendation proposed_change

```json
{
  "type": "environment_scenario_adjustment",
  "content": {
    "disable_allow_blocking": true,
    "increase_min_clear_width_m": true,
    "increase_walkway_width_m": true,
    "reason": "static_obstacle_collision_repeated"
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `disable_allow_blocking` | boolean | `obstacles.placements[].allow_blocking`을 `false`로 조정하라는 제안. |
| `increase_min_clear_width_m` | boolean | `obstacles.min_clear_width_m` 증가 제안. |
| `increase_walkway_width_m` | boolean | `corridor.walkway_width_m` 증가 제안. |
| `reason` | string | 구조화된 내부 reason code. 사용자 표시 문장 아님. |

### 10.2 Policy Recommendation proposed_change

```json
{
  "type": "policy_parameter_adjustment",
  "content": {
    "followSpeedKmh_max": 3.5,
    "maxPathErrorM_max": 0.8,
    "lookAheadDistanceM_max": 1.0,
    "pathSmoothingDistanceM_max": 0.25,
    "maxSteeringDelta_max": 0.06
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `followSpeedKmh_max` | number | 경로 추종 속도 상한 후보. |
| `maxPathErrorM_max` | number | 경로 이탈 허용 오차 상한 후보. |
| `lookAheadDistanceM_max` | number | look-ahead 거리 상한 후보. |
| `pathSmoothingDistanceM_max` | number | 경로 smoothing 거리 상한 후보. |
| `maxSteeringDelta_max` | number | 조향 변화량 상한 후보. |

## 11. analysis_text

`analysis_text`는 UI 표시용 자연어 보고서다.

특징:

- API response에만 포함한다.
- `status.json`, `request.json`, `report.json`, `recommendations.json`, `manifest.json`에는 저장하지 않는다.
- LLM 여부와 무관하게 rule-based/template 방식으로 생성 가능하다.
- JSON id나 evidence id를 그대로 나열하지 않고 사용자가 읽기 쉬운 문장으로 작성한다.

권장 섹션:

```text
[결과 요약]
총 4개 episode 중 성공 1회, 실패 3회가 확인되었습니다.

[주요 근거]
정적 장애물 충돌이 반복되었고 일부 episode에서 제한 시간 초과와 정체가 함께 확인되었습니다.

[판단]
환경 또는 장애물 배치 검토가 필요합니다.

[추천]
review 폴더에 환경 수정 후보가 생성되었습니다.
```

## 12. status.json

```json
{
  "status": "completed",
  "review_id": "0002",
  "run_id": "000005",
  "started_at": "2026-06-20T11:44:14.387040+00:00",
  "completed_at": "2026-06-20T11:44:16.000000+00:00",
  "error": null
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `status` | string | `running`, `completed`, `failed` 등 review lifecycle 상태. |
| `review_id` | string | 생성된 review id. |
| `run_id` | string | 분석 대상 run id. |
| `started_at` | string | 분석 시작 시각. ISO 8601 문자열. |
| `completed_at` | string \| null | 분석 완료 시각. |
| `error` | object \| null | 실패 시 오류 정보. 성공 시 null. |

### 12.1 error Object

```json
{
  "code": "ValueError",
  "message": "분석 중 오류가 발생했습니다: broken payload"
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `code` | string | 예외 class 또는 오류 code. |
| `message` | string | 사용자 표시 가능한 한국어 오류 문장. 원본 오류 detail을 포함할 수 있다. |

## 13. request.json

```json
{
  "project_path": "C:\\Users\\user\\Downloads\\testset_1 (1)",
  "run_id": "000005",
  "prompt": "이 Run의 실패 원인과 정책 수정 또는 환경 수정 필요 여부를 분석해줘",
  "requested_at": "2026-06-20T11:44:14.387040+00:00"
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `project_path` | string | 요청으로 받은 UserProject root 경로. |
| `run_id` | string | 요청으로 받은 run id. |
| `prompt` | string \| null | 요청으로 받은 사용자 prompt. |
| `requested_at` | string | 요청 저장 시각. |

주의: `request.json`에는 `based_on_review_id`를 저장하지 않는다.

## 14. report.json

`report.json`은 분석 근거와 요약을 저장하는 review artifact다.

```json
{
  "summary": {
    "overall_judgement": "change_recommended",
    "message": "환경 또는 장애물 관련 충돌 근거가 확인되어 환경 검토가 필요합니다."
  },
  "metrics": {
    "success_count": 1,
    "failure_count": 3,
    "collision_count": 152,
    "static_obstacle_collision_count": 152,
    "pedestrian_collision_count": 0,
    "near_miss_count": 1
  },
  "data_coverage": {
    "episode_dirs_count": 4,
    "result_file_count": 4,
    "events_file_count": 4,
    "actions_file_count": 4,
    "parsed_actions_file_count": 0,
    "skipped_large_actions_file_count": 4,
    "trace_file_count": 4,
    "summary_json": "present",
    "broken_json_count": 0,
    "broken_json_paths": [],
    "broken_jsonl_line_count": 0,
    "missing_artifact_warnings": []
  },
  "evidence": [],
  "findings": [],
  "patterns": []
}
```

### 14.1 data_coverage

| 필드 | 타입 | 설명 |
|---|---:|---|
| `episode_dirs_count` | integer | `episodes/` 하위 episode directory 수. |
| `result_file_count` | integer | 발견된 `result.json` 수. |
| `events_file_count` | integer | 발견된 `events.jsonl` 수. |
| `actions_file_count` | integer | 발견된 `actions.jsonl` 수. large file skip 여부와 무관한 존재 수. |
| `parsed_actions_file_count` | integer | 실제 파싱한 `actions.jsonl` 수. |
| `skipped_large_actions_file_count` | integer | 크기 제한 등으로 skip한 `actions.jsonl` 수. |
| `trace_file_count` | integer | 발견된 `trace.jsonl` 수. |
| `summary_json` | string | `summary.json` 상태. 예: `present`, `missing`. |
| `broken_json_count` | integer | 파싱 실패 JSON 파일 수. |
| `broken_json_paths` | array[string] | 파싱 실패 JSON 파일 경로 목록. |
| `broken_jsonl_line_count` | integer | 파싱 실패 JSONL line 수. |
| `missing_artifact_warnings` | array[string] | 누락된 result/events 등 artifact 경고. |

### 14.2 evidence[]

```json
{
  "evidence_id": "EV-0001",
  "run_id": "000005",
  "episode_id": "000001",
  "kind": "metric",
  "metric": "static_obstacle_collision_count",
  "value": 90,
  "message": "정적 장애물 충돌이 90회 발생했습니다.",
  "source_file": "runs/000005/episodes/000001/result.json",
  "event_type": null
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `evidence_id` | string | 근거 식별자. 예: `EV-0001`. |
| `run_id` | string | 근거가 속한 run id. |
| `episode_id` | string \| null | 근거가 속한 episode id. |
| `kind` | string | 근거 종류. 예: `metric`, `event`, `summary`. |
| `metric` | string \| null | metric 기반 근거일 때 metric code. |
| `value` | number \| string \| boolean \| null | 근거 값. |
| `message` | string | 사용자 표시용 한국어 근거 문장. |
| `source_file` | string | 근거가 나온 파일 경로. UserProject 기준 상대 경로. |
| `event_type` | string \| null | event 기반 근거일 때 원본 또는 정규화 event type. |

### 14.3 findings[]

```json
{
  "type": "static_obstacle_collision",
  "severity": "medium",
  "title": "정적 장애물 충돌 근거가 확인되었습니다.",
  "summary": "정적 장애물 충돌이 2개의 근거로 확인되었습니다.",
  "evidence_ids": ["EV-0001", "EV-0005"]
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `type` | string | finding code. 내부 code는 영어 유지. |
| `severity` | string | 심각도. 예: `low`, `medium`, `high`. |
| `title` | string | 사용자 표시용 한국어 제목. |
| `summary` | string | 사용자 표시용 한국어 요약. |
| `evidence_ids` | array[string] | 이 finding을 뒷받침하는 evidence id 목록. |

### 14.4 주요 finding type

| type | 설명 |
|---|---|
| `static_obstacle_collision` | 정적 장애물 충돌. |
| `pedestrian_collision` | 보행자 충돌. |
| `blocked_region_collision` | blocked region 충돌 또는 침범. |
| `penalty_region_violation` | penalty region 침범. |
| `near_miss` | 근접 위험. |
| `timeout` | 제한 시간 초과. |
| `stuck` | 로봇 정체. |
| `goal_not_reached` | 목표 미도달. |
| `robot_tip_over` | 로봇 전도. |
| `policy_decision_error` | 정책 판단 오류. |

### 14.5 patterns[]

patterns는 episode 반복 패턴을 나타낸다. 같은 pattern에 같은 `(run_id, episode_id)`가 중복 집계되지 않아야 한다.

```json
{
  "type": "timeout_repeated",
  "count": 3,
  "episode_ids": ["000001", "000003", "000004"],
  "message": "제한 시간 초과가 여러 episode에서 반복되었습니다."
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `type` | string | pattern code. |
| `count` | integer | pattern이 확인된 episode 수 또는 evidence 수. |
| `episode_ids` | array[string] | 관련 episode id 목록. |
| `message` | string | 사용자 표시용 한국어 문장. |

## 15. recommendations.json

```json
{
  "recommendation_type": "environment_review",
  "reason": "환경 또는 장애물 배치와 관련된 실패 근거가 확인되었습니다.",
  "evidence_ids": ["EV-0001", "EV-0005"],
  "recommendations": [],
  "artifacts": {
    "environment": {
      "generated": true,
      "path": "runs/000005/review/0002/scenario.json"
    },
    "policy": {
      "generated": false,
      "path": null
    }
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `recommendation_type` | string | 추천 유형. |
| `reason` | string | 추천 유형 결정 이유. 사용자 표시용 한국어 문장. |
| `evidence_ids` | array[string] | 추천 유형 판단에 사용된 핵심 evidence id 목록. |
| `recommendations` | array | 추천 item 목록. API response의 `recommendations`와 같은 구조. |
| `artifacts` | object | 추천 후보 artifact 생성 여부와 경로. |

## 16. manifest.json

```json
{
  "review_id": "0002",
  "run_id": "000005",
  "based_on_review_id": "0001",
  "generated_files": [
    "runs/000005/review/0002/status.json",
    "runs/000005/review/0002/request.json",
    "runs/000005/review/0002/report.json",
    "runs/000005/review/0002/recommendations.json",
    "runs/000005/review/0002/manifest.json",
    "runs/000005/review/0002/scenario.json"
  ],
  "source_run_files": [],
  "artifacts": {
    "environment": {
      "generated": true,
      "path": "runs/000005/review/0002/scenario.json"
    },
    "policy": {
      "generated": false,
      "path": null
    }
  },
  "comparison": {
    "previous_run_id": "000004",
    "comparison_status": "changed",
    "changed_artifacts": ["scenario.json", "setting.json"],
    "unchanged_artifacts": ["profile.json"],
    "added_artifacts": [],
    "removed_artifacts": []
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `review_id` | string | 현재 review id. |
| `run_id` | string | 분석 대상 run id. |
| `based_on_review_id` | string \| null | 직전 completed review id. 없으면 null. |
| `generated_files` | array[string] | 실제 생성된 review 파일 경로 목록. UserProject 기준 상대 경로. |
| `source_run_files` | array[string] | 분석에 참고한 run 파일 경로 목록. runtime/cache 파일은 제외한다. |
| `artifacts` | object | `recommendations.json.artifacts`와 동일한 구조. |
| `comparison` | object | 이전 run snapshot과 현재 run snapshot의 기본 비교 결과. |

### 16.1 comparison

| 필드 | 타입 | 설명 |
|---|---:|---|
| `previous_run_id` | string \| null | 비교 대상 이전 run id. |
| `comparison_status` | string | 비교 상태. 예: `changed`, `unchanged`, `no_baseline`. |
| `changed_artifacts` | array[string] | 이전 run과 달라진 snapshot artifact 목록. |
| `unchanged_artifacts` | array[string] | 이전 run과 동일한 snapshot artifact 목록. |
| `added_artifacts` | array[string] | 현재 run에서 새로 추가된 artifact 목록. |
| `removed_artifacts` | array[string] | 현재 run에서 사라진 artifact 목록. |

## 17. artifacts Object

`recommendations.json`과 `manifest.json`의 `artifacts`는 동일해야 한다.

```json
{
  "artifacts": {
    "policy": {
      "generated": false,
      "path": null
    },
    "environment": {
      "generated": true,
      "path": "runs/000005/review/0002/scenario.json"
    }
  }
}
```

| 필드 | 타입 | 설명 |
|---|---:|---|
| `policy.generated` | boolean | policy 후보 폴더 생성 여부. |
| `policy.path` | string \| null | 생성된 policy 후보 경로. 없으면 null. |
| `environment.generated` | boolean | scenario 후보 파일 생성 여부. |
| `environment.path` | string \| null | 생성된 scenario 후보 경로. 없으면 null. |

## 18. Policy Candidate Artifact

조건:

- `recommendation_type == "policy_review"`
- `<project_path>/policy/`가 존재

생성 위치:

```text
<project_path>/runs/<run_id>/review/<review_id>/policy/
```

규칙:

- 원본 `<project_path>/policy/`는 수정하지 않는다.
- `__pycache__`, `*.pyc`, `.DS_Store`, symlink는 복사 제외한다.
- 복사본의 Python 정책 파일만 수정한다.
- Python 파일은 `utf-8-sig` 기준으로 읽어 BOM이 있어도 compile 검증이 가능해야 한다.
- 수정 후 가능한 경우 `compile()`로 문법 검증한다.
- 수정 가능한 지점을 찾지 못하면 전체 분석은 실패시키지 않고 warning/reason에 남긴다.

현재 정책 후보 조정:

| 항목 | 상한 |
|---|---:|
| `followSpeedKmh` / `follow_speed_kmh` | `3.5` |
| `maxPathErrorM` / `max_path_error_m` | `0.8` |
| `lookAheadDistanceM` / `look_ahead_distance_m` | `1.0` |
| `pathSmoothingDistanceM` / `path_smoothing_distance_m` | `0.25` |
| `maxSteeringDelta` / `max_steering_delta` | `0.06` |

`configure_from_start()`에서 runtime config가 기본값을 덮어쓸 수 있으므로, 복사본에는 idempotent runtime cap 삽입이 가능하다.

## 19. Environment Candidate Artifact

조건:

- `recommendation_type == "environment_review"`
- `<project_path>/scenario.json`가 존재

생성 위치:

```text
<project_path>/runs/<run_id>/review/<review_id>/scenario.json
```

규칙:

- 원본 `<project_path>/scenario.json`은 수정하지 않는다.
- 추천 후보 원본은 반드시 root `<project_path>/scenario.json`이다.
- `runs/<run_id>/snapshot/scenario.json` 또는 `episodes/<episode_id>/scenario.json`을 후보 원본으로 사용하지 않는다.
- scenario top-level에 private metadata key를 추가하지 않는다.
- BOM이 있는 scenario도 `utf-8-sig`로 읽을 수 있어야 한다.

현재 환경 후보 조정:

| 필드 | 조정 |
|---|---|
| `obstacles.min_clear_width_m` | 숫자면 보수적으로 증가. 예: `0.9 -> 1.2`. |
| `obstacles.placements[].allow_blocking` | 존재하면 `false`로 설정. |
| `corridor.walkway_width_m.min` | 숫자면 안전 폭 방향으로 증가. |
| `corridor.walkway_width_m.max` | 숫자면 안전 폭 방향으로 증가. |
| fallback | 수정 가능한 필드가 없으면 `obstacles.min_clear_width_m`을 안전 기본값으로 추가. |

## 20. Event Type Normalization

`events.jsonl.event_type`은 PascalCase 값을 가질 수 있다. 분석기는 이를 내부 snake_case finding/metric으로 정규화한다.

| Event Type | Normalized Finding |
|---|---|
| `PenaltyRegionViolation` | `penalty_region_violation` |
| `StaticObstacleCollision` | `static_obstacle_collision` |
| `BlockedRegionCollision` | `blocked_region_collision` |
| `PedestrianNearMiss` | `near_miss` |
| `PedestrianCollision` | `pedestrian_collision` |
| `Timeout` | `timeout` |
| `RobotTipOver` | `robot_tip_over` |
| `Stuck` | `stuck` |
| `PolicyDecisionError` | `policy_decision_error` |

## 21. Warnings

warnings는 API response에 포함될 수 있다.

예:

```json
[
  "skipped large file: runs\\000005\\episodes\\000001\\actions.jsonl",
  "missing artifact: runs\\000001\\episodes\\000003\\result.json"
]
```

규칙:

- 요청한 run 범위의 warning만 포함한다.
- large actions skip은 파일 없음처럼 표현하지 않는다.
- broken JSON이 있으면 `broken_json_paths` 또는 warning에 경로를 노출한다.
- warning은 분석 실패가 아닌 비치명 정보를 표현한다.

## 22. analysis_index.json

현재 main 구현과 테스트가 `<project_path>/analysis_index.json` 갱신을 검증하고 있으므로 유지한다.

이 파일은 project-level index이며, 이번 review 저장 계약인 `runs/<run_id>/review/<review_id>/`와 충돌하지 않는다.

## 23. 클라이언트 구현 주의사항

- UI는 `analysis_text`를 우선 표시하고, 상세 탭에서 `report.json`의 findings/evidence를 보여줄 수 있다.
- `analysis_text`는 review JSON에 저장되지 않으므로, 나중에 같은 문구가 필요하면 API response를 별도로 저장하거나 report 기반으로 다시 생성해야 한다.
- `recommendations[].proposed_change.content`는 object여야 한다.
- 공개 API/artifact의 추천 설명 필드는 항상 `recommendation`만 사용한다.
- `artifacts.path`는 UserProject 기준 상대 경로다.
- `policy.generated=true`라고 해서 원본 policy가 바뀐 것은 아니다. 항상 review 하위 복사본만 바뀐다.
- `environment.generated=true`라고 해서 원본 scenario가 바뀐 것은 아니다. 항상 review 하위 scenario 후보만 바뀐다.

## 24. 검증 기준

최소 검증 항목:

| 항목 | 기대 결과 |
|---|---|
| run directory 없음 | review 폴더 생성 없음, `insufficient_data` 응답 |
| run directory 있음 | `review/<review_id>/` 생성 |
| 동일 run 재분석 | 기존 review 덮어쓰기 없음, 다음 review id 생성 |
| policy_review | `review/<id>/policy/` 생성, 원본 policy 불변 |
| environment_review | `review/<id>/scenario.json` 생성, 원본 scenario 불변 |
| none/insufficient_data | policy/scenario 후보 artifact 생성 없음 |
| artifacts | `recommendations.json.artifacts`와 `manifest.json.artifacts` 일치 |
| generated_files | 실제 생성 파일 전체 포함 |
| analysis_text | API response에만 존재, review JSON에는 없음 |
| PascalCase event | finding/metric으로 정규화 |
| BOM JSON/Python | `utf-8-sig`로 처리 가능 |
| large actions | 존재 수와 skip 수를 구분 |
