# Pedestrian Social Evaluation Design

## 목적

이 문서는 OdiroSim의 보행자를 단순 이동 장애물이 아니라 로봇 정책을 검증하는 사회적/동적 평가 장치로 확장하기 위한 설계와 구현 단계를 정리한다.

핵심 방향은 다음이다.

```text
Pedestrian = setup-time planned baseline trajectory + deterministic robot reaction
```

보행자는 에피소드 셋업 단계에서 재현 가능한 baseline trajectory를 갖는다. 런타임에서는 그 baseline을 기준으로 이동하되, 로봇 액터 때문에 필요한 감속, 정지, 짧은 sidestep, blocked/recover 상태만 deterministic하게 수행한다. 로봇 때문에 늦어진 시간은 speed-up/catch-up으로 회복하지 않는다.

## 현재 구현 요약

현재 구현된 범위:

- `planned_trajectory` 보행자 입력 지원
- `start_xy_m` / `goal_xy_m` 기반 baseline plan 생성
- 정적 장애물 footprint 기반 deterministic detour 생성
- optional deterministic path curve sampling
- `UEpisodePedestrianPlanSubsystem` plan cache
- pure C++ `FScenarioPedestrianPlanBuilder`
- `UEpisodePedestrianRuntimeComponent` baseline follow
- behavior parameter optional parsing
- `BehaviorHash`, `PedestrianScenarioHash`
- 로봇 weak reference 주입 기반 runtime reaction
- state machine: `FollowBaseline`, `YieldSlowdown`, `YieldStop`, `Sidestep`, `Blocked`, `Recover`
- runtime metric 누적: schedule delay, forced wait, blocked duration, path deviation, min robot distance

아직 구현하지 않은 범위:

- pedestrian runtime metric의 evaluation report / JSONL 정식 집계
- pedestrian-pedestrian path conflict / reservation scheduling
- authored `paths[]` / spline을 `FScenarioPedestrianPlan` source로 변환
- A-Star/NavMesh/flow-field 수준의 전역 planner
- named profile preset expansion
- final scalar score collapse

## ScenarioSetup 입력 모델

현재 planned pedestrian의 기본 입력은 다음 형태다.

```json
{
  "instance_id": "ped_01",
  "archetype_id": "adult_pedestrian",
  "xy_m": [-4, 0],
  "yaw_deg": 0,
  "start_xy_m": [-4, 0],
  "goal_xy_m": [4, 0],
  "movement": {
    "model": "planned_trajectory",
    "speed_mps": 1.2,
    "curve_offset_m": 0.0,
    "curve_sample_spacing_m": 0.5,
    "initial_distance_m": 0,
    "auto_start": true
  }
}
```

`actors.pedestrians[].behavior`는 선택 사항이다. 없으면 기본값으로 resolve된다.

```json
{
  "behavior": {
    "cooperation": 0.7,
    "evasiveness": 0.5,
    "personal_space_m": 0.8,
    "awareness_horizon_s": 2.5,
    "max_yield_wait_s": 4.0,
    "sidestep_distance_m": 0.6
  }
}
```

현재 지원 behavior fields:

| JSON field | 내부 property | 기본값 |
| --- | --- | --- |
| `cooperation` | `behavior_cooperation` | `0.5` |
| `evasiveness` | `behavior_evasiveness` | `0.35` |
| `personal_space_m` / `personal_space_cm` | `behavior_personal_space_cm` | `80cm` |
| `awareness_horizon_s` | `behavior_awareness_horizon_s` | `2.5s` |
| `max_yield_wait_s` | `behavior_max_yield_wait_s` | `4.0s` |
| `sidestep_distance_m` / `sidestep_distance_cm` | `behavior_sidestep_distance_cm` | `60cm` |

`sidestep_distance_m`은 사용자가 기대하는 선호 lateral offset이다. 로봇 footprint, personal space, predicted closest point를 기준으로 더 큰 clearance가 필요하면 runtime은 내부 안전 한도까지 sidestep 목표를 확장할 수 있다.

