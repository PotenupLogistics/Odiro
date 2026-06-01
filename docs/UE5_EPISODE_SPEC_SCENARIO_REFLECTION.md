# UE5 EpisodeSpec Scenario Reflection

## 1. 목적

`EpisodeSpec`이 UE compiler가 읽을 수 있는 구조를 갖췄는지만 보는 것이 아니라, 자연어 시나리오 조건이 actor/path로 반영됐는지 검증한다.

## 2. 검증 대상

* Kickboard obstacle
* `blocking_ratio`
* pedestrian actor
* pedestrian crossing path
* `path_id` 연결
* narrow sidewalk
* environment sampler numeric constraints

## 3. 성공 기준

* `static_obstacles`에 Kickboard semantic이 존재한다.
* `blocking_ratio`가 존재하고 0보다 크다.
* `pedestrians`와 `paths`가 연결된다.
* crossing behavior 또는 `pedestrian_crossing` path role이 존재한다.
* sidewalk width가 좁은 보도 조건에 맞는다.
* environmentSampling에서 `sidewalkWidthCm=120`이 주어지면 EpisodeSpec width는 `1.2m`다.
* 장애물/경로 차단 요구가 있으면 `static_obstacles`와 `blocking_ratio`가 필수다.
* 해당 요구가 있는데 `staticObstacleCount=0` 또는 `hasBlockingRatio=false`이면 `passed=false`와 `ueCompilerReadiness=false`로 처리한다.

## 4. UE controlled integration과의 관계

EpisodeSpec scenario reflection이 통과해야 UE controlled integration test로 넘긴다. 구조 validation만 통과하고 scenario reflection이 실패하면 UE에 전달할 준비가 덜 된 것으로 본다.

## Route-relative placement

* EpisodeSpec에서는 robot transform `location_m`과 route `goal_m`의 midpoint를 meter 기준으로 계산한다.
* route midpoint intent가 있으면 첫 static obstacle의 `transform.location_m`이 midpoint 0.5m 이내인지 검사한다.
* spawn=(0,0,0), goal=(8.0,0,0)이면 expected midpoint는 (4.0,0,0)이다.
* obstacle이 midpoint에서 벗어나면 `obstacle_not_near_route_midpoint` issue를 만들고 `ueCompilerReadiness=false`로 처리한다.
