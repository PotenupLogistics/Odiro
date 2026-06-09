# Proto-AI 문서 인덱스

루트 [README.md](../README.md)는 프로젝트 최상위 entry point입니다. 이 문서는 정리된 `docs` 폴더 구조와 주요 문서 위치를 안내합니다.

## 공식 계약 문서

공식 계약 문서 경로는 팀 공유 경로이므로 이동하거나 rename하지 않습니다.

* [Episode JSON Guide](ue_contracts/EPISODE_JSON_GUIDE.md)
* [EpisodeSetup JSON](ue_contracts/EPISODE_SETUP_JSON.md)
* [DeliveryBotSetup JSON](ue_contracts/DELIVERY_BOT_SETUP_JSON.md)
* [RunQueue JSON](ue_contracts/RUN_QUEUE_JSON.md)
* [EvaluationReport JSON](ue_contracts/EVALUATION_REPORT_JSON.md)
* [Policy Decision JSON Guide](policy_server/POLICY_DECISION_JSON_GUIDE.md)

기준 경로:

* `docs/ue_contracts/EPISODE_SETUP_JSON.md`
* `docs/ue_contracts/DELIVERY_BOT_SETUP_JSON.md`
* `docs/ue_contracts/RUN_QUEUE_JSON.md`
* `docs/ue_contracts/EVALUATION_REPORT_JSON.md`
* `docs/policy_server/POLICY_DECISION_JSON_GUIDE.md`

## 아키텍처

* [Scenario / Episode 용어 정리](architecture/SCENARIO_EPISODE_TERMINOLOGY.md)
* [UE 계약 마이그레이션 계획](architecture/UE_CONTRACT_MIGRATION_PLAN.md)
* [맵 생성 데이터 근거](architecture/MAP_GENERATION_DATA_SOURCES.md)
* [맵 생성 trace](architecture/MAP_GENERATION_TRACE.md)
* [Route-relative Placement](architecture/ROUTE_RELATIVE_PLACEMENT.md)
* [Scenario Intent Extraction](architecture/SCENARIO_INTENT_EXTRACTION.md)
* [Scenario Post-Processing](architecture/SCENARIO_POST_PROCESSING.md)
* [Scenario Reflection Validation](architecture/SCENARIO_REFLECTION_VALIDATION.md)
* [Scenario Repair Prompt](architecture/SCENARIO_REPAIR_PROMPT.md)
* [LLM World Config Generation Flow](architecture/LLM_WORLD_CONFIG_GENERATION_FLOW.md)
* [World Config Prompt Spec](architecture/WORLD_CONFIG_PROMPT_SPEC.md)
* [World Config Prompt Hardening](architecture/WORLD_CONFIG_PROMPT_HARDENING.md)
* [World Config Output Contract](architecture/WORLD_CONFIG_OUTPUT_CONTRACT.md)
* [World Config Generation Orchestrator](architecture/WORLD_CONFIG_GENERATION_ORCHESTRATOR.md)

## 환경 파라미터

* [환경 파라미터 명세](environment/ENVIRONMENT_PARAMETER_SPEC.md)
* [환경 샘플러 설계](environment/ENVIRONMENT_SAMPLER_DESIGN.md)
* [환경 샘플러 생성 연동](environment/ENVIRONMENT_SAMPLER_GENERATION_INTEGRATION.md)

## 정책

* [MVP 정책 범위](policy/MVP_POLICY_SCOPE.md)
* [정책 출처 registry](policy/POLICY_SOURCE_REGISTRY.md)
* [정책 카드 coverage](policy/POLICY_CARD_COVERAGE.md)
* [정책 카드 생성 가이드](policy/POLICY_CARD_GENERATION_GUIDE.md)
* [정책 추출 matrix](policy/POLICY_EXTRACTION_MATRIX.md)
* [정책 파라미터 catalog](policy/POLICY_PARAMETER_CATALOG.md)
* [Decision action mapping](policy/DECISION_ACTION_MAPPING.md)
* [Decision request field mapping](policy/DECISION_REQUEST_FIELD_MAPPING.md)

## RAG와 출처 처리

* [RAG chunking 전략](rag/RAG_CHUNKING_STRATEGY.md)
* [RAG retrieval 전략](rag/RAG_RETRIEVAL_STRATEGY.md)
* [출처 처리 가이드](rag/SOURCE_PROCESSING_GUIDE.md)

## UE 전달

