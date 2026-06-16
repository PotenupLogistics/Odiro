# v2 Agent Architecture

## 개요

v2 Agent는 기존 v1 실행 계약을 건드리지 않고, 시나리오 템플릿 생성과 실험 결과 분석을 분리합니다. Scenario generation v2는 항상 LangGraph runner를 사용하며, `V2_AGENT_LLM_ENABLED=true`일 때만 graph 내부 LLM-assisted JSON 호출 경로를 시도합니다. `V2_AGENT_GRAPH_ENABLED`는 scenario generation v2의 on/off switch가 아니며 결과 분석 v2 graph 경로 제어를 위해 유지합니다.

핵심 원칙:

* 수치 계산은 코드가 수행합니다.
* LLM은 해석과 추천만 담당합니다.
* raw log 전체를 LLM에 넣지 않습니다.
* LLM 출력은 반드시 validator를 통과해야 합니다.
* 실패 시 API 500 대신 fallback 응답과 warning을 반환합니다.

## ScenarioGenerationV2 흐름

```text
START
-> validate_request_node
-> interpret_user_prompt_node
-> select_scenario_pattern_node
-> build_scenario_template_node
-> validate_scenario_template_node
-> build_response_node
-> END
```

### `RequestNormalizer`

사용자 `prompt`를 trim하고 중복 공백을 정리합니다. v2 request schema는 `prompt`만 허용하므로 실행 개수 관련 필드는 FastAPI/Pydantic 단계에서 거부됩니다.

### `IntentParser`

자연어에서 환경 유형, 위험 요소, 주요 actor, 난이도 힌트를 추출합니다. 현재 MVP는 deterministic keyword 기반이며, LLM mode에서도 validator가 최종 안전장치입니다.

### `ScenarioTypeSelector`

의도에 따라 `straight_sidewalk`, `narrow_sidewalk`, `crosswalk`, `t_junction`, `obstacle_corridor` 중 기본 scenario type을 선택합니다. 애매한 입력은 보수적으로 sidewalk 계열로 해석합니다.

### `TemplatePlanner`

template에 들어갈 보도 폭, 장애물 수, 보행자 수, 속도 범위 같은 sampling 가능 값을 계획합니다. 좌표와 개수는 실행 샘플로 확정하지 않고 range로 유지합니다.

### `TemplateJsonWriter`

`TemplatePlan`을 `scenario.template.json` 형태의 dict로 변환합니다. 이 단계는 RunQueue, seed, episode sample을 생성하지 않습니다.

### `TemplateValidator`

template이 dict인지, `scenario_id`, `schema/version`, `intent.summary`, `ground_model`, `robot`, 장애물/보행자 조건을 갖는지 검증합니다. 중첩된 `{"min": ..., "max": ...}` 범위는 재귀적으로 `min <= max`를 확인합니다.

### `RepairHandler`

자동 보정 가능한 값만 다룹니다. 예를 들어 `scenario_id`를 snake_case로 정리하거나 뒤집힌 min/max를 swap합니다.

### `ResponseBuilder`

`scenario_generate_response_v2` wrapper를 만듭니다. `generation_mode`는 `deterministic`, `llm`, `llm_repaired`, `fallback` 중 하나입니다.

## ScenarioGenerationV2 LLM mode

LLM mode는 `V2_AGENT_LLM_ENABLED=true`일 때만 사용합니다. LangGraph의 `interpret_user_prompt_node`는 `prompts/system_prompt.md`와 `prompts/template_writer_prompt.md`를 읽고 JSON Schema structured output 기반 `scenario_template` 후보 출력을 요청합니다.

LLM output 처리 순서:

1. 가능하면 `scenario_template_v1` JSON Schema structured output으로 JSON object를 요청합니다.
2. `TemplateValidator`를 통과하면 LangGraph response의 template 후보로 사용합니다.
3. 검증 실패 시 deterministic repair를 먼저 적용합니다.
4. 그래도 invalid이면 validator errors, 원본 prompt, invalid template을 포함한 LLM-assisted repair를 1회 시도합니다.
5. repair 결과가 `TemplateValidator`를 통과하면 repaired template을 사용합니다.
6. repair도 실패하면 warning을 남기고 deterministic graph path/fallback을 사용합니다.
7. Scenario generation v2 response는 graph 실행 결과이므로 `generation_mode="langgraph"`를 유지합니다.

## ResultAnalysisV2 흐름

```text
WorkspaceScanner
-> ArtifactClassifier
-> ArtifactParser
-> EpisodeMetricExtractor
-> EventTimelineBuilderV2
-> RunAggregator
-> ExperimentAggregator
-> FailurePatternDetector
-> RagQueryBuilderV2
-> AnalysisContextBuilder
-> LlmFailureAnalyzer
-> RecommendationGenerator
-> RecommendationValidator
-> ResponseBuilder
```

### `WorkspaceScanner`

configured experiments root 하위 파일을 스캔합니다. 숨김 파일과 과도하게 큰 파일은 제외하고 warning으로 기록합니다.

### `ArtifactClassifier`

