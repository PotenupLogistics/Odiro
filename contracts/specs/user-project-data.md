# 사용자 프로젝트 데이터 계약

- 기준: 사용자 project root의 공유 파일 형식
- 대상: Bridge, Client, Agents가 함께 읽고 쓰는 파일
- 제외: Bridge IPC 요청/응답 형식. 해당 계약은 `bridge-ipc.md`가 소유
- 제외: 제거된 `Client/Docs/Data/**`
- 최종 기준: 새 writer/API/schema는 이 문서를 우선

## 공통 규칙

- JSON file은 root object
- 알 수 없는 field 허용 여부는 각 사용 주체가 자기 경계에서 결정
- Bridge: project/run 경계에서 최상위 계약과 경로 안전성 검증
- Simulator: snapshot을 읽고 실행에 필요한 값 검증
- Agents: API 입력과 분석 대상 run 검증

## Project 입력 파일

### `setting.json`

경로:

```text
<UserProject>/setting.json
runs/<RunId>/snapshot/setting.json
```

schema:

```json
"project_setting"
```

필수 root:

| Field        | Type   | 의미                           |
| ------------ | ------ | ------------------------------ |
| `schema`     | string | 고정값 `project_setting`       |
| `version`    | number | 고정값 `1`                     |
| `project_id` | string | project 식별자                 |
| `sampling`   | object | `scenario_sample` 생성 조건     |
| `runtime`    | object | simulator 실행 조건            |
| `evaluation` | object | episode result/evaluation 기준 |

`sampling` 최소 field:

| Field               | Type   | 의미                         |
| ------------------- | ------ | ---------------------------- |
| `base_seed`         | number | episode seed 기준값          |
| `episode_count`     | number | 한 run에서 생성할 episode 수 |
| `generator_version` | string | `scenario_sample` 생성기 버전 |

`runtime` 최소 field:

| Field            | Type   | 의미                                    |
| ---------------- | ------ | --------------------------------------- |
| `map_id`         | string | simulator가 열 map 이름 또는 package id |
| `fixed_fps`      | number | project 실행 FPS                        |
| `time_scale`     | number | 필요 시 실행 시간 배율                  |
| `max_duration_s` | number | episode timeout. `0`이면 timeout 없음   |

`evaluation` 최소 field:

| Field                      | Type   | 의미                  |
| -------------------------- | ------ | --------------------- |
| `goal_acceptance_radius_m` | number | 목표 도달 판정 반경   |
| `tip_over_angle_deg`       | number | 전복 판정 각도        |
| `near_miss_distance_m`     | number | 보행자 near-miss 거리 |

Seed:

```text
episode_seed = sampling.base_seed + episode_index
```

- `episode_index`: 0-based
- `EpisodeId`: 1-based 6자리 decimal string
- random range/choices: `scenario.json` 소유
- seed, episode 수: `setting.sampling` 소유
- 생성기 버전 기록 위치: `scenario_sample.sample.source.generator_version`

### `profile.json`

경로:

```text
<UserProject>/profile.json
runs/<RunId>/snapshot/profile.json
```

schema:

```json
"simulation_profile"
```

필수 root:

| Field          | Type   | 의미                            |
| -------------- | ------ | ------------------------------- |
| `schema`       | string | 고정값 `simulation_profile`     |
| `version`      | number | 고정값 `1`                      |
| `profile_id`   | string | profile 식별자                  |
| `display_name` | string | UI 표시명                       |
| `description`  | string | 설명                            |
| `robot`        | object | robot capability/setup snapshot |

`robot` 최소 section:

| Field   | Type   | 의미                                     |
| ------- | ------ | ---------------------------------------- |
| `body`  | object | 길이, 폭, 높이, wheel base 등 물리 크기  |
| `drive` | object | 속도, 가속, 조향, engine/drive 설정      |
| `lidar` | object | LiDAR mode, range, angle step, height 등 |

규칙:

