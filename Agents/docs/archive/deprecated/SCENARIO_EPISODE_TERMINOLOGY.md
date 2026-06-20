# Scenario / Episode Terminology

> Deprecated
>
> 현재 Scenario / Episode / Run / EpisodeScenario 용어의 canonical 기준은
> [docs/specs/simulation-interface.md](../../../../docs/specs/simulation-interface.md)입니다.
>
> 이 문서는 legacy UE handoff 시기의 용어 추적용으로만 보관합니다.
> 현재 v2 Agent 기준 문서나 LLM context allowlist에 포함하지 않습니다.

## Scenario

* 추상적인 상황 유형이다.
* 예: `obstacle_ahead`, `narrow_sidewalk_kickboard_crossing`
* 자연어 prompt에서 추출되는 의도다.
* `environmentSampling.scenarioType`과 연결 가능하다.

## Episode

* 실제 한 번 실행되는 구체 시뮬레이션 인스턴스다.
* scenario에 seed, 위치, 보도폭, 장애물, 로봇 설정 등이 적용된 결과다.
* 같은 scenario라도 seed/파라미터가 다르면 다른 episode가 될 수 있다.

## EpisodeSetup

* Episode의 맵/액터/경로/로봇 위치/목적지를 정의하는 JSON이다.
* WorldConfig에서 변환되어 생성될 UE 입력 JSON이다.
* `xy_m`, `yaw_deg`, `goal_xy_m`를 사용한다.

## DeliveryBotSetup

* DeliveryBot의 `drive`, `path_follow`, `lidar` 튜닝값을 정의하는 JSON이다.
* 로봇 위치, 목적지, route, run 정보는 포함하지 않는다.

## Execution Pair

* EpisodeSetup + DeliveryBotSetup 조합이다.
* UE Runner가 실제로 실행하는 입력 단위다.

## EvaluationReport

* Episode 실행 결과 JSON이다.
* 현재 작업에서는 결과 분석 담당 범위 밖이므로 구현하지 않는다.
* 문서와 향후 연동 지점만 기록한다.

## scenario_id 사용 기준

* `scenario_id`는 episode의 유형 또는 생성 의도를 나타내는 식별자로 사용한다.
* 실제 실행 단위는 `pair_id`, `run_id`, `requestId` 등으로 추적한다.
