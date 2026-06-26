# v2 Agent LangGraph Design

## 1. 적용 이유

v2 Agent는 deterministic/rule-based 경로와 optional LLM 경로를 갖고 있습니다. LangGraph는 이 흐름을 node, edge, state, guardrail 단위로 명시해 분석 가능성과 단계별 테스트 가능성을 높이기 위한 구조입니다.

특히 scenario generation은 scenario 생성, 검증, repair, fallback이 분기형 workflow이고, result analysis는 workspace scan부터 RAG context, LLM recommendation validation까지 긴 pipeline입니다. Graph 표현은 각 단계의 입력/출력과 실패 처리 책임을 분리하는 데 유리합니다.

## 2. 적용 범위

Scenario generation v2는 `/api/v2/scenarios/generate`에서 항상 LangGraph runner를 사용합니다. `V2_AGENT_LLM_ENABLED=false`이면 deterministic node path만 사용하고, `true`이면 graph 내부 LLM-assisted node가 설정된 첫 LLM provider로 JSON 호출을 시도한 뒤 validator/repair/fallback을 거칩니다.

Result analysis v2는 `/api/v2/analysis/run`에서 항상 `ResultAnalysisGraphRunnerV2`를 사용합니다.

두 v2 endpoint 모두 provider chain을 순회하지 않습니다. 선택된 provider의 LLM 호출이 실패하면 scenario generation은 deterministic fallback, result analysis는 rule-based fallback으로 degraded 처리합니다.

주의: 일부 내부 node 이름에 남은 `template`은 legacy 구현 명칭입니다. 외부 계약과 저장 대상은 `scenario` JSON입니다.

## 3. ScenarioGenerationV2 graph 설계

```text
START
→ validate_request_node
→ interpret_user_prompt_node
→ select_scenario_pattern_node
→ build_scenario_template_node
→ validate_scenario_template_node
    ├─ valid → build_response_node → END
    ├─ repair → repair_scenario_template_node → validate_scenario_template_node
    └─ fallback → fallback_scenario_template_node → validate_scenario_template_node
```

## 4. ResultAnalysisV2 graph 설계

```text
START
→ scan_workspace_node
→ classify_artifacts_node
→ parse_artifacts_node
→ extract_episode_metrics_node
→ build_event_timelines_node
→ select_representative_failed_episodes_node
→ aggregate_runs_node
→ aggregate_experiments_node
→ detect_failure_patterns_node
→ route_analysis_need_node
    ├─ insufficient_data → build_insufficient_data_response_node → END
    ├─ no_change_needed → build_no_change_response_node → END
    └─ patterns_found → build_rag_query_node
→ retrieve_rag_context_node
→ build_analysis_context_node
→ llm_failure_analysis_node
→ generate_recommendations_node
→ validate_recommendations_node
→ route_recommendation_validation_node
    ├─ valid → build_response_node → END
    └─ invalid → rule_based_fallback_node → build_response_node → END
```

## 5. State 정의

`ScenarioGenerationGraphStateV2`는 request, prompt, parsed intent, selected pattern, optional LLM scenario candidate, generated scenario, validation result, response, warnings를 가진 state입니다.

`ResultAnalysisGraphStateV2`는 request, user project run root, artifacts, classified/parsed artifacts, parse warnings, episode metrics, timelines, representative failed episodes, run aggregates, failure patterns, RAG queries/context, analysis context, LLM analysis, recommendations, validation errors, response, warnings를 가집니다.

## 6. Node 목록

Scenario nodes:

* `validate_request_node`
* `interpret_user_prompt_node`
* `select_scenario_pattern_node`
* `build_scenario_template_node`
* `validate_scenario_template_node`
* `repair_scenario_template_node`
* `fallback_scenario_template_node`
* `build_response_node`

Analysis nodes:

* `scan_workspace_node`
* `classify_artifacts_node`
* `parse_artifacts_node`
* `extract_episode_metrics_node`
* `build_event_timelines_node`
* `select_representative_failed_episodes_node`
* `aggregate_runs_node`
* `aggregate_experiments_node`
* `detect_failure_patterns_node`
* `build_rag_query_node`
* `retrieve_rag_context_node`
* `build_analysis_context_node`
* `llm_failure_analysis_node`
* `generate_recommendations_node`
* `validate_recommendations_node`
* `rule_based_fallback_node`
* `build_response_node`