- Profile은 project 고정 입력
- Profile 값이 달라지면 별도 project로 간주
- 거리와 크기는 meter 단위
- robot policy 코드/config는 `<UserProject>/policy/` 소유
- surface, prop, pedestrian catalog는 profile에 넣지 않음

`robot.body` 최소 field:

| Field              | Type   | 의미            |
| ------------------ | ------ | --------------- |
| `length_m`         | number | robot 전체 길이 |
| `width_m`          | number | robot 전체 폭   |
| `height_m`         | number | robot 전체 높이 |
| `wheel_base_m`     | number | wheel base      |
| `turning_radius_m` | number | 최소 회전 반경  |

`robot.drive` 최소 field:

| Field                     | Type    | 의미                              |
| ------------------------- | ------- | --------------------------------- |
| `max_speed_kmh`           | number  | 전진 최대 속도                    |
| `max_reverse_kmh`         | number  | 후진 최대 속도                    |
| `accel_kmh_per_s`         | number  | 전진 가속 기준값                  |
| `decel_kmh_per_s`         | number  | 감속 기준값                       |
| `reverse_accel_kmh_per_s` | number  | 후진 가속 기준값                  |
| `steering_rate_per_s`     | number  | 조향 입력 변화율                  |
| `throttle_rate_per_s`     | number  | throttle 입력 변화율              |
| `brake_rate_per_s`        | number  | brake 입력 변화율                 |
| `stop_brake`              | number  | 정지 시 brake 입력                |
| `gear_switch_stop_kmh`    | number  | 기어 전환을 허용할 정지 판정 속도 |
| `gear_switch_brake`       | number  | 기어 전환 시 brake 입력           |
| `slowdown_range_kmh`      | number  | 감속 제어 범위                    |
| `speed_tolerance_kmh`     | number  | 목표 속도 허용 오차               |
| `speed_limit_brake`       | number  | 속도 제한 초과 시 brake 입력      |
| `use_handbrake`           | boolean | handbrake 사용 여부               |
| `max_torque`              | number  | engine torque 설정                |
| `max_rpm`                 | number  | engine max rpm                    |
| `idle_rpm`                | number  | engine idle rpm                   |
| `engine_brake`            | number  | engine brake 설정                 |
| `rev_up_moi`              | number  | engine rev up moment of inertia   |
| `rev_down_rate`           | number  | engine rev down rate              |

추가 규칙:

- `robot.drive.physics`: v1 정식 field 아님
- Python policy `/scenario/start`의 `robotSpec.bodyLengthCm`, `bodyWidthCm`, `bodyHeightCm`, `wheelBaseCm`, `turningRadiusCm`: `robot.body` meter 값에서 파생

`robot.lidar` 최소 field:

| Field               | Type    | 의미                     |
| ------------------- | ------- | ------------------------ |
| `mode`              | string  | LiDAR mode               |
| `range_m`           | number  | LiDAR 감지 거리          |
| `angle_step_degree` | number  | ray 간 yaw 간격          |
| `height_m`          | number  | ray 시작 높이            |
| `store_missed_rays` | boolean | miss ray도 저장할지 여부 |

생성된 `scenario_sample` 참조:

- `sample.source.profile_ref`: run snapshot의 profile 경로
- `sample.source.profile_hash`: run snapshot profile hash

### `scenario.json`

경로:

```text
<UserProject>/scenario.json
runs/<RunId>/snapshot/scenario.json
```

schema:

```json
"scenario"
```

필수 root:

| Field         | Type   | 의미                              |
| ------------- | ------ | --------------------------------- |
| `schema`      | string | 고정값 `scenario`                 |
| `version`     | number | 고정값 `1`                        |
| `scenario_id` | string | scenario 식별자                   |
| `intent`      | string | 검증하려는 상황/가설              |
| `corridor`    | object | 공간 skeleton과 lane/surface 구성 |
| `obstacles`   | object | 정적 장애물 배치 규칙             |
| `pedestrians` | object | 보행자 수와 encounter 규칙        |
| `robot`       | object | 로봇 시작/목적지 anchor           |