planned trajectory의 baseline geometry는 `movement` 아래 optional curve fields로 조정할 수 있다.

| JSON field | 내부 property | 기본값 |
| --- | --- | --- |
| `curve_offset_m` / `curve_offset_cm` | `path_curve_offset_cm` | `0cm` |
| `curve_sample_spacing_m` / `curve_sample_spacing_cm` | `path_curve_sample_spacing_cm` | `50cm` |

`curve_offset_m`가 `0`이면 기존처럼 raw polyline을 그대로 사용한다. 값이 있으면 setup 단계에서 deterministic curve sample point를 생성하고, 이 point들이 `PlanHash`, nominal duration, runtime conflict prediction의 기준이 된다.

기존 `paths[]` 포맷은 여전히 legacy spline follower 입력으로 유지된다.

```json
"paths": [
  {
    "path_id": "ped_01_path",
    "points_xy_m": [[-3, -3], [3, 3]],
    "closed_loop": false
  }
]
```

현재 `planned_trajectory`는 이 `paths[]`를 plan source로 사용하지 않는다. 장기적으로는 `paths[]` 또는 editor-authored spline을 route source로 받아 `FScenarioPedestrianPlan`으로 샘플링하는 방향이 맞다.

## 시스템 책임

### UScenarioCompiler

책임:

- ScenarioSetup JSON을 `FScenarioWorldSpec` / `FScenarioSimulationSetupSpec`로 컴파일한다.
- 기존 `path_id` 기반 보행자와 신규 `planned_trajectory` 보행자를 모두 허용한다.
- `planned_trajectory` 보행자에 대해 `start_xy_m`, `goal_xy_m`을 필수로 검증한다.
- `actors.pedestrians[].behavior`가 있으면 optional numeric property로 복사한다.
- meter 입력은 centimeter property로 변환한다.

하지 않는 일:

- 실제 actor bounds 조회
- static obstacle footprint resolve
- pedestrian plan 생성
- runtime robot reaction 판단

### UEpisodeSimulationSubsystem

책임:

- 에피소드 월드 lifecycle을 관리한다.
- ground region, path actor, static obstacle, robot, pedestrian actor를 spawn/clear한다.
- static obstacle actor spawn 이후 resolved footprint를 수집한다.
- `UEpisodePedestrianPlanSubsystem::BuildPlans`를 호출한다.
- planned pedestrian spawn 시 `FScenarioPedestrianPlan`을 runtime component에 주입한다.
- planned pedestrian runtime component에 평가 대상 robot actor weak reference를 주입한다.

현재 world setup 순서:

```text
SetupEpisodeWorld
  -> ClearEpisode
  -> GroundRegions spawn
  -> Paths spawn
  -> Placeables spawn
       - StaticObstacle
       - DeliveryBot
  -> BuildPedestrianPlanContext
       - resolved static obstacle footprints
  -> UEpisodePedestrianPlanSubsystem::BuildPlans
  -> DynamicActors spawn
       - legacy spline pedestrian
       - planned trajectory pedestrian
  -> BuildRuntimeContext
```

로봇 반응 최적화:

- `UEpisodePedestrianRuntimeComponent`가 매 tick world actor 전체를 검색하지 않는다.
- `UEpisodeSimulationSubsystem`이 planned pedestrian spawn 시점에 `DeliveryBot` runtime actor를 찾아 `SetRobotActor(...)`로 주입한다.
- runtime component는 주입된 `TWeakObjectPtr<AActor>` 하나만 예측 대상으로 사용한다.

하지 않는 일:

- path planning algorithm 직접 구현
- pedestrian state machine 실행
- evaluation score 계산

### UEpisodePedestrianPlanSubsystem

타입: `UWorldSubsystem`

책임:

- 현재 world/episode에 대한 pedestrian plan cache를 소유한다.
- `BuildPlans(...)`, `FindPlan(instanceId)`, `ClearPlans()` API를 제공한다.
- build diagnostics와 build result를 로깅한다.

하지 않는 일:

- 매 tick 이동 판단
- robot reaction 실행
- evaluation score 산출

