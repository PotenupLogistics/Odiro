# Scenario Sample

경로:

```text
experiments/<Experiment>/scenarios/<SampleId>.json
```

schema:

```json
"scenario_sample"
```

## 역할

Scenario Sample은 Scenario Template을 seed로 확정한 canonical scenario다.
실험 중에는 고정되어야 하며, run은 이 파일을 입력으로 사용한다.

작성/소비 기준:

- LLM이 직접 작성하는 파일이 아니라 생성기가 만든다.
- LLM과 분석 도구의 기본 입력은 `sample`, `scenario`, `validation`이다.
- Unreal 실행용 `ScenarioSetup`/world payload는 이 파일에 저장하지 않는다.
- 실행 payload는 run 또는 preview 시점에 `scenario_sample`에서 파생해 생성한다.

## Root

```json
{
  "schema": "scenario_sample",
  "version": 1,
  "sample": {},
  "scenario": {},
  "validation": {}
}
```

| 필드 | 필수 | 합의 |
| --- | --- | --- |
| `schema` | 필수 | 고정값 `scenario_sample` |
| `version` | 필수 | schema version. v1은 `1` |
| `sample` | 필수 | sample 식별자와 생성 계보 |
| `scenario` | 필수 | seed로 확정된 시나리오 본문 |
| `validation` | 필수 | 생성기 보정, 경고, 수동 override 여부 |

## sample

```json
"sample": {
  "sample_id": "000001",
  "scenario_id": "pinch_oncoming_low_coop_000001",
  "source": {
    "template_ref": "templates/scenarios/pinch_oncoming_low_coop.template.json",
    "template_hash": "sha256:templatehash0001",
    "profile_ref": "experiments/E/profile.json",
    "profile_hash": "sha256:profilehash0001",
    "setting_ref": "experiments/E/setting.json",
    "setting_hash": "sha256:settinghash0001",
    "seed": 3007,
    "generator_version": "0.1.0"
  }
}
```

| 필드 | 합의 |
| --- | --- |
| `sample_id` | experiment 내부 sample 식별자. 파일명과 맞추는 것을 권장 |
| `scenario_id` | 표시/로그 조인용 식별자. template id와 sample id를 조합할 수 있음 |
| `sample.source.template_ref` | 원본 scenario template 경로 |
| `sample.source.template_hash` | 원본 scenario template hash |
| `sample.source.profile_ref` | sample 생성 시 참조한 experiment profile 경로 |
| `sample.source.profile_hash` | experiment profile hash |
| `sample.source.setting_ref` | sample 생성 시 참조한 experiment setting 경로 |
| `sample.source.setting_hash` | experiment setting hash |
| `sample.source.seed` | 이 sample을 만든 seed |
| `sample.source.generator_version` | sample 생성기 버전 |

`sample_id`는 run 결과의 `episodes/<SampleId>/`와 조인되는 핵심 키다.
`sample.source`는 계보 기록이다. 같은 template/profile/setting/seed라도 `generator_version`이 달라지면 결과가 달라질 수 있다.

## scenario

`scenario`는 seed로 확정된 시나리오 본문이다.
`params`는 template 범위값의 확정 결과이고, `semantic`은 LLM과 분석 도구가 읽는 의미론 view다.

```json
"scenario": {
  "params": {
    "corridor.walkway_width_m": 2.7,
    "corridor.curb_side[0].width_m": 0.7,
    "corridor.segments.pinch.min_width_m": 1.25,
    "obstacles.curb_clutter.density_per_10m": 2,
    "pedestrians.background.count": 2,
    "pedestrians.main_conflict.cooperation": 0.31
  },
  "semantic": {
    "route_axis": {},
    "robot": {},
    "layout": [],
    "static_obstacles": [],
    "pedestrians": [],
    "clear_width_profile": [],
    "summary": {}
  }
}
```

| 필드 | 합의 |
| --- | --- |
| `params` | template의 범위값 또는 선택값이 seed로 확정된 값 |
| `semantic` | 사람이 읽고 분석하기 쉬운 상대적 값 |

`params` 기록 기준:

- template에서 범위값이나 선택값이었던 항목만 기록한다.
- key는 가능한 한 template 경로를 유지한다.
- 값은 sample 생성 후 바뀌지 않는 scalar 또는 짧은 배열을 우선한다.

## scenario.semantic

`scenario.semantic`은 원본 좌표 payload가 아니라, 사람이 읽고 분석하기 쉬운 상대적 값을 기록한다.

| 필드 | 합의 |
| --- | --- |
| `route_axis` | along/offset 기준 축 |
| `robot` | 확정된 로봇 시작/목적지 pose |
| `layout` | segment별 lane/surface 구성 |
| `static_obstacles` | 생성된 정적 장애물 의미론 |
| `pedestrians` | 생성된 보행자 의미론 |
| `clear_width_profile` | 진행 구간별 유효 통로 폭 |
| `summary` | sample 형상 요약 |

### route_axis