규칙:

- 한 project에는 편집 가능한 `scenario.json` 하나만 둠
- 고정값과 random range/choices를 같은 file에 기록
- seed, episode 수, generator version은 `setting.json`의 `sampling` 소유
- run 시작 시 `runs/<RunId>/snapshot/scenario.json`으로 복사
- episode 실행 전 snapshot scenario와 seed로 `episodes/<EpisodeId>/scenario.json` 확정
- Unreal 실행용 actor/world payload는 저장하지 않음
- Unreal 실행용 payload는 preview/run 시점에 파생

Random 값 표현:

```json
"walkway_width_m": 3.0
```

```json
"walkway_width_m": { "min": 2.5, "max": 4.0 }
```

```json
"replaced_by": { "choices": ["grass", "road"] }
```

`corridor` 최소 field:

| Field             | Type   | 의미                                           |
| ----------------- | ------ | ---------------------------------------------- |
| `axis`            | object | `type: "polyline"`과 `points_m` 경로 점 목록   |
| `walkway_width_m` | number | 주 보행로 폭. number 또는 `{min,max}`          |
| `building_side`   | array  | 보행로 건물 측 lane/surface 폭 규칙            |
| `curb_side`       | array  | 보행로 연석 측 lane/surface 폭 규칙            |
| `segments`        | array  | axis 상의 의미 segment 목록                    |

`corridor.segments[]` 최소 field:

| Field           | Type   | 의미                                           |
| --------------- | ------ | ---------------------------------------------- |
| `id`            | string | obstacle, pedestrian, robot anchor 참조 id      |
| `type`          | string | `straight`, `narrowing`, `crosswalk`, `entrance` |
| `along_range_m` | array  | `[start_m, end_m]`                             |

규칙:

- `corridor.axis.points_m`은 최소 두 점
- `corridor.segments[].type`을 사용
- `walkway_width_m`은 `corridor` 바로 아래에 둠

`robot.start`, `robot.goal` anchor:

| Field      | Type   | 의미                                           |
| ---------- | ------ | ---------------------------------------------- |
| `type`     | string | `entry`, `exit`, `corridor_pose`               |
| `segment`  | string | `corridor_pose`일 때 참조 segment id           |
| `along_m`  | number | `corridor_pose`일 때 segment 상 거리           |
| `offset_m` | number | `corridor_pose`일 때 axis 기준 lateral offset  |
| `heading`  | string | 선택값. `forward`, `backward`, `auto`          |

생성 규칙:

- range/choices는 `scenario_sample` 생성 시 seed로 확정
- 확정 결과는 `episodes/<EpisodeId>/scenario.json`의 `scenario.params`, `scenario.semantic`에 기록

검증 규칙:

- Scenario 저장 시 검증
- `scenario_sample` 생성 직후 검증
- `error`: 저장 또는 episode 생성 중단
- `warning`: 생성 계속, `validation.diagnostics`에 기록
- `repair`: 보정 후 생성 계속, 보정 사실을 `validation.diagnostics`에 기록
- `segments[].id`, `placements[].id`, `encounters[].id`: 각각 unique
- placement, encounter, robot 참조 segment: 존재 필수
- `corridor_pose.along_m`: 참조 segment의 `along_range_m` 범위 안
- `allow_blocking` 없이 `min_clear_width_m` 계약 위반 시 error

## 환경 어휘 목록

`scenario.json` 작성용 공유 어휘.

사용 위치:

- Project Scenario의 `corridor`
- Project Scenario의 `obstacles`
- Project Scenario의 `pedestrians`

기본 규칙:

- surface, prop, persona, encounter type은 이 문서의 어휘만 사용
- 모르는 이름 임의 생성 금지
- catalog 정의를 scenario 안에 복사하지 않음
- scenario에는 배치/생성할 내용만 기록
- 거리와 크기는 meter 단위