* [UE setup pair 전달 package](handoff/UE_SETUP_PAIR_HANDOFF_PACKAGE.md)
* [UE 팀 메시지 초안](handoff/UE_TEAM_MESSAGE_DRAFT.md)
* [UE5 팀 endpoint 사용 가이드](handoff/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
* [UE 연동 전달 인덱스](handoff/UE_INTEGRATION_HANDOFF_INDEX.md)
* [UE 전달 manifest](handoff/UE_HANDOFF_DELIVERY_MANIFEST.md)
* [전달 release notes](handoff/HANDOFF_RELEASE_NOTES.md)

RunQueue package export는 `scripts/export_ue5_run_queue_package.py`를 사용합니다. 산출물은 `data/run_queue_exports/` 아래 local ignored path에 저장하며, UE용 RunQueue JSON은 `docs/ue_contracts/RUN_QUEUE_JSON.md` 계약 필드만 포함합니다.

사용자용 scenario 생성 API는 `POST /api/v1/scenarios/generate`입니다. 입력은 자연어 `prompt`를 필수로 받고, 선택적으로 `episode_count`를 허용합니다. 성공 응답은 wrapper field 없는 RunQueue JSON입니다. `episode_count`를 생략하면 `SCENARIO_EPISODE_DEFAULT_COUNT`를 사용하고, 요청값은 1 이상 `SCENARIO_EPISODE_MAX_COUNT` 이하의 strict integer여야 합니다. 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하는 구조는 아닙니다. EpisodeSetup / DeliveryBotSetup / RunQueue JSON은 null-free 정책을 따르고 optional field는 값이 없으면 생략합니다.

로봇 실측 크기 W/D/H `0.44m / 1.00m / 0.64m`는 API request가 아니라 서버 기본 `RobotProfile`로 주입합니다. 생성/export된 EpisodeSetup에는 additive root field `robot_profile`이 포함되며, `min_passable_width_m=0.84m` 기준으로 보도 폭, 장애물 gap, robot spawn/goal 여유 검증에 사용합니다. UE가 아직 이 field를 소비하지 않으면 무시해도 되지만 collision box와 실제 크기 일치 여부는 UE 확인 항목입니다.

## Provider

* [OpenAI Provider 가이드](providers/OPENAI_PROVIDER_GUIDE.md)
* [Ollama Provider 가이드](providers/OLLAMA_PROVIDER_GUIDE.md)
* [Ollama live smoke 가이드](providers/OLLAMA_LIVE_SMOKE_GUIDE.md)
* [Ollama 실패 진단](providers/OLLAMA_FAILURE_DIAGNOSTICS.md)
* [Ollama timeout tuning 가이드](providers/OLLAMA_TIMEOUT_TUNING_GUIDE.md)
* [LLM provider 설정](providers/LLM_PROVIDER_CONFIGURATION.md)
* [LLM client abstraction](providers/LLM_CLIENT_ABSTRACTION.md)

## JSON 계약과 자연어

* [JSON 계약](json_contracts/JSON_CONTRACTS.md)
* [JSON 계약 검증 가이드](json_contracts/JSON_CONTRACT_VALIDATION_GUIDE.md)
* [자연어 입력 계획](json_contracts/NATURAL_LANGUAGE_INPUT_PLAN.md)
* [자연어 생성 계약](json_contracts/NATURAL_LANGUAGE_GENERATION_CONTRACT.md)

## 수동 검토

* [수동 확인 CLI](manual_review/MANUAL_CONFIRMATION_CLI.md)
* [수동 확인 가이드](manual_review/MANUAL_CONFIRMATION_GUIDE.md)
* [수동 확인 입력 가이드](manual_review/MANUAL_CONFIRMATION_INPUT_GUIDE.md)
* [수동 검토 실행 계획](manual_review/MANUAL_REVIEW_EXECUTION_PLAN.md)
* [수동 검토 queue](manual_review/MANUAL_REVIEW_QUEUE.md)

## 도구

* [API shell 가이드](tooling/API_SHELL_GUIDE.md)
* [하네스 가이드](tooling/HARNESS_GUIDE.md)
* [리포트 serialization 가이드](tooling/REPORT_SERIALIZATION_GUIDE.md)

## 연구와 참고 자료

* [연구 alignment](research/RESEARCH_ALIGNMENT.md)
* [연구 출처 registry](research/RESEARCH_SOURCE_REGISTRY.md)

Eureka / DrEureka 원문 PDF는 repository에 포함하지 않습니다. 공식 URL은 [연구 출처 registry](research/RESEARCH_SOURCE_REGISTRY.md)의 `RSR-005`, `RSR-006` 항목을 기준으로 확인하고, 필요한 경우 개인 로컬 보관 경로에만 둡니다. `docs/references/*.pdf`는 local-only 파일로 `.gitignore` 대상입니다.

## Archive

* [이전 EpisodeSpec archive](archive/previous_episode_spec/)
* [이전 handoff smoke/checklist archive](archive/deprecated/)
* [Legacy UE EpisodeSpec JSON guide](archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md)

Archive 문서는 현재 계약 문서가 아니라 과거 구현과 의사결정을 추적하기 위한 참고 자료다.

## 검증 명령

```powershell
uv run python -m harness.checks.check_all
uv run pytest
```

현재 단계에서는 sample JSON과 fixture 파일을 의도적으로 자동 생성하지 않습니다. Fine-tuning candidate full JSON은 로컬 전달 산출물로만 보관하며 repository에 포함하지 않습니다.
