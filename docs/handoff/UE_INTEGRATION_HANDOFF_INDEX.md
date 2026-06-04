# UE Integration Handoff Index

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

## 5. 현재 추천 endpoint

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`
* 디버깅용: `responseFormat=both`

## 6. 현재 확인된 smoke 결과

controlled smoke 기준으로 `handoffSuccess`, `episodeValidationPassed`, `episodeScenarioReflectionPassed`, `hasKickboardSemantic`, `hasBlockingRatio`, `hasCrossingPedestrian`, `pedestrianPathLinked`, `ueCompilerReadiness`가 모두 true로 기록됐다.
