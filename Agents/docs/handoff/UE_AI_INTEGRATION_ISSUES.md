# UE-AI Integration Issues

상태: legacy EpisodeSpec handoff issue list.

- 현재 user project 실행 계약 아님
- 현재 scenario/run 파일 기준: `contracts/specs/user-project-data.md`
- 이 문서는 이전 UE handoff 확인 항목 기록

## 1. UE 확인 필요

| Issue | Classification | Note |
| --- | --- | --- |
| `obstacle.kickboard` prop_id 추가 가능 여부 | blocker | 실제 Kickboard mesh를 쓰려면 catalog 추가 필요 |
| `obstacle.road_barrier_01` 임시 매핑 시각 허용 여부 | non-blocker | MVP smoke는 이 매핑으로 진행 가능 |
| `EpisodeSandbox`에 `BP_DeliveryBot_GridBoundsActor` 존재 여부 | blocker | controlled integration 실행 전 확인 필요 |
| UEpisodeCompiler가 EpisodeSpec root fields를 모두 처리하는지 | blocker | `schema`, `run`, `ground_model`, `paths`, `actors` 처리 필요 |
| `ground_model.regions` 시각화/충돌 처리 방식 | non-blocker | 우선 debug visualization 가능 |
| `actors.static_obstacles` prop catalog 매핑 | blocker | prop_id resolution 필요 |
| `paths[].role=pedestrian_crossing` 처리 방식 | non-blocker | movement behavior와 연결 필요 |
| `actors.pedestrians[].properties.semantic_behavior` 처리 방식 | non-blocker | Crossing semantic 보존용 |
| `properties.blocking_ratio` 사용 여부 | deferred | debug/log/metric에 활용 가능 |
| robot `route.goal_m` 주입 확인 | blocker | robot route integration 필요 |

## 2. AI 확인 필요

| Issue | Classification | Note |
| --- | --- | --- |
| UE feedback에 따른 EpisodeSpec adapter 수정 | deferred | UE parser 결과 기반 조정 |
| `obstacle.kickboard` 추가 후 mapping 변경 | deferred | catalog 추가 후 prop_id 변경 |
| LLM 실패 시 자체 fallback 정책 유지 | deferred | API 안정성을 우선하고 로컬 provider는 수동 검증용으로 유지 |
| 추가 scenario type 확장 여부 | deferred | UE integration 후 결정 |

## 3. Blocker / Non-blocker / Deferred 기준

* blocker: UE controlled integration 실행 전에 반드시 해결해야 하는 항목
* non-blocker: MVP smoke는 가능하지만 품질/표현 개선을 위해 추적할 항목
* deferred: 다음 개발 단계에서 다룰 항목