### FScenarioPedestrianPlanBuilder

타입: pure C++ helper/algorithm class

책임:

- `planned_trajectory` 보행자만 골라 deterministic baseline plan을 생성한다.
- `InstanceId` 기준 정렬로 결과 순서를 안정화한다.
- `planned_start_cm`, `planned_goal_cm`, `speed_cm_per_second`를 읽는다.
- 정적 장애물 footprint와 start-goal 선분이 교차하면 deterministic detour point를 추가한다.
- `FEpisodePedestrianBehaviorParams`를 resolve한다.
- `FEpisodePedestrianPathShapeParams`를 resolve하고 curve sample point를 생성한다.
- `PlanHash`, `BehaviorHash`, `PedestrianScenarioHash`를 계산한다.

현재 한계:

- full pathfinding이 아니다.
- detour segment가 다시 다른 obstacle과 충돌하는지 완전 검증하지 않는다.
- 다른 보행자 baseline/reservation은 고려하지 않는다.
- authored spline route source는 아직 읽지 않는다.
- curve smoothing은 obstacle clearance를 침범한다고 판단되면 raw polyline으로 fallback한다.

### UEpisodePedestrianRuntimeComponent

책임:

- 자신에게 주입된 `FScenarioPedestrianPlan`을 따라 baseline progress 기반으로 이동한다.
- 로봇 weak reference와 위치 변화 기반 observed velocity를 사용해 fixed-sample conflict prediction을 수행한다.
- 로봇의 live global path/repath 결과는 보행자 반응 입력으로 사용하지 않는다. 이는 로봇 정책의 회피 행동과 보행자 회피 행동이 서로를 따라가며 지표를 오염시키는 피드백을 막기 위함이다.
- deterministic state machine을 실행한다.
- speed-up/catch-up 없이 delay를 보존한다.
- runtime metrics를 누적한다.

현재 state:

| State | 의미 |
| --- | --- |
| `FollowBaseline` | conflict 없음. baseline을 normal speed로 진행 |
| `YieldSlowdown` | predicted conflict가 있으나 hard stop 전. 감속 |
| `YieldStop` | personal space/stop distance 안쪽 conflict. progress 정지 |
| `Sidestep` | evasiveness가 충분할 때 좌/우 후보를 예측 평가하고, 선택한 방향을 state 동안 고정한 뒤 clearance lateral offset 적용 |
| `Blocked` | `max_yield_wait_s` 이상 yield stop 지속 |
| `Recover` | conflict 해소 후 lateral offset을 baseline으로 복귀 |

전이 안정화 기준:

- `Sidestep`은 일반 전이에서 최소 `2.0s` 유지한다.
- `YieldStop`과 `Recover`도 짧은 깜빡임처럼 보이지 않도록 state별 최소 유지 시간을 둔다.
- conflict가 해제되어도 즉시 `Recover`/`FollowBaseline`으로 가지 않고, clear 상태가 짧게 유지된 뒤 전이한다.
- actor yaw는 목표 이동 방향을 바로 대입하지 않고 tick delta 기반 보간으로 따라간다.

현재 runtime metrics:

| Property | 의미 |
| --- | --- |
| `ActiveTimeSeconds` | runtime component 활성 시간 |
| `ScheduleDelaySeconds` | nominal baseline progress 대비 지연 |
| `ForcedWaitSeconds` | `YieldStop`/`Blocked` 누적 시간 |
| `BlockedDurationSeconds` | `Blocked` 누적 시간 |
| `PathDeviationCm` | 현재 lateral deviation |
| `MaxPathDeviationCm` | episode 중 최대 lateral deviation |
| `MinRobotDistanceCm` | 주입된 robot과의 최단 거리 |

하지 않는 일:

- baseline plan 자체 변경
- global reroute
- pedestrian-pedestrian avoidance
- evaluation report 직접 작성

## Hash / Provenance

Hash는 값을 대체하기 위한 저장 방식이 아니라, derived artifact가 어떤 입력에서 나왔는지 검증하기 위한 fingerprint다.

현재 hash 구분:

