# v2 Agent API 문서

## 목적

v2 API는 기존 v1 실행 중심 흐름과 별도로, Agent 역할을 명확히 나눈 API입니다.

* `/api/v2/scenarios/generate`: 사용자 자연어 `prompt`를 입력받아 Unreal에서 샘플링 가능한 `scenario.template.json` 형태의 템플릿을 생성합니다.
* `/api/v2/analysis/run`: `experiments` root 하위 파일을 스캔/분류/파싱/집계하여 정책 또는 환경 개선 필요 여부를 판단합니다.

기본 동작은 deterministic/rule-based입니다. `V2_AGENT_LLM_ENABLED=true`일 때만 optional LLM JSON 호출 경로를 사용합니다.

현재 v2 response schema는 MVP 단계의 임시 wrapper이며, 최종 Unreal 연동 규격과 분석 결과 JSON 계약이 확정되면 조정될 수 있습니다.

## v1과 v2 차이

| 구분 | v1 | v2 |
| --- | --- | --- |
| Scenario generation | 기존 RunQueue 생성 흐름 유지 | prompt만 받아 `scenario.template.json` 생성 |
| 실행 개수 | `episode_count` 선택 허용 | 실행 개수 관련 필드 거부 |
| 실행 산출물 | EpisodeSetup / DeliveryBotSetup / RunQueue 생성 | 실행 샘플, seed, RunQueue 생성하지 않음 |
| Analysis | 명시적 입력 파일 경로 기반 분석 | 파라미터 없이 experiments root 전체 분석 |
| LLM | 기존 provider 흐름 유지 | optional LLM mode + deterministic/rule-based fallback |
| 호환성 | 기존 API 계약 유지 | v1 계약을 변경하지 않는 신규 API |

## `scenario.template.json`과 `scenario.json`

`scenario.template.json`은 Agent 또는 시나리오 에디터가 생성/수정하는 원본 템플릿입니다. 범위값, 선택값, 랜덤 요소를 포함할 수 있습니다.

```json
{
  "sidewalk_width_m": {
    "min": 1.0,
    "max": 1.5
  }
}
```

`scenario.json`은 `scenario.template.json`으로부터 샘플링된 실행용 시나리오 샘플입니다. template과 같은 양식을 공유하되, 범위값이 resolve된 확정값으로 바뀐 형태입니다.

```json
{
  "sidewalk_width_m": 1.2
}
```

## `POST /api/v2/scenarios/generate`

### 목적

사용자 자연어 `prompt`를 입력받아 Unreal에서 샘플링 가능한 `scenario.template.json`을 생성합니다.

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

### Response

```json
{
  "schema": "scenario_generate_response_v2",
  "version": 2,
  "status": "success",
  "scenario_id": "narrow_sidewalk_static_obstacle_pedestrian_crossing",
  "summary": "narrow sidewalk / 정적 장애물 / 보행자 횡단 시나리오 템플릿을 생성했습니다.",
  "scenario_template": {
    "schema": "scenario_template",
    "version": 2,
    "scenario_id": "narrow_sidewalk_static_obstacle_pedestrian_crossing",
    "scenario_type": "narrow_sidewalk",
    "intent": {
      "summary": "narrow sidewalk / 정적 장애물 / 보행자 횡단 시나리오 템플릿을 생성했습니다.",
      "risk_factors": [
        "narrow_sidewalk",
        "static_obstacle_ahead",
        "pedestrian_crossing"
      ]
    },
    "ground_model": {
      "default_region_type": "walkable"
    },
    "robot": {
      "type": "delivery_robot",
      "start_area": {},
      "goal_area": {}
    },
    "static_obstacles": {
      "count": {
        "min": 1,
        "max": 2
      }
    },
    "pedestrians": {
      "count": {
        "min": 1,
        "max": 3
      }
    }
  },
  "validation": {
    "valid": true,
    "errors": [],
    "warnings": []
  },
  "assumptions": [
    "좌표 단위는 meter로 가정했습니다.",
    "각도 단위는 degree로 가정했습니다."
  ],
  "generation_mode": "deterministic"
}
```

### `generation_mode`

| 값 | 의미 |
| --- | --- |
| `deterministic` | LLM 비활성 상태에서 deterministic template 생성 경로 사용 |
| `llm` | LLM이 유효한 `scenario_template` JSON을 생성했고 검증 통과 |
| `llm_repaired` | LLM 최초 결과는 실패했지만 repair 결과가 검증 통과 |
| `fallback` | LLM/repair 실패 후 deterministic fallback 사용 |

## `POST /api/v2/analysis/run`

### 목적

`experiments` root 하위의 실험 구성, 시나리오 샘플, 실행 결과, 정책 snapshot을 분석하여 정책 또는 환경 개선 필요 여부를 판단합니다.

### Request

빈 body 또는 body 없음으로 동작합니다.

```json
{}
```

현재 v2에서는 아래 값을 받지 않습니다.

* `experiment_id`
* `run_id`
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
    "runs_count": 2,
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

## fallback 정책

* LLM 호출 실패 시 API 500이 아니라 fallback 응답을 반환합니다.
* LLM JSON 파싱 실패 시 fallback합니다.
* `scenario_template` 검증 실패 시 repair를 시도하고, repair도 실패하면 deterministic fallback합니다.
* analysis recommendation의 evidence가 실제 experiment/run/episode와 맞지 않으면 rule-based fallback합니다.
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

* `scenario_template` schema는 최종 Unreal 계약 확정 전까지 최소 구조 검증만 수행합니다.
* 이미지 파일은 path, size, modified time 같은 metadata만 기록합니다.
* LLM mode는 optional이며, 기본 테스트는 fake/mock client 또는 disabled mode로 외부 API key 없이 통과해야 합니다.
* provider별 attempt log와 representative failed episode timeline 요약은 후속 보강 대상입니다.