Scenario에서 쓰는 위치:

| Scenario field                              | 사용하는 어휘  |
| ------------------------------------------- | -------------- |
| `corridor.building_side[].surface`          | surface id     |
| `corridor.curb_side[].surface`              | surface id     |
| `corridor.segments[].replaced_by`           | surface id     |
| `obstacles.placements[].prop`               | prop id        |
| `obstacles.placements[].palette.categories` | prop category  |
| `obstacles.placements[].palette.classes`    | prop class     |
| `pedestrians.encounters[].type`             | encounter type |
| `pedestrians.encounters[].persona`          | persona id     |
| `pedestrians.encounters[].overrides`        | behavior field |

### Surface

| surface_id         | 로봇 기준 | 의미                           | 주 사용처                           |
| ------------------ | --------- | ------------------------------ | ----------------------------------- |
| `sidewalk`         | walkable  | 기본 보행로                    | 주 통로                             |
| `crosswalk_stripe` | walkable  | 횡단보도 표시/보행 가능 stripe | `crosswalk` segment                 |
| `grass`            | penalty   | 잔디/화단                      | 통로를 좁히거나 회피 비용을 만들 때 |
| `road`             | penalty   | 차도                           | 보도-차도 경계, 횡단 상황           |
| `driveway`         | penalty   | 진출입로                       | 건물/연석 쪽 출입 구간              |
| `wall`             | blocked   | 벽/물리적 경계                 | 건물측 경계                         |
| `building`         | blocked   | 건물 영역                      | 통과 불가 영역                      |

선택 기준:

- 보행 가능한 주 통로: `sidewalk`
- 통로 폭을 줄이는 녹지/화단: `grass`
- 차도/진출입로 위험 영역: `road`, `driveway`
- 통과 불가능한 건물 경계: `wall`, `building`
- 보행 가능한 횡단보도 표시: `crosswalk_stripe`

### Prop

`prop`는 정적 장애물/소품의 의미 id.

사용 방식:

- 직접 world 좌표에 배치하지 않음
- `fixed`, `pattern`, `scatter` 배치 규칙에서 사용

Prop category:

| category           | 의미                                | 예시 상황             |
| ------------------ | ----------------------------------- | --------------------- |
| `street_furniture` | 벤치, 볼라드, 표지판 등 거리 시설물 | 보도 가장자리 clutter |
| `traffic_control`  | 콘, 바리케이드 등 교통 통제물       | 임시 협폭, gate       |
| `delivery_item`    | 박스, 적재물, 배달 관련 물체        | 보도 위 임시 장애물   |
| `utility`          | 전봇대, 설비함 등 시설물            | 고정 장애물           |
| `surface_object`   | 낮은 턱, 덮개 등 지면 위 물체       | 낮은 profile 장애물   |

Prop class:

| class              | 의미                                      |
| ------------------ | ----------------------------------------- |
| `blocking`         | 로봇이 통과할 수 없는 장애물              |
| `traversable_cost` | 통과는 가능하지만 비용/주의가 필요한 물체 |

Sensor profile:

| sensor_profile | 의미                                          |
| -------------- | --------------------------------------------- |
| `solid`        | 넓고 안정적으로 감지되는 물체                 |
| `thin`         | 콘/기둥처럼 얇아 일부 ray만 맞을 수 있는 물체 |
| `low_profile`  | 낮아서 감지/회피가 어려울 수 있는 물체        |

배치 방식:

- 특정 장애물 하나: `fixed`
- 통로 일부를 막는 문/게이트: `pattern`
- 보도 가장자리 clutter: `scatter`
- `scatter` 후보 제한: `palette.categories`, `palette.classes`
- 장애물의 구체 표현 방식은 scenario에 쓰지 않음

### Pedestrian

`pedestrians` 작성 범위:

- 보행자 path 목록 직접 작성 아님
- 배경 보행자 규모 기록
- 특정 encounter를 의미론적으로 기록

Persona:

| persona_id   | 의미                                    | 쓰기 좋은 상황                     |
| ------------ | --------------------------------------- | ---------------------------------- |
| `passive`    | 잘 비켜주는 보행자                      | 로봇이 무난히 통과 가능한 baseline |
| `normal`     | 자기 경로를 유지하되 적당히 양보        | 일반 보행자 흐름                   |
| `assertive`  | 잘 비켜주지 않는 보행자                 | 협폭/대향 통과 스트레스 테스트     |
| `vulnerable` | 저속, 큰 personal space, 낮은 회피 성향 | 조심스럽게 접근해야 하는 보행자    |

Behavior override:

| Field                 | 의미                                              |
| --------------------- | ------------------------------------------------- |
| `cooperation`         | 로봇에게 양보하는 성향. `0`에 가까울수록 비협조적 |
| `evasiveness`         | 옆으로 피하려는 성향                              |
| `personal_space_m`    | 유지하려는 개인 공간                              |
| `awareness_horizon_s` | 충돌/접근을 예측하는 시간 범위                    |
| `max_yield_wait_s`    | 양보하며 기다릴 수 있는 최대 시간                 |
| `sidestep_distance_m` | 옆으로 비켜서는 거리                              |

Encounter type:

| type             | 의미                             | 주요 field                                     |
| ---------------- | -------------------------------- | ---------------------------------------------- |
| `oncoming_pass`  | 대향 보행자와 통과/양보 판단     | `at`, `meet_offset_m`, `persona`, `overrides`  |
| `overtake`       | 뒤에서 추월하는 보행자 반응      | `at`, `speed_mps`, `persona`                   |
| `cross_path`     | 전방 끼어듦/횡단 반응            | `at`, `trigger_distance_m`, `from`, `persona`  |
| `standing_group` | 정지 군집 앞 통과/대기/우회 판단 | `at`, `size`, `blocked_width_ratio`, `persona` |

선택 기준:

- 좁은 통로에서 양보 판단 확인: `oncoming_pass`, `assertive`
- 갑작스러운 전방 진입: `cross_path`
- 보행자가 로봇 뒤에서 접근: `overtake`
- 통로 일부를 점유하는 정지 인파: `standing_group`
- `background.persona_mix` 같은 거시 분포 값은 v1에서 사용하지 않음

저장 전 확인:

- `surface`: 이 문서의 surface 어휘
- `prop`: catalog에 존재하는 prop id
- `palette.categories`: 정의된 prop category
- `palette.classes`: 정의된 prop class
- `persona`: 정의된 persona id
- `encounters[].type`: 정의된 encounter type
- scenario 본문: catalog 정의 자체 복사 금지
- pedestrian 설정: 과도한 거시 분포 값 금지

## Run 결과 파일