| 필드 | 합의 |
| --- | --- |
| `type` | v1은 `polyline` |
| `origin_xy_m` | template/world 변환 기준점 |
| `heading_deg` | 기준 heading |
| `points_m` | corridor axis의 polyline points |
| `length_m` | axis 전체 길이 |

`along_m`은 이 축을 따라 진행한 거리이고, `offset_m`은 축 좌우 오프셋이다.
event, obstacle, pedestrian은 가능하면 이 좌표계로 조인 가능해야 한다.
`events.jsonl`은 이 좌표계를 `properties.robot_along_m`, `properties.robot_offset_m`, `properties.target_along_m`, `properties.target_offset_m`으로 참조한다.

### robot

| 필드 | 합의 |
| --- | --- |
| `start.segment` | 확정된 시작 segment |
| `start.along_m`, `start.offset_m` | corridor 기준 시작 위치 |
| `start.lane` | 시작 lane hint |
| `start.heading_deg` | 실행 payload로 변환할 시작 yaw |
| `start.source_anchor_type` | template의 원래 anchor type |
| `goal.segment` | 확정된 목적지 segment |
| `goal.along_m`, `goal.offset_m` | corridor 기준 목적지 위치 |
| `goal.lane` | 목적지 lane hint |
| `goal.source_anchor_type` | template의 원래 anchor type |

### layout

`layout`은 이 sample의 지면과 lane 구성을 읽는 곳이다.

| 필드 | 합의 |
| --- | --- |
| `along_range_m` | 이 구성이 유지되는 진행 구간 |
| `segment` | template의 segment id |
| `lanes[].lane` | lane 이름 |
| `lanes[].offset_range_m` | axis 기준 좌우 범위 |
| `lanes[].surface` | surface 이름 |
| `lanes[].type` | `walkable`, `penalty`, `blocked` |

### static_obstacles

| 필드 | 합의 |
| --- | --- |
| `id` | 생성된 obstacle instance id. `events.jsonl.properties.target_id`와 조인 |
| `prop` | 생성된 prop id |
| `perception_tag` | 감지/로그 조인을 위한 tag |
| `class` | `blocking` 또는 `traversable_cost` |
| `sensor_profile` | `solid`, `thin`, `low_profile` 등 감지 특성 주석 |
| `along_m`, `offset_m` | corridor 기준 위치 |
| `yaw_deg` | obstacle yaw |
| `footprint_m` | 분석용 footprint |
| `placed_by` | 이 인스턴스를 만든 template placement id |
| `clear_width_remaining_m` | 이 장애물 지점에서 남은 유효 통로 폭 |

### pedestrians

| 필드 | 합의 |
| --- | --- |
| `id` | 생성된 pedestrian instance id |
| `role` | `encounter` 또는 `background` |
| `placed_by` | template encounter id. background면 생략 가능 |
| `type` | encounter type |
| `persona` | template의 persona 이름 |
| `behavior` | persona와 overrides가 전개된 행동 벡터 |
| `speed_mps` | 확정된 보행자 속도 |
| `baseline` | 로봇이 없었다면 보행자가 걸었을 기준 동선 요약 |
| `pedestrian_scenario_hash` | plan+behavior 지문 |

`behavior`는 실행 payload와 분석이 공유하는 확정값이다.
LLM은 persona 이름보다 `behavior`와 `baseline`을 우선 읽어야 한다.

### clear_width_profile / summary

| 필드 | 합의 |
| --- | --- |
| `clear_width_profile[].along_range_m` | 통로 폭을 요약하는 진행 구간 |
| `clear_width_profile[].clear_width_m` | 해당 구간의 유효 통로 폭 |
| `clear_width_profile[].limited_by` | 통로 폭을 제한한 obstacle/region id |
| `summary.global_min_clear_width_m` | sample 전체 최소 유효 통로 폭 |
| `summary.min_clear_at_along_m` | 최소 통로 폭이 발생한 위치 |
| `summary.total_length_m` | route axis 전체 길이 |
| `summary.encounter_in_min_clear_zone` | 주요 encounter가 최소 통로 폭 구간과 겹치는지 |


합의:

- 재현성의 기준은 sample 파일의 `sample.source`와 `scenario.params`/`scenario.semantic`이다.
- 컴파일된 world payload를 저장해야 할 필요가 생기면 별도 derived artifact로 다룬다.

## validation

```json
"validation": {
  "edited_by_user": false,
  "diagnostics": []
}
```

| 필드 | 합의 |
| --- | --- |
| `edited_by_user` | 생성 후 sample이 직접 수정되었는지 |
| `diagnostics` | 생성기 repair/warning/error 기록 |

분석 기준:

- `edited_by_user: true`인 sample은 순수 template+seed 생성물과 구분한다.
- repair가 있었던 sample은 `scenario.params`와 `scenario.semantic`을 읽을 때 보정 내용을 함께 확인한다.
- error가 있으면 run 대상에서 제외하는 것을 기본으로 한다.