| Hash | 의미 |
| --- | --- |
| `SourceSpecHash` | source ScenarioSetup spec fingerprint |
| `ResolvedFootprintHash` | spawn 이후 resolve된 static obstacle footprint fingerprint |
| `PlanHash` | baseline path/timing fingerprint |
| `BehaviorHash` | resolved behavior parameter fingerprint |
| `PedestrianScenarioHash` | `PlanHash + BehaviorHash` fingerprint |

원칙:

```text
SourceSpec + Seed + ResolvedFootprints + SemanticNavConfig
  -> FScenarioPedestrianPlan
  -> PlanHash

Behavior JSON/defaults
  -> FEpisodePedestrianBehaviorParams
  -> BehaviorHash

PlanHash + BehaviorHash
  -> PedestrianScenarioHash
```

현재 구현은 `uint32` 문자열 hash를 사용한다. 장기적으로 실험 무결성을 더 강하게 보장하려면 canonical serialization 기반 `SHA-256`으로 교체하는 것이 맞다.

## 평가 지표 카탈로그

### Safety

| 지표 | 단위 | 상태 | 정의 |
| --- | --- | --- | --- |
| `pedestrian_collision_count` | count | 기존 유지 | 로봇-보행자 collision 횟수 |
| `near_miss_count` | count | 기존 유지 | near-miss 구간 수 |
| `near_miss_total_duration_s` | s | 기존 유지 | near-miss 누적 시간 |
| `near_miss_min_distance_m` | m | 기존 유지 | episode 중 최단 보행자 거리 |
| `time_inside_safety_radius_s` | s | MVP3 후보 | safety radius 안에 머문 누적 시간 |
| `min_time_to_collision_s` | s | 후속 | 상대 속도 기반 최소 TTC |

### Courtesy / Non-Obstruction

| 지표 | 단위 | 상태 | 정의 |
| --- | --- | --- | --- |
| `pedestrian_forced_wait_duration_s` | s | runtime metric 구현, report 연동 필요 | 로봇 때문에 정지/대기한 누적 시간 |
| `pedestrian_schedule_delay_s` | s | runtime metric 구현, report 연동 필요 | baseline progress 대비 지연 |
| `pedestrian_path_deviation_m` | m | runtime metric 구현, report 연동 필요 | baseline에서 벗어난 lateral deviation |
| `pedestrian_blocked_duration_s` | s | runtime metric 구현, report 연동 필요 | `Blocked` 상태 누적 시간 |
| `robot_blocked_pedestrian_path_duration_s` | s | 후속 | 로봇 footprint가 pedestrian corridor를 점유한 시간 |

주의:

- schedule delay는 catch-up/speed-up으로 줄이지 않는다.
- sidestep 후 baseline으로 돌아와도 deviation 기록은 유지한다.

### Predictability

MVP3에 우선 포함할 후보:

| 지표 | 단위 | 정의 |
| --- | --- | --- |
| `robot_cut_in_front_count` | count | 로봇이 보행자의 baseline corridor 앞을 기하학적으로 가로지른 횟수 |
| `conflict_entry_speed_mps` | m/s | 로봇이 pedestrian conflict zone에 들어갈 때의 속도 |

후순위:

| 지표 | 이유 |
| --- | --- |
| `late_yield_count` | yield event와 conflict timing 정의가 필요해 noisy할 수 있음 |
| `yield_start_time_before_conflict_s` | state machine과 conflict 시점 정의에 민감 |
| `sudden_stop_near_pedestrian_count` | 로봇 제어 특성과 정책 의도 분리가 필요 |

### Efficiency / Task Balance

| 지표 | 단위 | 상태 | 정의 |
| --- | --- | --- | --- |
| `goal_reached` | 0/1 | 기존 | 목표 도달 여부 |
| `duration_s` | s | 기존 | episode duration |
| `goal_distance_m` | m | 기존 | 종료 시 목표까지 거리 |
| `robot_idle_duration_s` | s | MVP3 후보 | 불필요한 정지 시간 |
| `robot_extra_path_distance_m` | m | 후속 | 기준 경로 대비 추가 이동 거리 |