### Scenario Sample

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/scenario.json
```

schema:

```json
"scenario_sample"
```

필수 root:

| Field        | Type   | 의미                                        |
| ------------ | ------ | ------------------------------------------- |
| `schema`     | string | 고정값 `scenario_sample`                    |
| `version`    | number | 고정값 `1`                                  |
| `sample`     | object | sample id, scenario id, source 입력과 seed  |
| `scenario`   | object | seed로 확정한 `params`와 실행용 `semantic`  |
| `validation` | object | 생성 진단                                   |

`sample.source` 최소 field:

| Field               | Type   | 의미                         |
| ------------------- | ------ | ---------------------------- |
| `template_ref`      | string | run snapshot scenario 경로   |
| `template_hash`     | string | run snapshot scenario hash   |
| `profile_ref`       | string | run snapshot profile 경로    |
| `profile_hash`      | string | run snapshot profile hash    |
| `setting_ref`       | string | run snapshot setting 경로    |
| `setting_hash`      | string | run snapshot setting hash    |
| `seed`              | number | episode seed                 |
| `generator_version` | string | sampler 구현/설정 버전       |

규칙:

- 실행 입력과 재현성 파일
- 사용자가 직접 편집하지 않음

### Run Status

경로:

```text
runs/<RunId>/status.json
```

schema:

```json
"run_status"
```

소유자:

```text
Bridge
```

필수 root:

| Field        | Type   | 의미                                                    |
| ------------ | ------ | ------------------------------------------------------- |
| `schema`     | string | 고정값 `run_status`                                     |
| `version`    | number | 고정값 `1`                                              |
| `run`        | object | project path, run id, status path                       |
| `process`    | object | simulator 실행 파일, process id, policy port, exit code |
| `state`      | string | process 생명주기 상태                                   |
| `started_at` | string | UTC timestamp                                           |
| `updated_at` | string | UTC timestamp                                           |
| `exited_at`  | string | 종료 상태 timestamp. 종료 상태에서 기록                 |
| `error`      | string | `failed` 상태의 사람이 읽는 오류. 없으면 생략           |

`state` 값:

| 값         | 의미                                      |
| ---------- | ----------------------------------------- |
| `starting` | Bridge가 process 시작 전 status file 생성 |
| `running`  | 자식 process 시작 성공                    |
| `stopping` | Bridge가 종료 요청 전송                   |
| `exited`   | 자식 process가 exit code 0으로 종료       |
| `failed`   | process 시작 또는 실행 실패               |

### Run Summary

경로:

```text
runs/<RunId>/summary.json
```

schema:

```json
"run_summary"
```

필수 root:

| Field     | Type   | 의미                      |
| --------- | ------ | ------------------------- |
| `schema`  | string | 고정값 `run_summary`      |
| `version` | number | 고정값 `1`                |
| `run`     | object | run metadata              |
| `rows`    | array  | episode별 result 요약 row |

`run` 최소 field:

| Field                  | Type   | 의미                        |
| ---------------------- | ------ | --------------------------- |
| `run_id`               | string | 6자리 decimal string        |
| `project_id`           | string | project 식별자              |
| `started_at`           | string | 실행 시작 시각              |
| `ended_at`             | string | 실행 종료 시각              |
| `policy_snapshot_hash` | string | opaque policy snapshot hash |

`rows[]` 최소 field:

| Field                   | Type    | 의미                                    |
| ----------------------- | ------- | --------------------------------------- |
| `episode_id`            | string  | 6자리 decimal string                    |
| `scenario_id`           | string  | scenario 표시 식별자                    |
| `scenario_hash`         | string  | `scenario_sample` hash                  |
| `scenario_source_hash`  | string  | snapshot scenario hash                  |
| `profile_hash`          | string  | snapshot profile hash                   |
| `setting_hash`          | string  | snapshot setting hash                   |
| `seed`                  | number  | episode seed                            |
| `outcome`               | string  | `Success`, `Failure`, `Cancelled` 등    |
| `terminal_reason`       | string  | episode 종료 원인                       |
| `duration_s`            | number  | episode 실행 시간                       |
| `usable_for_llm_tuning` | boolean | 분석/튜닝 근거 사용 가능 여부           |
| `metrics`               | object  | 주요 count/distance metric subset       |
| `scenario_params`       | object  | 핵심 `scenario_sample.scenario.params` subset   |
| `scenario_semantic`     | object  | 핵심 `scenario_sample.scenario.semantic` subset |

규칙:

- 빠른 분석/필터링용 집계 파일
- 원본 아님
- 원본: episode `scenario.json`, `result.json`, `events.jsonl`

### Episode Result

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/result.json
```

schema:

```json
"episode_result"
```

필수 root:

| Field           | Type   | 의미                         |
| --------------- | ------ | ---------------------------- |
| `schema`        | string | 고정값 `episode_result`      |
| `version`       | number | 고정값 `1`                   |
| `episode`       | object | episode id, hashes, seed     |
| `run`           | object | run id, policy snapshot hash |
| `summary`       | object | 최종 결과 기준               |
| `metrics`       | object | evaluation metrics           |
| `event_summary` | object | `events.jsonl` 집계          |

