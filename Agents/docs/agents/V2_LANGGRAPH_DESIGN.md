# v2 Agent LangGraph Design

## 1. 적용 이유

v2 Agent는 이미 deterministic/rule-based 기본 경로와 optional LLM fallback 구조를 갖고 있습니다. LangGraph는 이 흐름을 node, edge, state, guardrail 단위로 명시해 분석 가능성과 단계별 테스트 가능성을 높이기 위한 후보입니다.

특히 scenario generation은 template 생성, 검증, repair, fallback이 분기형 workflow이고, result analysis는 workspace scan부터 RAG context, LLM recommendation validation까지 긴 pipeline입니다. Graph 표현은 각 단계의 입력/출력과 실패 처리 책임을 분리하는 데 유리합니다.

## 2. optional로 두는 이유

현재 v2 API의 기본 동작은 이미 배포 가능한 형태입니다. LangGraph를 필수 dependency로 추가하면 설치/CI/운영 환경이 바뀌고, 아직 확정되지 않은 response JSON 및 Unreal `scenario.template.json` schema를 과도하게 고정할 위험이 있습니다.

따라서 `V2_AGENT_GRAPH_ENABLED=false`를 기본값으로 둡니다. ResultAnalysisV2 runner는 node 메서드 기반 graph-compatible pipeline으로 확장되었지만, `langgraph` 자체는 여전히 optional dependency입니다. graph mode가 꺼져 있으면 기존 v2 pipeline 결과가 그대로 유지되어야 합니다.

## 3. ScenarioGenerationV2 graph 설계

```text
START
→ normalize_prompt_node
→ classify_intent_node
→ select_scenario_type_node
→ plan_template_node
→ generate_template_json_node
→ validate_template_node
→ route_validation_node
    ├─ valid → build_response_node → END
    ├─ invalid_and_repairable → repair_template_node → validate_template_node
    └─ failed → deterministic_fallback_node → build_response_node → END
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

`ScenarioGenerationGraphStateV2`는 request, normalized prompt, parsed intent, selected scenario type, template plan, generated template, validation result, response, warnings를 가진 느슨한 state입니다.

`ResultAnalysisGraphStateV2`는 request, experiments root, artifacts, classified/parsed artifacts, parse warnings, episode metrics, timelines, representative failed episodes, run/experiment aggregates, failure patterns, RAG queries/context, analysis context, LLM analysis, recommendations, validation errors, response, warnings를 가집니다.

## 6. Node 목록

Scenario nodes:

* `normalize_prompt_node`
* `classify_intent_node`
* `select_scenario_type_node`
* `plan_template_node`
* `generate_template_json_node`
* `validate_template_node`
* `repair_template_node`
* `deterministic_fallback_node`
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

현재 `ResultAnalysisGraphRunnerV2`는 위 node들을 Python 메서드로 순차 실행합니다. 이 구조는 나중에 `StateGraph`에 연결할 수 있도록 node 이름과 state update boundary를 유지합니다.

## 7. Edge와 조건 분기

Scenario validation 결과는 `valid`, `invalid_and_repairable`, `failed`로 route합니다. repair는 제한 횟수 안에서만 validate node로 되돌아가며, 실패 시 deterministic fallback으로 이동합니다.

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
* LangGraph dependency는 optional입니다.
* graph mode 기본값은 `false`입니다.
* raw log 전체를 LLM에 전달하지 않습니다.
* response JSON 최종 계약처럼 field를 고정하지 않습니다.
* LLM 출력은 validator와 evidence validation을 통과해야 합니다.
* 실패 시 기존 deterministic/rule-based fallback을 사용합니다.

## 10. HITL 후보

* scenario template validation 실패가 반복될 때 repair 후보 승인
* blocked region violation 같은 safety-critical pattern의 원인 분류 확인
* RAG query keyword와 expected context 검토
* recommendation의 policy/environment target 확정
* Unreal schema 확정 전 신규 response field 추가 여부 판단

## 11. 실제 LangGraph 전환 계획

1. `V2_AGENT_GRAPH_ENABLED=false` 기본값을 유지합니다.
2. ResultAnalysisV2 graph-compatible node pipeline을 기존 v2 response schema와 동등하게 검증합니다.
3. LangGraph 설치 환경에서만 실제 `StateGraph` integration test를 별도 marker로 실행합니다.
4. ScenarioGenerationV2 runner도 같은 node pipeline 방식으로 확장합니다.
5. v2 response schema와 Unreal template schema가 확정되면 graph state 타입을 더 엄격하게 좁힙니다.
6. 운영 환경에서 canary 방식으로 graph mode를 켜고 warning/fallback 비율을 확인합니다.
