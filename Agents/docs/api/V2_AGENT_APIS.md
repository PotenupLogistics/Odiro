# v2 Agent API 문서

## 목적

v2 API는 기존 v1 실행 중심 흐름과 별도로, Agent 역할을 명확히 나눈 API입니다.

* `/api/v2/scenarios/generate`: 사용자 자연어 `prompt`를 입력받아 `<UserProject>/scenario.json`에 저장 가능한 Project Scenario JSON을 생성합니다.
* `/api/v2/analysis/run`: 사용자 project path와 run id를 입력받아 해당 run 결과를 분석합니다.

Scenario generation v2는 항상 LangGraph runner를 실행합니다. `V2_AGENT_LLM_ENABLED=true`일 때만 LangGraph 내부 LLM-assisted node에서 JSON 호출을 시도하고, 실패하거나 validator를 통과하지 못하면 deterministic graph path로 fallback합니다. `V2_AGENT_GRAPH_ENABLED`는 scenario generation v2의 on/off switch가 아니며, 현재 결과 분석 v2 graph 경로 제어에 사용됩니다.

Scenario generation v2의 외부 response body는 API wrapper가 아니라 `scenario` v1 JSON 객체 자체입니다.
Obstacle placement kind는 Unreal parser와 맞춰 `fixed`, `pattern`, `scatter`를 허용합니다. 기본 LLM 생성은 여전히 단순한 `fixed` placement를 우선 사용합니다.

## v1과 v2 차이

| 구분 | v1 | v2 |
| --- | --- | --- |
| Scenario generation | `410 RUN_QUEUE_REMOVED` 안내 | prompt만 받아 `scenario` v1 JSON 생성 |
| 실행 개수 | `episode_count` 선택 허용 | 실행 개수 관련 필드 거부 |
| 실행 산출물 | 신규 생성 없음 | 실행 샘플, seed, RunQueue 생성하지 않음 |
| Analysis | 명시적 입력 파일 경로 기반 분석 | `project_path`, `run_id` 기반 run 분석 |
| LLM | 기존 provider 흐름 유지 | optional LLM mode + deterministic/rule-based fallback |
| 호환성 | 기존 API 계약 유지 | v1 계약을 변경하지 않는 신규 API |

## Project Scenario와 Episode Scenario

Project Scenario는 Agent 또는 시나리오 에디터가 생성/수정하는 `<UserProject>/scenario.json` 원본입니다. 범위값, 선택값, 랜덤 요소를 포함할 수 있습니다.

```json
{
  "sidewalk_width_m": {
    "min": 1.0,
    "max": 1.5
  }
}
```

Episode Scenario는 run 시점에 snapshot scenario와 seed로 확정되는 실행용 artifact입니다. v2 scenario generation endpoint는 episode scenario를 생성하지 않습니다.

```json
{
  "sidewalk_width_m": 1.2
}
```

## `POST /api/v2/scenarios/generate`

### 목적

사용자 자연어 `prompt`를 입력받아 `scenario` v1 JSON 객체를 생성합니다. 이 endpoint는 `V2_AGENT_GRAPH_ENABLED` 값과 무관하게 LangGraph runner를 사용합니다.

### Request

```json
{
  "prompt": "좁은 보도에서 로봇 전방에 장애물이 있고, 보행자가 로봇 경로를 가로지르는 위험 상황을 만들어줘."
}
```

### 받지 않는 값

v2 scenario generation은 실행 샘플 생성 API가 아니므로 아래 필드를 받지 않습니다.

* `episode_count`
* `count`
* `iterations`
* `run_count`
* `seed`
* `run_queue`
* `experiment_id`
* `run_id`
* `current_template`
* `mode`