규칙:

- `summary.terminal_reason`: episode 종료 원인 기준
- 종료 원인 event는 `events.jsonl`에 기록
- 가능하면 `summary.terminal_event_index`로 종료 원인 event 참조

### Robot Actions

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/actions.jsonl
```

line schema:

```json
"robot_action"
```

필수 line root:

| Field                     | Type           | 의미                                    |
| ------------------------- | -------------- | --------------------------------------- |
| `schema`                  | string         | 고정값 `robot_action`                   |
| `version`                 | number         | 고정값 `1`                              |
| `sequence`                | number         | episode 내 policy 판단 순번             |
| `run_time_seconds`        | number         | episode 실행 시간. 단위 s               |
| `status`                  | string         | `ok` 또는 `error`                       |
| `front_half_angle_degree` | number         | 전방 관측 영역 half angle               |
| `lidar_rays`              | array          | 해당 decide 시점의 LiDAR ray 목록       |
| `observed_objects`        | array          | LiDAR ray를 actor 단위로 묶은 관측 요약 |
| `robot_state`             | object         | decide 시점의 robot state               |
| `action`                  | object or null | policy 반환 action. 실패 시 `null`      |
| `error`                   | object         | `status: "error"`일 때 실패 정보        |
| `path`                    | object         | decide 시점의 path 추적 상태            |

규칙:

- `sequence`: `events.jsonl.action_sequence`와 action/event 조인 key
- policy 호출 실패도 같은 `sequence`로 error line 기록

### Episode Events

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/events.jsonl
```

line schema:

```json
"episode_event"
```

필수 line root:

| Field              | Type           | 의미                                                |
| ------------------ | -------------- | --------------------------------------------------- |
| `schema`           | string         | 고정값 `episode_event`                              |
| `version`          | number         | 고정값 `1`                                          |
| `event_index`      | number         | episode 내 event 순번                               |
| `run_time_seconds` | number         | episode 실행 시간. 단위 s                           |
| `source`           | string         | event 생성 주체                                     |
| `event_type`       | string         | event 종류                                          |
| `reason`           | string         | 기계가 읽기 쉬운 원인 코드                          |
| `message`          | string         | 사람이 읽는 짧은 설명                               |
| `action_sequence`  | number or null | 이전 action sequence. 연결할 action이 없으면 `null` |
| `properties`       | object         | event별 상세 요약                                   |

기본 `source` 값:

| 값                    | 의미                             |
| --------------------- | -------------------------------- |
| `EvaluationSubsystem` | Unreal evaluation system event   |
| `PythonPolicy`        | Python policy/pathfinder event   |
| `PolicyRuntime`       | policy 호출/통신/응답 처리 event |

규칙:

- 종료 원인 사건은 반드시 기록
- action 원본을 복사하지 않음
- action 원본은 `action_sequence`로 `actions.jsonl` 참조

### Episode Trace

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/trace.jsonl
```

line schema:

```json
"episode_trace"
```

필수 line root:

| Field              | Type   | 의미                              |
| ------------------ | ------ | --------------------------------- |
| `schema`           | string | 고정값 `episode_trace`            |
| `version`          | number | 고정값 `1`                        |
| `sample_index`     | number | 0-based trace sample index        |
| `run_time_seconds` | number | episode 실행 timestamp            |
| `delta_seconds`    | number | previous sample과의 frame delta   |
| `robot`            | object | robot ground-truth state          |
| `actors`           | array  | robot 외 actor ground-truth state |

규칙:

- robot이 관측하거나 결정한 정보가 아님
- replay, debugging, run 이후 분석용 환경 상태 기록
- `actions.jsonl`, `events.jsonl`과의 기본 join key는 `run_time_seconds`

### Episode Preview

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/preview.png
```

형식:

```text
PNG 파일
```

규칙:

- episode 대표 이미지
- 실패, near-miss, 충돌 등 대표 event 장면 우선
- 해상도, event 참조, 대체 frame 규칙은 추후 확정

