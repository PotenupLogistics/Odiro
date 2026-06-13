> Archived document.
> This document is kept for historical reference and is not the current UE contract.
> Current UE contracts live under `contracts/specs/`.

# UE5 EpisodeSpec Handoff Summary

## 1. 현재 상태

AI Backend는 OpenAI-first provider chain에서 내부 `WorldConfig`를 UE 실행용 `EpisodeSpec`으로 변환할 수 있다.

## 2. Endpoint

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`
* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=both`
* Ollama는 fallback provider로 유지한다.

## 3. EpisodeSpec 변환 범위

* `map` -> `ground_model.regions`
* `robot` -> `actors.robot`
* `obstacles` -> `actors.static_obstacles`
* `pedestrians` -> `paths` + `actors.pedestrians`
* `runtime` -> `run`

## 4. 단위

`EpisodeSpec`은 meter/degree 기준이다. AI 내부 `WorldConfig`의 cm 값은 adapter에서 m로 변환된다.

## 5. Kickboard 처리

현재 UE catalog에는 `obstacle.kickboard`가 없으므로 MVP에서는 `obstacle.road_barrier_01`로 임시 매핑하고 `properties.semantic_type="Kickboard"`를 남긴다. 실제 smoke에서 LLM이 Kickboard obstacle을 생성하지 않으면 mapping warning은 발생하지 않는다.

## 6. UE 쪽 확인 요청

* `obstacle.kickboard` prop_id 추가 가능 여부
* `BP_DeliveryBot_GridBoundsActor`가 테스트 맵에 있는지
* `EpisodeSandbox` 맵에서 `ground_model` region과 actor spawn이 정상 동작하는지
* `behavior=Crossing` pedestrian 처리 방식

## 7. 다음 단계

UE에서 EpisodeSpec JSON parsing 후 actor spawn controlled integration test를 진행한다.

## 8. Controlled scenario smoke 결과

`harness/reports/manual_openai_episode_spec_handoff_smoke.json` 기준으로 `providerUsed=openai`, `fallbackUsed=false`, Kickboard semantic, blocking ratio, crossing pedestrian, path linkage가 모두 기록된다. 이 report는 summary만 저장하며 API key, full WorldConfig, full EpisodeSpec은 저장하지 않는다.

## 9. Export CLI

UE 기본 권장 format은 `episode_spec`이다.

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format episode_spec
```

`responseFormat=both`와 `--format both`는 디버깅용이다. `--out` 없이 실행하면 파일을 생성하지 않고 콘솔에만 출력한다.