Scenario generation v2 runner는 실제 LangGraph `StateGraph`를 compile/invoke합니다. ResultAnalysisGraphRunnerV2도 graph-compatible node boundary를 유지합니다.

## 7. Edge와 조건 분기

Scenario validation 결과는 `valid`, `repair`, `fallback`으로 route합니다. repair는 제한 횟수 안에서만 validate node로 되돌아가며, 실패 시 deterministic fallback으로 이동합니다.

Analysis는 데이터 수와 패턴 유무로 먼저 route합니다. 데이터가 없으면 insufficient data response, 반복 패턴이 없으면 no-change response를 만듭니다. 반복 패턴이 있으면 RAG query/context를 구성하고 optional LLM 또는 rule-based recommendation 경로를 탑니다.

## 8. Tool 목록

* `RequestNormalizer`
* `IntentParser`
* `ScenarioTypeSelector`
* `TemplatePlanner`
* `TemplateJsonWriter`
* `TemplateValidator`
* `RepairHandler`
* `WorkspaceScanner`
* `ArtifactClassifier`
* `ArtifactParser`
* `EpisodeMetricExtractor`
* `EventTimelineBuilderV2`
* `RunAggregator`
* `ExperimentAggregator`
* `FailurePatternDetector`
* `RepresentativeFailedEpisodeSelectorV2`
* `RagQueryBuilderV2`
* `FileBasedRagRetrieverAdapterV2`
* `RagContextBuilderV2`
* `LlmFailureAnalyzer`
* `RecommendationGenerator`
* `RecommendationValidator`

## 9. Guardrail 목록

* v1 Agent/API는 변경하지 않습니다.
* Scenario generation v2는 항상 LangGraph runner를 사용합니다.
* Result analysis v2는 항상 `ResultAnalysisGraphRunnerV2`를 사용합니다.
* `/api/v2/scenarios/generate`의 외부 response body는 raw `scenario` v1 객체입니다.
* Scenario generation v2 structured output은 `corridor_pose`, fixed number/range value, Unreal-supported override fields, optional `allow_blocking`, optional background `spawn_zone`을 허용합니다.
* Robot `entry`/`exit`는 추상 anchor로 concrete pose field를 섞지 않으며, segment/along/offset이 필요하면 `corridor_pose`로 표현합니다.
* Obstacle placement는 Unreal parser와 맞춰 `fixed`, `pattern`, `scatter`를 허용합니다. 기본 생성은 단순한 `fixed` placement를 우선합니다.
* 문자열 choices는 `corridor.segments[].replaced_by`에만 제한적으로 허용합니다.
* raw log 전체를 LLM에 전달하지 않습니다.
* graph 내부 diagnostics, validation, generation mode는 외부 scenario generation response wrapper로 노출하지 않습니다.
* LLM 출력은 validator와 evidence validation을 통과해야 합니다.
* 실패 시 기존 deterministic/rule-based fallback을 사용합니다.

## 10. HITL 후보

* Project Scenario validation 실패가 반복될 때 repair 후보 승인
* blocked region violation 같은 safety-critical pattern의 원인 분류 확인
* RAG query keyword와 expected context 검토
* recommendation의 policy/environment target 확정
* Unreal schema 확정 전 신규 response field 추가 여부 판단

## 11. 실제 LangGraph 전환 계획

1. ScenarioGenerationV2 runner는 실제 `StateGraph` 경로를 기본으로 유지합니다.
2. `V2_AGENT_LLM_ENABLED`로 scenario generation graph 내부 LLM-assisted node 사용 여부를 제어합니다.
3. LLM generation은 `project_scenario_v1` structured output schema를 우선 사용하고, 모든 결과는 `TemplateValidator`를 통과해야 합니다.
4. ResultAnalysisV2 graph-compatible node pipeline을 기존 v2 response schema와 동등하게 검증합니다.
5. v2 response schema와 Unreal scenario schema가 확정되면 graph state 타입을 더 엄격하게 좁힙니다.
6. 운영 환경에서 LLM validator failure와 fallback warning 비율을 확인합니다.