### Sensor Captures

경로:

```text
runs/<RunId>/episodes/<EpisodeId>/captures/
```

형식:

```text
image/data 파일
```

규칙:

- episode 중 저장한 센서 데이터 파일
- `actions.jsonl`, `trace.jsonl`, `events.jsonl`에서 필요한 capture 참조 가능
- capture 목록 파일, 파일명 규칙, 폴더 구조는 추후 확정

### AI 분석

경로:

```text
runs/<RunId>/review/
runs/<RunId>/review/analysis_run_response_v2.json
```

schema:

```json
"analysis_run_response_v2"
```

규칙:

- AI 분석 결과물
- `/api/v2/analysis/run` 응답과 같은 JSON을 `analysis_run_response_v2.json`에 저장
- 분석 근거는 `project_id`, `run_id`, `episode_id` 증거로 추적 가능
- 근거 대상: `summary.json`, `result.json`, `events.jsonl`, `scenario_sample`
- 추가 report/finding schema와 prompt 기록 방식은 추후 확정

## 행동 정책 패키지

경로:

```text
<UserProject>/policy/
runs/<RunId>/snapshot/policy/
```

필수 진입점:

```text
policy/__init__.py:create_policy
```

진입점 규칙:

- `create_policy()`는 인자 없이 호출 가능해야 함
- 반환값은 `Client/Resources/policy-runtime.py`가 호출하는 policy object
- runtime은 policy object의 `start`, `decide`, `end`를 JSON 직렬화 가능한 dict 입출력으로 호출한다.

snapshot 복사 규칙:

- `policy/` 전체 copy
- symlink 금지
- `__pycache__`, `.pyc`, `.pyo` 제외
- `Client/Resources/policy-runtime.py`는 project policy package나 project template에 포함하지 않음

## Project template

개발 위치:

```text
static/project-templates/<TemplateId>/
```

배포 위치:

```text
build/Release/resources/project-templates/<TemplateId>/
```

TemplateId:

- 이 문서는 지원 template id 목록을 소유하지 않음
- 개발 중 template id: `static/project-templates/` 직접 하위 폴더 이름
- 배포 build template id: `resources/project-templates/` 직접 하위 폴더 이름
- `TemplateId`: 안전한 단일 경로 조각

필수 내용:

```text
setting.json
profile.json
scenario.json
policy/__init__.py
```

규칙:

- 모든 project template은 필수 내용 포함
- 빈 project용 template도 최소 유효 JSON과 기본 policy 포함
- 실행 예시용 template의 policy는 예시 구현
- 실행 예시용 template의 policy를 최소 골격 policy 기준으로 사용하지 않음

금지 내용:

- `runs/`, `review/`, `episodes/`, `snapshot/`
- `Client/Resources/policy-runtime.py`
- 개발 도구 문서와 실행 보조 파일
- 생성된 Python cache

## Run 기본 폴더

개발 위치:

```text
static/run-defaults/
```

배포 위치:

```text
build/Release/resources/run-defaults/
```

목적:

- `workspace.createRun`이 새 `runs/<RunId>/` directory 생성 시 먼저 복사
- run 기본 폴더 구조만 제공
- project 입력 파일 포함 금지
- Bridge/Simulator가 나중에 작성할 빈 결과 파일 포함 금지

허용 내용:

```text
review/
episodes/
```

source-only marker:

- `review/.gitkeep`
- `episodes/.gitkeep`
- `workspace.createRun` 복사 제외

금지 내용:

- `status.json`
- `summary.json`
- `snapshot/`
- `episodes/<EpisodeId>/`
- `setting.json`, `profile.json`, `scenario.json`, `policy/`
- 생성된 Python cache

생성 책임:

- `workspace.createRun`: run 기본 폴더 복사 후 `snapshot/` 생성
- `process.startSimulator`: `status.json` 생성
- Simulator: `summary.json`, episode 결과 파일 생성
