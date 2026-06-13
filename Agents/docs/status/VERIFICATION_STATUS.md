# Verification Status

현재 검증 snapshot. 최신 실행 결과로 갱신한다.

## Project Checks

- `uv run pytest` -> `500 passed, 1 warning`
- `uv run python -m harness.checks.check_all` -> `PASS_WITH_WARNING`

현재 harness warning은 일부 source document와 manual review workflow가 아직 완료되지 않았기 때문에 남아 있습니다. Legacy UE handoff route는 제거되었고, 사용자용 scenario generation과 RunQueue export check를 기준으로 검증합니다.

## Controlled Smoke

- `providerUsed=openai`
- `fallbackUsed=false`
- `effectiveResponseFormat=episode_spec`
- `handoffSuccess=true`
- `episodeValidationPassed=true`
- `episodeScenarioReflectionPassed=true`
- `ueCompilerReadiness=true`
- `environmentSampling.enabled=true`
- `sidewalkWidthCm=120`
- `obstacleBlockingRatio=0.6`
- `timeLimitSec=60`

## Setup Pair Live Smoke

- `providerUsed=openai`
- `effectiveResponseFormat=setup_pair`
- `handoffSuccess=true`
- `episodeSetupExists=true`
- `deliveryBotSetupExists=true`
- `episodeSetupValidationPassed=true`
- `deliveryBotSetupValidationPassed=true`
- `setupPairTraceExists=true`
- `EpisodeSetup`: robot `[0.0, 0.0]` -> `[8.0, 0.0]`, obstacle `[4.0, 0.0]`
- `DeliveryBotSetup`: `stop_distance_m=1.2`, `slow_down_distance_m=3.5`

## Scenario Generation Smoke

- Endpoint: `POST /api/v1/scenarios/generate`
- Input: required natural language `prompt`, optional `episode_count`
- Output: wrapper-free RunQueue JSON
- OpenAI live smoke 1회 통과
- `run count=5`
- EpisodeSetup / DeliveryBotSetup / RunQueue are null-free
- EpisodeSetup artifact includes additive root field `robot_profile`
- narrow sidewalk policy comparison uses one EpisodeSetup and five policy-specific DeliveryBotSetup files

## Related Notes

- [Handoff Release Notes](../handoff/HANDOFF_RELEASE_NOTES.md)
- [UE Setup Pair Handoff Package](../handoff/UE_SETUP_PAIR_HANDOFF_PACKAGE.md)
- [UE5 Endpoint Usage For UE Team](../handoff/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
