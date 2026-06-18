# UE Integration Handoff Index

상태: legacy handoff index.

- 현재 user project 실행 계약 아님
- 현재 scenario/run 파일 기준: `contracts/specs/user-project-data.md`
- 이 문서는 이전 UE handoff 문서 탐색용

## 1. 목적

UE 담당자가 AI Backend handoff 관련 문서를 어떤 순서로 보면 되는지 안내한다.

## 2. 먼저 볼 문서

* `UE_TEAM_HANDOFF_PACKAGE.md`
* `UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md`
* `UE_EPISODE_SPEC_JSON_GUIDE.md`
* `UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md`
* `UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md`

## 3. 구현 참고 문서

* `UE5_EPISODE_SPEC_ADAPTER.md`
* `UE5_WORLD_CONFIG_FIELD_MAPPING.md`
* `UE5_WORLD_CONFIG_PARSER_PSEUDOCODE.md`
* `UE5_CONTROLLED_INTEGRATION_TEST_PLAN.md`
* `UE5_HANDOFF_ACCEPTANCE_CHECKLIST.md`

## 4. 디버깅 참고 문서

* `UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md`
* `SCENARIO_POST_PROCESSING.md`
* `SCENARIO_REFLECTION_VALIDATION.md`
* `WORLD_CONFIG_GENERATION_ORCHESTRATOR.md`

## 5. Legacy endpoint 기록

* `POST /api/v1/scenarios/generate`

이 endpoint는 현재 `410 RUN_QUEUE_REMOVED` 안내만 반환한다.

Legacy `/api/v1/ue5/world-config/handoff`와 `responseFormat=episode_spec` / `responseFormat=both` 설명은 archive/tooling 참고용이며 현재 FastAPI/OpenAPI에는 노출되지 않는다.

## 6. Legacy smoke 결과

controlled smoke 기준으로 `handoffSuccess`, `episodeValidationPassed`, `episodeScenarioReflectionPassed`, `hasKickboardSemantic`, `hasBlockingRatio`, `hasCrossingPedestrian`, `pedestrianPathLinked`, `ueCompilerReadiness`가 모두 true로 기록됐다.
