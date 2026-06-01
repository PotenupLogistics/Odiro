# UE5 EpisodeSpec Adapter

## 1. 목적

AI 내부 `WorldConfig`를 UE5 MVP 컴파일러가 읽을 수 있는 `EpisodeSpec`으로 변환한다.
EpisodeSpec 계약의 source of truth는 `docs/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.

## 2. 왜 adapter가 필요한가

* AI 내부 계약은 `WorldConfig`이고 UE 실행 계약은 `EpisodeSpec`이다.
* AI `WorldConfig`는 cm 기준이고, UE `EpisodeSpec`은 meter 기준이다.
* UE는 `actors`와 `paths` 중심으로 환경을 구성한다.

## 3. 변환 규칙

* `map` -> `ground_model.regions`
* `robot` -> `actors.robot`
* `obstacles` -> `actors.static_obstacles`
* `pedestrians` -> `paths` + `actors.pedestrians`
* `runtime` -> `run`
* guide에 없는 `penalties` 필드는 생성하지 않는다.
* penalty region이 필요할 때는 guide의 singular `penalty` 필드를 사용한다.
* `properties`는 shallow map으로 유지하며 boolean, number, string, numeric vector3만 사용한다.

## 4. 단위 변환

* cm -> m
* kmh -> m/s
* degree 유지

## 5. Kickboard mapping

현재 UE catalog에는 `obstacle.kickboard`가 없다.
MVP에서는 `obstacle.road_barrier_01`로 임시 매핑하고 `properties.semantic_type="Kickboard"`를 남긴다.
UE 쪽에는 `obstacle.kickboard` prop 추가 여부 확인이 필요하다.

## 5.1 Generic obstacle mapping

Generic `Obstacle` type is converted to `actors.static_obstacles`.
The current fallback prop is `obstacle.box_01`.
The adapter preserves `properties.semantic_type="Obstacle"` and maps `blockingRatio` to `properties.blocking_ratio`.

If the prompt explicitly says there are no pedestrians, empty `actors.pedestrians` and empty `paths` are valid for EpisodeSpec scenario reflection.

## 6. 검증 규칙

`episode_spec_validator`는 root 필드, meter/degree 단위, actor/path id 중복, pedestrian path 참조, static obstacle catalog, rectangle ground shape, robot route, 위치/크기/스케일 배열 길이, pedestrian speed 권장 범위를 검증한다.

## 7. Controlled smoke

수동 smoke 결과는 `harness/reports/ue5_episode_spec_handoff_smoke.json`와 `harness/reports/ue5_episode_spec_handoff_smoke.md`에 기록한다. 하네스는 live Ollama를 호출하지 않고 report 구조만 검증한다.

## 8. Scenario reflection

`episode_spec_scenario_reflection`은 EpisodeSpec의 구조가 아니라 자연어 시나리오 조건 반영 여부를 확인한다. Kickboard semantic, generic obstacle semantic, blocking ratio, pedestrian actor, crossing path, path linkage, no-pedestrian prompt, narrow sidewalk 조건을 확인한다.

## Environment Parameter Conversion

The adapter uses concrete numeric environment parameters. It does not use low / middle / high as contract values.

Unit conversion rules:

* AI internal `sidewalkWidthCm` uses cm and is converted from cm to m for `EpisodeSpec` `ground_model.regions[].shape.size_m`.
* `pedestrianSpeedMps` maps directly to `EpisodeSpec` `movement.speed_mps` when pedestrian movement speed is available.
* `obstacleBlockingRatio` maps to `EpisodeSpec` `properties.blocking_ratio`.
* `timeLimitSec` maps to `EpisodeSpec` `run.time_limit_s`.

Example conversions:

* `sidewalkWidthCm=120` -> `shape.size_m` y value `1.2`
* `obstacleBlockingRatio=0.9` -> `properties.blocking_ratio=0.9`
* `pedestrianSpeedMps=1.2` -> `movement.speed_mps=1.2`
* `timeLimitSec=60` -> `run.time_limit_s=60`
