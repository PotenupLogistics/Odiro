# OpenAI Live Smoke Status

## Scenario Generate API

검증 일시: 2026-06-04 16:22 KST

대상 API:

* `POST /api/v1/scenarios/generate`

입력 형태:

```json
{
  "prompt": "좁은 보도에서 정적 장애물이 배달 로봇의 경로 일부를 막고 있는 상황을 생성해줘. 보행자는 없고, 로봇은 안전하게 감속하거나 우회해야 한다."
}
```

민감정보 저장 정책:

* OpenAI API key 저장 없음
* raw OpenAI response 저장 없음
* rawContent 저장 없음
* full prompt 외 provider raw output 저장 없음

## 결과 요약

* OpenAPI 노출: 확인
* OpenAI live smoke: 실행
* 정상 입력 응답: 성공
* 응답 top-level field: `schema`, `version`, `runs`
* wrapper field: 없음
* explicit `null`: 없음
* run count: 5
* pair ID unique: 통과
* RunQueue path 형식: `Json/Input/...`
* EpisodeSetup file count: 5
* DeliveryBotSetup file count: 5
* EpisodeSetup validation: 통과
* DeliveryBotSetup validation: 통과
* RunQueue validation: 통과

Export path:

```text
data/run_queue_exports/20260604T071832Z_SCENARIO-GENERATE-001
```

## Optional Field / Null-free 확인

* EpisodeSetup / DeliveryBotSetup / RunQueue 전체에 explicit `null` 없음
* `paths=[]` 유지
* `actors.pedestrians=[]` 유지
* 숫자 `0.0` 값은 yaw 등에서 유지
* `evaluation.near_miss.distance_m=0.5` 기본값 확인
* `evaluation.scoring.pedestrian_collision=-10` 기본값 확인

## Variation 확인

* episode 0: baseline
* episode 1: obstacle lateral offset
* episode 2: obstacle lateral offset opposite side
* episode 3: `conservative_lidar`
  * `stop_distance_m`
  * `slow_down_distance_m`
  * `front_half_angle_degree`
* episode 4: `slower_path_follow`
  * `max_speed_kmh`
  * `target_speed_kmh`
  * `obstacle_slow_speed_kmh`
  * `min_turn_speed_kmh`

## Input Validation

아래 입력은 reject됨:

* `{ "prompt": "" }` -> HTTP 422
* `{ "prompt": "test", "extra": "x" }` -> HTTP 422
* `{}` -> HTTP 422

## 남은 리스크

이번 사용자용 API 입력은 `prompt` 하나만 받기 때문에 fixed parameter를 직접 전달하지 않는다. fixed parameter override 방지는 단위 테스트로 검증되어 있으며, Swagger live smoke에서는 별도 fixed input 검증 대상이 아니다.

이번 prompt에는 로봇 시작/목표 좌표 또는 route midpoint가 명시되지 않았다. OpenAI 생성 결과는 로봇 시작 `[2.0, 0.0]`, 목표 `[18.0, 0.0]`, 장애물 `[9.0, 0.0]` 계열로 생성되었다. 따라서 “특정 좌표 고정”이나 “정확한 midpoint 강제”는 이번 smoke의 성공 기준이 아니라 prompt-only API의 남은 설계 리스크로 본다.

## 검증 명령

```powershell
uv run python -m harness.checks.check_all
uv run pytest
```

결과:

* Harness: `PASS_WITH_WARNING`
* pytest: `497 passed, 1 warning`
