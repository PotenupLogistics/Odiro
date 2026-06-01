# Proto-AI 문서 인덱스

루트 [README.md](../README.md)는 프로젝트의 최상위 entry point입니다. 이 문서는 구현 세부 사항, UE handoff, 현재 상태를 확인하기 위한 상세 문서 인덱스입니다.

## 먼저 볼 문서

* [Current Project Status](CURRENT_PROJECT_STATUS.md)
* [Next Actions](NEXT_ACTIONS.md)
* [UE AI Integration Issues](UE_AI_INTEGRATION_ISSUES.md)

## UE handoff 문서

* [UE Integration Handoff Index](UE_INTEGRATION_HANDOFF_INDEX.md)
* [UE Team Handoff Package](UE_TEAM_HANDOFF_PACKAGE.md)
* [UE5 Endpoint Usage For UE Team](UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
* [UE5 EpisodeSpec Handoff Summary](UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md)
* [UE5 EpisodeSpec Controlled Smoke Result](UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md)
* [OpenAI-first Handoff Result](OPENAI_FIRST_HANDOFF_RESULT.md)
* [UE Team Message Draft](UE_TEAM_MESSAGE_DRAFT.md)
* [UE Handoff Delivery Manifest](UE_HANDOFF_DELIVERY_MANIFEST.md)
* [Handoff Release Notes](HANDOFF_RELEASE_NOTES.md)
* [Handoff Readiness Checklist](HANDOFF_READINESS_CHECKLIST.md)
* [Harness Warning Explanation](HARNESS_WARNING_EXPLANATION.md)

## 계약 및 생성 흐름 문서

* [OpenAI Provider Guide](OPENAI_PROVIDER_GUIDE.md)
* [OpenAI-first Handoff Result](OPENAI_FIRST_HANDOFF_RESULT.md)
* [Environment Parameter Spec](ENVIRONMENT_PARAMETER_SPEC.md)
* [Environment Sampler Design](ENVIRONMENT_SAMPLER_DESIGN.md)
* [JSON Contracts](JSON_CONTRACTS.md)
* [JSON Contract Validation Guide](JSON_CONTRACT_VALIDATION_GUIDE.md)
* [LLM World Config Generation Flow](LLM_WORLD_CONFIG_GENERATION_FLOW.md)
* [World Config Prompt Spec](WORLD_CONFIG_PROMPT_SPEC.md)
* [World Config Generation Orchestrator](WORLD_CONFIG_GENERATION_ORCHESTRATOR.md)

## 현재 상태 요약

* Policy knowledge card: 9개
* Policy RAG chunk: 9개
* UE 실행 계약: `EpisodeSpec`
* UE 권장 endpoint: `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`
* Controlled smoke: `providerUsed=openai`, `fallbackUsed=false`, `episodeValidationPassed=true`, `episodeScenarioReflectionPassed=true`, `ueCompilerReadiness=true`

## 검증 명령

```powershell
uv run python -m harness.checks.check_all
uv run pytest
```

현재 단계에서는 sample JSON과 fixture 파일을 의도적으로 자동 생성하지 않습니다.