파일 경로와 이름으로 artifact type을 분류합니다. 지원 타입은 `experiment_setting`, `experiment_profile`, `experiment_policy_config`, `experiment_policy_source`, `scenario_sample`, `run_summary`, `run_policy_snapshot`, `episode_result`, `episode_events`, `episode_actions`, `episode_trace`, `episode_preview`, `episode_capture`, `unknown`입니다.

### `ArtifactParser`

JSON은 `json.loads`, JSONL은 line-by-line 파싱합니다. 깨진 JSONL line은 warning으로 남기고 나머지 line은 사용합니다. 이미지 파일은 내용 분석 없이 metadata만 기록합니다.

### `EpisodeMetricExtractor`

episode별 success, failure, collision, near miss, blocked/penalty region violation, timeout, duration 같은 지표를 코드로 계산합니다. 값이 없으면 count는 0, 알 수 없는 값은 null로 둡니다.

### `EventTimelineBuilderV2`

episode event/action artifact에서 핵심 event timeline을 정규화합니다. `event_type`, `type`, `name` 같은 입력 차이를 흡수하고, collision, near miss, blocked region violation, timeout 같은 key event만 내부 analysis context에 포함합니다. 최종 response schema는 변경하지 않습니다.

### `RunAggregator`

episode metric을 run 단위로 집계합니다. `episode_count`, `success_count`, `failure_count`, `success_rate`, 주요 failure type을 계산합니다.

### `ExperimentAggregator`

run summary를 experiment 단위로 묶습니다. 전체 success rate와 main failure patterns를 계산하고, 여러 run이 있으면 간단한 trend를 기록합니다.

### `FailurePatternDetector`

반복 `blocked_region_violation`, `near_miss`, `collision`, `timeout`, `goal_not_reached` 패턴을 rule-based로 탐지합니다. 반복 기준은 같은 유형이 2개 이상 확인되는 것입니다.

### `RagQueryBuilderV2`

반복 실패 패턴을 RAG 검색 query 후보로 변환합니다. 예를 들어 `blocked_region_violation_repeated`는 `policy_safety`, `near_miss_repeated`는 `pedestrian_safety` query로 매핑합니다. 현재 retriever adapter는 vector DB 없이 실패하지 않는 skeleton이며, query와 빈 context만 내부 analysis context에 제공합니다.

### `AnalysisContextBuilder`

LLM에 전달 가능한 요약 context를 만듭니다. 모든 raw log를 넣지 않고 workspace summary, experiment summaries, run summaries, failure patterns 중심으로 구성합니다.

### `LlmFailureAnalyzer`

현재 MVP에서는 LLM 연결 전후 공통 형태를 맞추기 위한 얇은 분석 단계입니다. 수치 계산은 하지 않고, 추천 생성에 필요한 해석 payload만 제공합니다.

### `RecommendationGenerator`

rule-based recommendation을 생성합니다. 반복 실패 패턴이 없으면 빈 배열을 반환합니다.

### `RecommendationValidator`

recommendation target이 `policy` 또는 `environment`인지 확인하고, evidence가 실제 experiment/run/episode를 참조하는지 검증합니다. 잘못된 LLM evidence는 반영하지 않습니다.

### `ResponseBuilder`

`analysis_run_response_v2` wrapper를 만듭니다. `modified_policy_json`과 `modified_environment_json`은 recommendations 기반으로 코드가 생성합니다.

## ResultAnalysisV2 LLM mode

LLM mode는 summarized context만 전달합니다.

전달 대상:

* workspace summary
* experiment summaries
* run aggregates
* detected failure patterns
* representative evidence summary
* warnings summary

전달하지 않는 대상:

* full `events.jsonl`
* full `actions.jsonl`
* full `trace.jsonl`
* image/capture binary content

LLM이 반환하는 것은 final response 전체가 아니라 recommendation/summary 중심 JSON입니다. metrics와 modified JSON 후보는 코드가 생성합니다.

## fallback 설계

Scenario generation에서 LLM output 또는 repair output이 검증 실패하면 deterministic fallback을 사용합니다.

Analysis에서 LLM 호출, JSON 파싱, recommendation validation, evidence validation이 실패하면 rule-based fallback을 사용합니다.

fallback은 정상적인 degradation path입니다. API는 success response를 유지하고, warning으로 원인을 노출합니다.

## LangGraph runner

`ScenarioGenerationGraphRunnerV2`는 실제 LangGraph `StateGraph`를 compile/invoke하는 scenario generation v2 기본 실행 경로입니다. `ResultAnalysisGraphRunnerV2`는 graph-compatible node pipeline으로 확장되어 scan, classify, parse, metric extraction, timeline/RAG context, recommendation validation, response build를 node 메서드 단위로 실행합니다.

`langgraph` import가 실패해도 module import와 테스트가 깨지지 않도록 `StateGraph = None` fallback을 사용합니다. ResultAnalysisV2 graph runner는 실제 `langgraph` dependency 없이도 순차 node pipeline으로 동작합니다. graph mode가 꺼져 있으면 기존 `ResultAnalysisV2Agent` 경로를 그대로 사용합니다.

graph mode true에서도 final response는 기존 `analysis_run_response_v2` schema를 유지합니다. episode timeline, representative failed episode, RAG query/context는 내부 `analysis_context`와 runner state에만 존재하며 API response field로 추가하지 않습니다.