### Response

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "pinch_oncoming_pass",
  "intent": "협폭 구간에서 마주 오는 보행자와 조우할 때 로봇이 안전하게 감속, 양보, 통과하는지 검증한다.",
  "corridor": {},
  "obstacles": {},
  "pedestrians": {},
  "robot": {}
}
```

외부 응답 최상위에는 `status`, `summary`, `template`, `validation`, `assumptions`, `generation_mode` wrapper field를 포함하지 않습니다. LangGraph mode와 validation 결과는 내부 graph state에서만 사용합니다.

`scenario` v1은 다음 값을 허용합니다.

* robot anchor `type`: `entry`, `exit`, `corridor_pose`
  * `entry`/`exit`: 추상 anchor이며 `{ "type": "entry" }`, `{ "type": "exit" }`처럼 concrete pose field 없이 사용합니다.
  * `corridor_pose`: concrete anchor이며 `segment`, `along_m`, `offset_m`를 포함해야 합니다.
* 수치 필드: 고정 숫자 또는 `{ "min": ..., "max": ... }`
* `obstacles.placements[].kind`: `fixed`, `pattern`, `scatter`
* `pedestrians.encounters[].overrides`: `cooperation`, `evasiveness`, `personal_space_m`, `awareness_horizon_s`, `max_yield_wait_s`, `sidestep_distance_m`
* `obstacles.placements[].allow_blocking`: optional boolean
* `pedestrians.background.spawn_zone.segments`: optional segment id 목록
* `corridor.segments[].replaced_by`: fixed string 또는 `{ "choices": [...] }`

## `POST /api/v2/analysis/run`

### 목적

사용자 project의 특정 run 결과를 분석하여 정책 또는 환경 개선 필요 여부를 판단합니다. 분석 결과는 기존 response schema로 반환하며, 동시에 review artifact를 사용자 project 내부에 저장합니다.

### Request

```json
{
  "project_path": "X:/Projects/DeliveryBotA",
  "run_id": "000001",
  "prompt": "장애물 때문에 실패했는지 중심으로 다시 분석해줘"
}
```

필수:

* `project_path`: 사용자 project root
* `run_id`: 6자리 decimal string

옵션:

* `prompt`: 사용자 자연어 재분석 요청입니다. 현재는 LLM 호출 없이 rule-based keyword focus만 summary message에 보조 반영하며, metrics나 recommendation을 조작하지 않습니다.

아래 값은 받지 않습니다.

* `experiment_id`
* `path`
* `analysis_option`

### Response

```json
{
  "schema": "analysis_run_response_v2",
  "version": 2,
  "status": "success",
  "analysis_scope": {
    "experiments_count": 1,
    "runs_count": 1,
    "episodes_count": 20
  },
  "summary": {
    "overall_judgement": "change_recommended",
    "message": "반복 실패 패턴이 확인되어 정책 또는 환경 개선 검토가 필요합니다."
  },
  "metrics": {
    "success_count": 12,
    "failure_count": 8,
    "collision_count": 1,
    "near_miss_count": 4,
    "blocked_region_violation_count": 5,
    "penalty_region_violation_count": 0
  },
  "patterns": [],
  "recommendations": [],
  "modified_policy_json": [],
  "modified_environment_json": [],
  "warnings": [],
  "analysis_mode": "rule_based"
}
```

### Review artifact

요청한 run directory가 존재하면 review artifact를 아래 위치에 저장합니다.

```text
<project_path>/runs/<run_id>/review/<review_id>/
```

생성 파일:

* `status.json`
* `request.json`
* `report.json`
* `recommendations.json`
* `manifest.json`

`review_id`는 `0001`, `0002`처럼 증가합니다. API는 `review_id`만 생성하며, `run_id`와 `episode_id`는 request와 run artifact에서 읽은 값을 사용합니다. `<project_path>/runs/<run_id>`가 없으면 기존처럼 `insufficient_data` response를 반환하고 review directory를 만들지 않습니다.

project-level index는 `<project_path>/analysis_index.json`에 갱신합니다.

### `overall_judgement`

| 값 | 의미 |
| --- | --- |
| `change_recommended` | 반복 실패 패턴 또는 개선 필요성이 확인됨 |
| `no_change_needed` | 반복 실패 패턴이 없고 추천할 수정 사항이 없음 |
| `insufficient_data` | 분석 가능한 실행 결과가 부족함 |

### `analysis_mode`

| 값 | 의미 |
| --- | --- |
| `rule_based` | LLM 비활성 상태에서 rule-based 분석/추천 사용 |
| `llm` | LLM 추천이 검증을 통과하여 반영됨 |
| `fallback` | LLM 실패 또는 검증 실패 후 rule-based fallback 사용 |

## LLM mode 설정

```text
V2_AGENT_LLM_ENABLED=false
V2_AGENT_LLM_REPAIR_ENABLED=true
V2_AGENT_LLM_MAX_REPAIR_ATTEMPTS=1
```

* `V2_AGENT_LLM_ENABLED`: 기본값은 `false`입니다. `false`면 deterministic/rule-based 경로를 사용하고, `true`면 optional LLM JSON 호출 경로를 사용합니다.
* `V2_AGENT_LLM_REPAIR_ENABLED`: LLM 생성 결과가 validator를 통과하지 못할 때 repair를 시도할지 결정합니다.
* `V2_AGENT_LLM_MAX_REPAIR_ATTEMPTS`: repair 최대 시도 횟수입니다.

## Graph mode 설정

```text
V2_AGENT_GRAPH_ENABLED=false
```

* 이 설정은 scenario generation v2 endpoint의 graph on/off switch가 아닙니다.
* `/api/v2/scenarios/generate`는 이 값과 무관하게 LangGraph runner를 사용합니다.
* 현재 이 설정은 결과 분석 v2 graph 경로 제어와 legacy rollback 호환을 위해 유지합니다.

## fallback 정책

* LLM 호출 실패 시 API 500이 아니라 fallback 응답을 반환합니다.
* LLM JSON 파싱 실패 시 fallback합니다.
* `scenario` 검증 실패 시 repair를 시도하고, repair도 실패하면 deterministic fallback합니다.
* analysis recommendation의 evidence가 실제 project/run/episode와 맞지 않으면 rule-based fallback합니다.
* fallback이 발생하면 `warnings` 또는 `validation.warnings`에 사유를 남깁니다.

## 추천 없음 규칙

추천할 근거가 없거나 데이터가 부족하면 아래 배열은 항상 빈 배열입니다.

```json
{
  "recommendations": [],
  "modified_policy_json": [],
  "modified_environment_json": []
}
```

## 현재 MVP 한계

* `scenario` schema는 최종 Unreal 계약 확정 전까지 최소 구조 검증만 수행합니다.
* 이미지 파일은 path, size, modified time 같은 metadata만 기록합니다.
* timeline builder와 RAG query builder 결과는 내부 analysis context에 포함되며, 현재 final response schema에는 새 field를 추가하지 않습니다.
* LLM mode는 optional이며, 기본 테스트는 fake/mock client 또는 disabled mode로 외부 API key 없이 통과해야 합니다.
* provider별 attempt log와 representative failed episode timeline 요약은 후속 보강 대상입니다.
