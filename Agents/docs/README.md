# v2 Agent 문서 인덱스

이 문서는 현재 v2 Agent 기준으로 먼저 읽을 문서와 LLM context에 넣을 수 있는 문서를 구분한다. 과거 UE handoff, migration, research, deprecated 문서는 현재 계약이나 current architecture 기준으로 사용하지 않는다.

## 현재 기준 문서

* [Repository / user project 구조](../../docs/specs/project-structure.md): repository 전체 구조와 user project 폴더 배치 기준이다. 어떤 폴더와 파일이 어디에 있어야 하는지를 설명하며, 실행 순서와 파일 내부 schema의 canonical 기준은 아니다.
* [Simulation interface](../../docs/specs/simulation-interface.md): Scenario, EpisodeScenario, Run, Episode, RunId, EpisodeId 등 실행 용어와 실행 흐름의 canonical 기준이다.
* [User project data contract](../../contracts/specs/user-project-data.md): Bridge, Client, Agents가 함께 읽고 쓰는 `setting.json`, `profile.json`, `scenario.json`, `runs/<RunId>/**`, episode 결과 파일의 schema/field/rule 기준이다. 새 writer/API/schema는 이 문서를 우선한다.
* [v2 Agent API](api/V2_AGENT_APIS.md): v2 Agent HTTP API 기준이다.
* [v2 Agent Architecture](agents/V2_AGENT_ARCHITECTURE.md): v2 Agent 내부 구조 기준이다.
* [v2 Agent LangGraph Design](agents/V2_LANGGRAPH_DESIGN.md): v2 Agent LangGraph 흐름 기준이다.

`project-structure.md`와 `user-project-data.md`는 중복 문서가 아니다. 전자는 repository와 user project의 배치 책임을, 후자는 Bridge/Client/Agents가 공유하는 파일 형식 계약을 소유한다.

## v2 Scenario Generation API

사용자용 scenario 생성 API는 `POST /api/v2/scenarios/generate`이다. 입력은 자연어 `prompt`이며, 응답은 `<UserProject>/scenario.json`에 저장 가능한 `scenario` JSON이다. 실행 개수, seed, scenario sample, RunQueue 생성은 담당하지 않는다.

`POST /api/v1/scenarios/generate`는 현재 `410 RUN_QUEUE_REMOVED` 안내만 반환한다. legacy RunQueue package export는 public v2 API 경계가 아니라 과거 UE handoff tooling이다.

## 환경 문서

* [User project data contract](../../contracts/specs/user-project-data.md): 현재 user project 파일 형식 및 기본 환경 어휘 계약이다.
* `Client/Json/environment-catalog.md`: UE/Client 쪽 원본 catalog이다.
* [Environment catalog](environment/environment-catalog.md): `Client/Json/environment-catalog.md`를 v2 Agent LLM context에서 참고하기 위해 둔 임시 사본이다. surface id, prop id, prop category, prop class, persona id, encounter type, behavior override field를 참고한다.

`Agents/docs/environment/environment-catalog.md`만 LLM context allowlist에 넣는다. `Client/Json/environment-catalog.md`는 원본 위치이지만 Agent allowlist에 직접 넣지 않는다. 추후 `contracts/specs/environment-catalog.md` 또는 `contracts/specs/scenario-environment-catalog.md`가 canonical contract로 생기면 Agents 사본은 제거하고 contracts 쪽 문서를 참조한다.

## LLM Context Allowlist

v2 Agent에서 markdown 문서를 LLM context로 읽는 구조를 만들 때 기본 allowlist는 current 기준 문서만 포함한다.

```text
docs/specs/simulation-interface.md
contracts/specs/user-project-data.md
Agents/docs/api/V2_AGENT_APIS.md
Agents/docs/agents/V2_AGENT_ARCHITECTURE.md
Agents/docs/agents/V2_LANGGRAPH_DESIGN.md
Agents/docs/environment/environment-catalog.md
```

`Agents/docs/environment/environment-catalog.md`는 임시 AI-side catalog이다. contracts 하위 canonical environment catalog가 추가되면 allowlist에서 제거한다.

## LLM Context Exclusions

다음 문서는 현재 v2 Agent 기준 문서나 LLM context allowlist에 포함하지 않는다.

```text
Agents/docs/archive/**
contracts/specs/EpisodeSetup.json.md
contracts/specs/DeliveryBotSetup.json.md
contracts/specs/RunQueue.json.md
legacy UE handoff 문서
migration plan 문서
research 문서
deprecated 문서
```

legacy UE handoff 문서는 과거 구현과 의사결정 추적용으로만 보관한다. 현재 Scenario / Episode / Run 용어 기준은 [Simulation interface](../../docs/specs/simulation-interface.md)를 따른다.

## 개발/운영 참고

아래 문서는 current contract나 LLM allowlist가 아니라 개발자가 필요할 때 직접 확인하는 운영 참고 문서다.

* [v2 Agent 테스트/운영 가이드](development/V2_AGENT_TESTING_GUIDE.md)
* [Environment Parameter Spec](environment/ENVIRONMENT_PARAMETER_SPEC.md)
* [Capabilities](status/CAPABILITIES.md)
* [Verification Status](status/VERIFICATION_STATUS.md)
* [Next Actions](status/NEXT_ACTIONS.md)
* [Decisions](status/DECISIONS.md)
* [OpenAI Provider Guide](providers/OPENAI_PROVIDER_GUIDE.md)
* [Ollama Provider Guide](providers/OLLAMA_PROVIDER_GUIDE.md)
* [API Shell Guide](tooling/API_SHELL_GUIDE.md)
* [Harness Guide](tooling/HARNESS_GUIDE.md)

통합 harness 확인은 `uv run python -m harness.checks.check_all`을 사용한다.

## Legacy / Archive

아래 문서는 과거 UE handoff, migration, research, archive 추적용이다. 현재 v2 Agent 기준 문서나 LLM context allowlist에 포함하지 않는다.

* [Handoff Release Notes](handoff/HANDOFF_RELEASE_NOTES.md)
* [UE Integration Handoff Index](handoff/UE_INTEGRATION_HANDOFF_INDEX.md)
* [UE Handoff Delivery Manifest](handoff/UE_HANDOFF_DELIVERY_MANIFEST.md)
* [Map Generation Data Sources](architecture/MAP_GENERATION_DATA_SOURCES.md)
* [Map Generation Trace](architecture/MAP_GENERATION_TRACE.md)
* [Research Alignment](research/RESEARCH_ALIGNMENT.md)
* [Legacy UE EpisodeSpec JSON Guide](archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md)
