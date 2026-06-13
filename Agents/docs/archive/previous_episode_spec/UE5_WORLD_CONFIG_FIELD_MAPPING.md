> Archived document.
> This document is kept for historical reference and is not the current UE contract.
> Current UE contracts live under `contracts/specs/`.

# UE5 World Config Field Mapping

## 1. 목적

AI Backend가 반환하는 `worldConfig`를 UE5에서 어떤 Actor 또는 Component 생성 로직으로 연결할지 정의한다.
UE 실행 계약인 EpisodeSpec의 기준 문서는 `docs/archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.

## 2. UE5가 읽는 최상위 필드

* `schemaVersion`
* `worldId`
* `scenarioId`
* `seed`
* `map`
* `robot`
* `obstacles`
* `pedestrians`
* `environmentObjects`
* `runtime`

## 3. map 필드 매핑

| Field | Meaning | UE5 구현 힌트 |
| --- | --- | --- |
| `map.type` | 맵 유형 | Sidewalk/Crosswalk 등 시나리오 tag |
| `map.lengthCm` | 보도 길이 | 보도 mesh 또는 spline length |
| `map.sidewalkWidthCm` | 보도 폭 | 보도 plane width |
| `map.surfaceCondition` | 노면 상태 | material/tag |
| `map.slopeDegree` | 경사 | plane rotation 또는 slope actor |

## 4. robot 필드 매핑

| Field | Meaning | UE5 구현 힌트 |
| --- | --- | --- |
| `robot.botId` | 로봇 식별자 | DeliveryRobot actor id |
| `robot.spawn.x/y/z` | 시작 위치 | DeliveryRobot actor transform location |
| `robot.goal.x/y/z` | 목적지 | Goal marker actor location |
| `robot.policyId` | 정책 식별자 | AI Backend policy context 또는 controller 설정값 |

현재 JSON Schema의 location에는 `yawDegree`가 없으므로 UE5 yaw는 별도 기본값 또는 UE5 내부 설정을 사용한다.

## 5. obstacles 필드 매핑

| Field | Meaning | UE5 구현 힌트 |
| --- | --- | --- |
| `objectId` | 장애물 식별자 | Actor name/tag |
| `type` | 장애물 종류 | `Kickboard`이면 Kickboard obstacle actor |
| `position.x/y/z` | 위치 | Actor transform location |
| `blockingRatio` | 경로 차단 정도 | 디버그 표시 또는 테스트 metric |

현재 JSON Schema의 obstacle position에는 `yawDegree`가 없으므로 yaw는 UE5 내부 기본값을 사용한다.

## 6. pedestrians 필드 매핑

| Field | Meaning | UE5 구현 힌트 |
| --- | --- | --- |
| `objectId` | 보행자 식별자 | Pedestrian actor id |
| `spawn.x/y/z` | 시작 위치 | 이동 시작 point |
| `goal.x/y/z` | 목적 위치 | 이동 목표 point |
| `speedKmh` | 이동 속도 | movement component speed |
| `behavior` | 보행자 행동 | `Crossing`이면 spawn에서 goal 방향으로 횡단 이동 |

## 7. runtime 필드 매핑

| Field | Meaning | UE5 구현 힌트 |
| --- | --- | --- |
| `runtime.maxDurationSec` | 최대 실행 시간 | simulation timeout |
| `runtime.captureReplay` | replay 저장 여부 | replay capture toggle |
| `runtime.emitEventLog` | event log 출력 여부 | event log export toggle |

## 8. UE5가 무시해도 되는 handoff metadata

* `metadata`
* `diagnostics`
* `postProcessing`
* `warnings`

UE5는 `worldConfig`를 실행 기준으로 사용하고, `diagnostics`와 `postProcessing`은 디버깅용으로만 사용한다.
# WorldConfig to EpisodeSpec mapping

* `map.lengthCm`, `map.sidewalkWidthCm` -> `ground_model.regions[].shape.size_m` meter 값
* `robot.spawn` -> `actors.robot.transform.location_m`
* `robot.goal` -> `actors.robot.route.goal_m`
* `pedestrians[].spawn/goal` -> `paths[].points_m`
* `pedestrians[]` -> `actors.pedestrians[]`
* `obstacles[]` -> `actors.static_obstacles[]`
* `runtime.maxDurationSec` -> `run.time_limit_s`

`WorldConfig`는 계속 AI 내부 계약으로 유지하고, UE 실행 직전에 `EpisodeSpec`으로 변환한다.
## Environment Parameter Conversion

Environment parameters use explicit numeric values. Labels such as low / middle / high may be used in prose only and must not be stored as JSON contract values.

* AI `sidewalkWidthCm` is cm and maps to `EpisodeSpec` meter values in `shape.size_m`.
* `sidewalkWidthCm=120` maps to `shape.size_m` y value `1.2`.
* `map.lengthCm=1200` maps to `shape.size_m` x value `12.0`.
* `pedestrianSpeedMps` maps to `movement.speed_mps`.
* `obstacleBlockingRatio` maps to `properties.blocking_ratio`.
* `timeLimitSec` maps to `run.time_limit_s`.
