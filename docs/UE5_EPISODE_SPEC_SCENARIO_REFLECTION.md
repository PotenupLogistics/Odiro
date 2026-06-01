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

## 3. 성공 기준

* `static_obstacles`에 Kickboard semantic이 존재한다.
* `blocking_ratio`가 존재하고 0보다 크다.
* `pedestrians`와 `paths`가 연결된다.
* crossing behavior 또는 `pedestrian_crossing` path role이 존재한다.
* sidewalk width가 좁은 보도 조건에 맞는다.

## 4. UE controlled integration과의 관계

EpisodeSpec scenario reflection이 통과해야 UE controlled integration test로 넘긴다. 구조 validation만 통과하고 scenario reflection이 실패하면 UE에 전달할 준비가 덜 된 것으로 본다.