## MVP별 구현 계획

### MVP 1: Planned baseline trajectory

상태: 완료

구현된 것:

- shared plan/point/reservation type 추가
- compiler에 `planned_trajectory` 입력 파싱 추가
- `UEpisodePedestrianPlanSubsystem` 추가
- `FScenarioPedestrianPlanBuilder` 추가
- static obstacle footprint 수집 경로 추가
- deterministic static obstacle detour 생성
- `PlanHash` 산출
- optional path curve sampling
- planned pedestrian spawn/bind
- `UEpisodePedestrianRuntimeComponent` baseline follower 구현
- planned pedestrian obstacle sample JSON 추가

검증:

- 같은 setup/spec/footprint에서 같은 plan 생성
- static obstacle이 direct route 위에 있으면 detour point 생성
- 기존 spline pedestrian 경로 유지

### MVP 2: Robot-aware deterministic reaction + 필수 계측

상태: core 구현 완료, report 연동 남음

구현된 것:

- optional `actors.pedestrians[].behavior` 파싱
- `FEpisodePedestrianBehaviorParams`
- `BehaviorHash`
- `PedestrianScenarioHash`
- runtime state machine
- robot conflict fixed-sample prediction
- world actor scan 제거
- `UEpisodeSimulationSubsystem`에서 robot weak reference 주입
- forced wait / schedule delay / deviation / blocked duration / min robot distance 누적
- no catch-up 규칙 적용

남은 것:

- runtime metric을 `UEpisodeEvaluationSubsystem`에서 읽어 report/event로 집계
- forced wait count / state transition event 누적
- blocked event policy 결정
- threshold calibration
- runtime behavior automation/integration test

검증 기준:

- 로봇이 보행자 path corridor를 막으면 보행자가 감속/정지/sidestep한다.
- 로봇이 사라지거나 충분히 멀어지면 normal speed로 재개한다.
- `ScheduleDelaySeconds`는 speed-up으로 줄어들지 않는다.
- 같은 episode 조건에서 reaction state와 stats가 재현된다.

### MVP 3: Social evaluation metrics

목표:

- MVP2 runtime metric을 evaluation/report에 연결한다.
- safety/courtesy/predictability/efficiency breakdown을 만든다.
- scalar score는 reporting/ranking용으로만 둔다.

작업:

- `pedestrian_forced_wait_duration_s`
- `pedestrian_schedule_delay_s`
- `pedestrian_path_deviation_m`
- `pedestrian_blocked_duration_s`
- `robot_cut_in_front_count`
- `conflict_entry_speed_mps`
- report JSON schema 업데이트

### MVP 4: Pedestrian profiles

목표:

- 사용자가 named preset으로 behavior를 쉽게 선택하게 한다.
- 내부적으로는 speed/cooperation/evasiveness/personal-space를 독립 축으로 유지한다.

예상 preset:

| profile | 의도 |
| --- | --- |
| `Passive` | 로봇에게 비교적 잘 양보 |
| `Normal` | 표준 보행자 |
| `Assertive` | 자신의 path priority가 높고 덜 비킴 |
| `Vulnerable` | 보호 중심 preset. 단, speed/evasiveness/cooperation은 독립 축으로 유지 |

## Open Decisions

| 항목 | 현재 판단 |
| --- | --- |
| authored spline path | route source로 유지할 가치가 큼. 단, runtime follower가 아니라 plan source로 변환해야 함 |
| pedestrian-pedestrian conflict | MVP2 밖. setup-time reservation/scheduling으로 후속 처리 |
| global reroute | 로봇 때문에 runtime reroute하지 않음. baseline 비교가 깨짐 |
| Blocked 회복 | MVP2에서는 conflict clear 후 `Recover`. reroute 없음 |
| multi robot | 현재는 평가 대상 robot 1대 weak reference 주입. 다중 로봇은 registry/snapshot으로 확장 |
| hash algorithm | 현재 `uint32` 문자열. 장기적으로 canonical SHA-256 필요 |
