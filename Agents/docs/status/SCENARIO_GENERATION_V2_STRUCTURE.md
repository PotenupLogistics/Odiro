# Scenario Generation v2 Structure

## 1. 역할

Scenario Generation v2는 사용자 자연어 `prompt` 하나를 `scenario` v1 JSON 객체로 변환해 외부 API response body로 직접 반환한다.

이 기능은 template 파일 관리, experiment 설정 반영, seed/sample 확정, Unreal 실행 payload 생성, RunQueue 생성, 결과 분석을 담당하지 않는다.

주요 런타임 진입점은 다음 두 가지다.

- FastAPI endpoint: `app/api/routes.py::scenario_generate_v2_endpoint()`
- LangGraph runner: `app/agents/scenario_generation_v2/graph_runner.py::ScenarioGenerationGraphRunnerV2`

## 2. FastAPI endpoint

Endpoint:

```text
POST /api/v2/scenarios/generate
```

Request model:

```text
app.models.scenario_generation_v2.ScenarioGenerateV2Request
```

Response model:

```text
app.models.scenario_generation_v2.ProjectScenarioV1Response
```

Routing:

```text
scenario_generate_v2_endpoint()
→ Settings()
→ ScenarioGenerationGraphRunnerV2(settings=settings).run(request)
→ response.scenario 추출
→ ProjectScenarioV1Response
```

`/api/v2/scenarios/generate`는 v2 LangGraph workflow를 기본 실행 경로로 사용한다. LangGraph import가 불가능한 환경에서는 runner 내부에서만 기존 sequential agent로 fallback한다.

`V2_AGENT_LLM_ENABLED=false`이면 LangGraph 내부 deterministic parser/selector/builder 경로만 사용한다. `V2_AGENT_LLM_ENABLED=true`이면 `interpret_user_prompt_node`에서 JSON Schema structured output 기반 LLM-assisted 후보 scenario 생성을 먼저 시도하고, validator를 통과한 경우에만 그 후보를 사용한다. 이 경우 deterministic pattern selection은 건너뛴다. LLM 호출 실패 또는 LLM output validation 실패 시 deterministic graph path로 fallback한다. `generation_mode`는 내부 graph response metadata로만 유지하고 외부 API response body에는 노출하지 않는다.

LLM prompt는 validator가 요구하는 최소 `scenario` v1 구조를 직접 제시한다. 특히 `corridor.axis`, `corridor.walkway_width_m`, `corridor.segments`, object형 `obstacles`, `pedestrians.encounters`, `robot.start`, `robot.goal`을 명시해 WorldConfig-style output이 나오지 않도록 한다.

Structured output schema는 `app/agents/scenario_generation_v2/scenario_template_schema.py`에 있다. 이 schema는 root required fields, corridor/obstacles/pedestrians/robot 최소 구조, surface/placement/encounter/persona enum을 제한한다. Segment id cross-reference 같은 관계 검증은 계속 `TemplateValidator`가 담당한다.

주의: `scenario_template_schema.py`, `TemplateValidator`는 legacy 내부 이름입니다. 외부 API와 저장 파일 계약은 `scenario` JSON입니다.

현재 schema는 Project Scenario v1 계약에 맞춰 robot abstract anchor(`entry`/`exit`)와 concrete anchor(`corridor_pose`)를 구분하고, 고정 숫자 또는 `{min,max}` 범위값, fixed/pattern/scatter obstacle placement, Unreal-supported override fields, placement `allow_blocking`, background `spawn_zone.segments`, 제한적 `corridor.segments[].replaced_by` choices를 허용한다. 기본 LLM 생성은 단순한 `fixed` placement를 우선한다.

직선/커브/공사구간/S자 길 prompt는 `ScenarioPresetRegistry`가 `blank`, `line`, `curved`, `barricade`, `s-curve` canonical preset 중 하나로 해석할 수 있다. legacy id는 loader가 아니라 registry에서 `curved-road -> curved`, `demo -> line`으로 resolve한다. preset은 `ScenarioPresetLoader`의 optional load 결과를 `ScenarioPresetPatcher`가 deepcopy 후 사용자 intent로 patch하고, `TemplateValidator` 검증 실패 시 deterministic repair 후 재검증한다. preset 누락, 파싱 실패, patch/validation 실패는 API 500이 아니라 기존 writer 기반 scenario fallback으로 처리한다. 이 처리는 bundled preset을 generation seed로 읽는 것이며, scenario 파일 저장이나 template 관리 책임을 추가하지 않는다.

보행자 요청은 현재 alpha 정책상 외부 scenario body에서 실행용 보행자 경로로 확장하지 않는다. 응답 root에는 `pedestrians`를 포함하되 `background.count=0`, `background.speed_mps=1.0`, `encounters=[]`로 정규화한다.

## 3. Request / Response 구조

Request는 `prompt`만 받는다. Pydantic model은 `extra="forbid"`이므로 `experiment_id`, `run_id`, `project_id`, `sample_count`, `episode_count`, `base_seed`, `seed`, `current_template`, `mode` 같은 필드는 허용하지 않는다.

```json
{
  "prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"
}
```

Response body는 wrapper 없이 `scenario` v1 root object 자체를 반환한다.

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "pinch_oncoming_pass",
  "intent": "협폭 구간에서 마주 오는 보행자와 조우할 때 로봇이 안전하게 감속, 양보, 통과하는지 검증한다.",
  "corridor": {},
  "obstacles": {},
  "pedestrians": {},
  "robot": {}
}
```

Response에는 `status`, `summary`, `template`, `validation`, `assumptions`, `generation_mode`, `template_path`, `scenario_path`, `sample_id`, `generated_count`, `scenario_sample`을 넣지 않는다.

## 4. 실제 LangGraph StateGraph 흐름

`ScenarioGenerationGraphRunnerV2`는 LangGraph가 import 가능하면 초기화 시 `StateGraph(ScenarioGenerationGraphStateV2)`를 구성하고 `compile()`을 호출한다. 실행 시에는 `compiled_graph.invoke()`를 호출한다.

Graph:

```text
START
→ validate_request_node
→ interpret_user_prompt_node
→ select_scenario_pattern_node
→ build_scenario_template_node
→ validate_scenario_template_node
→ build_response_node
→ END
```

Conditional edges:

```text
validate_scenario_template_node
├─ valid    → build_response_node
├─ repair   → repair_scenario_template_node → validate_scenario_template_node
└─ fallback → fallback_scenario_template_node → validate_scenario_template_node
```

Fallback to the old sequential agent occurs only when `langgraph.graph.StateGraph` cannot be imported. That fallback calls `ScenarioGenerationV2Agent.generate(request)`.

## 5. State 구조

`ScenarioGenerationGraphStateV2`는 prompt-only workflow에 필요한 상태만 담는다.

Included:

- `request`
- `prompt`
- `interpreted_intent`
- `selected_pattern`
- `llm_template_candidate`
- `llm_validation`
- `llm_warnings`
- `scenario`
- `validation`
- `diagnostics`
- `repair_count`
- `status`
- `summary`
- `assumptions`
- `response`
- `output`

Excluded:

- `experiment_id`
- `run_id`
- `experiment_root`
- `setting`
- `profile`
- `template_candidates`
- `scenario_samples`
- `sampled_params`
- `base_seed`
- `sample_count`

## 6. Node별 역할

| Node | 실제 함수/class | 유형 | 입력 State | 출력 State | 역할 |
| --- | --- | --- | --- | --- | --- |
| `validate_request_node` | `ScenarioGenerationGraphRunnerV2.validate_request_node` | Guardrail / validator | `request`, `prompt` | `prompt`, optional failed `validation` | prompt 존재와 문자열/blank 여부를 확인한다. |
| `interpret_user_prompt_node` | `ScenarioGenerationGraphRunnerV2.interpret_user_prompt_node` | LLM-assisted / Deterministic parser | `prompt` | normalized `prompt`, optional `interpreted_intent`, optional `llm_template_candidate`, `llm_validation`, `llm_warnings` | `V2_AGENT_LLM_ENABLED=true`이면 prompt 정규화 직후 LLM 후보 생성을 먼저 시도한다. 유효한 후보가 없을 때만 기존 `IntentParser`로 자연어 의도를 구조화한다. |
| `select_scenario_pattern_node` | `ScenarioGenerationGraphRunnerV2.select_scenario_pattern_node` | Deterministic selector | optional `interpreted_intent`, optional `llm_template_candidate` | `selected_pattern` | 유효한 LLM 후보가 있으면 후보 scenario id를 선택값으로 사용한다. 없으면 기존 `ScenarioTypeSelector`로 지원 패턴 중 하나를 선택한다. |
| `build_scenario_template_node` | `ScenarioGenerationGraphRunnerV2.build_scenario_template_node` | Scenario builder | `interpreted_intent`, `selected_pattern`, optional validated `llm_template_candidate` | `scenario`, `summary`, `assumptions` | validator를 통과한 LLM 후보가 있으면 사용하고, 없으면 기존 planner/writer로 deterministic `scenario` v1 객체를 만든다. 최종 확정 전 agent postprocess에서 optional preset load/patch/validate/fallback 흐름을 적용한다. |
| `validate_scenario_template_node` | `ScenarioGenerationGraphRunnerV2.validate_scenario_template_node` | Guardrail / validator | `scenario` | `validation`, `diagnostics`, `status` | 기존 `TemplateValidator`로 schema, 참조, catalog, forbidden field를 검사한다. |
| `repair_scenario_template_node` | `ScenarioGenerationGraphRunnerV2.repair_scenario_template_node` | Repair | invalid `scenario`, `repair_count` | repaired `scenario`, incremented `repair_count`, diagnostics | 기존 `RepairHandler`로 deterministic repair를 적용한다. 최대 2회 경로만 허용된다. |
| `fallback_scenario_template_node` | `ScenarioGenerationGraphRunnerV2.fallback_scenario_template_node` | Fallback | invalid state after repair attempts | fallback `scenario`, `validation`, `assumptions` | `narrow_sidewalk_cross_path` deterministic fallback scenario를 생성한다. |
| `build_response_node` | `ScenarioGenerationGraphRunnerV2.build_response_node` | Response builder | final `scenario`, `validation`, `summary`, `assumptions` | internal `response`, `output` | graph 내부 결과와 diagnostics를 담은 internal response를 만든다. FastAPI boundary에서는 이 중 `scenario`만 외부 raw `scenario` response로 반환한다. |

테스트 환경의 재현성 검증은 `V2_AGENT_LLM_ENABLED=false`로 실행되어 deterministic parser/selector/builder/validator path만으로 통과한다. LLM enabled 경로는 mock LLM client로 graph node 내부 호출과 invalid output fallback을 검증한다.

## 7. Validation / repair / fallback 흐름

Validation checks include:

- `schema == "scenario"`
- `version == 1`
- `scenario_id` snake_case
- `intent`, `corridor`, `robot` 존재
- `corridor.axis.type == "polyline"`
- fixed number 또는 `{min,max}` 범위값
- corridor segment id unique
- obstacle placement id unique
- obstacle placement kind-specific required fields for `fixed`, `pattern`, `scatter`
- pedestrian encounter id unique
- placement/encounter/robot `corridor_pose` segment 참조 무결성
- `corridor_pose.along_m` segment range 검사
- `entry`/`exit` robot anchor에 concrete pose field 혼합 금지
- LLM output의 mixed abstract anchor는 repair 단계에서 `corridor_pose`로 보정하거나 concrete field를 제거
- background `spawn_zone.segments` segment 참조 무결성
- supported override fields: `cooperation`, `evasiveness`, `personal_space_m`, `awareness_horizon_s`, `max_yield_wait_s`, `sidestep_distance_m`
- optional placement `allow_blocking` boolean
- `corridor.segments[].replaced_by` string 또는 `{choices:[...]}`
- legacy fields 금지: `ground_model`, `static_obstacles`, `pedestrians.path`
- ownership/runtime fields 금지: `experiment_id`, `run_id`, `sample_count`, `base_seed`, `seed`, `sample_id`, `scenario_path`, `template_path`, `generated_count`, `ue_payload`
- policy/robot setup field 금지: `policy`, `robot_setup`, `robot.setup`

Repair first applies deterministic local repair. It normalizes `scenario_id`, migrates legacy `template_id` to `scenario_id`, removes `template_id`, and swaps inverted `min/max` ranges. If LLM output remains invalid and LLM repair is enabled, the graph sends the original prompt, invalid scenario, and validator errors to an LLM-assisted repair call using the same structured output schema. Only repair results that pass `TemplateValidator` are used. If repair still fails, deterministic fallback scenario generation runs.

Preset 기반 scenario 후보도 같은 validator gate를 사용한다. 흐름은 `load -> deepcopy -> patch -> validate -> repair if needed -> re-validate -> response`이며, repair 후에도 `TemplateValidator`를 통과하지 못하면 preset 후보를 버리고 기존 `TemplateJsonWriter` 기반 fallback scenario를 사용한다. 외부 `/api/v2/scenarios/generate` response body는 항상 raw `scenario` JSON 계약을 유지하며 preset diagnostic field를 추가하지 않는다.

## 8. 하지 않는 일

Scenario Generation v2 does not:

- accept seed, base_seed, or sample_count as API inputs
- load `experiments/<ExperimentId>/setting.json`
- load `experiments/<ExperimentId>/profile.json`
- load legacy `templates/scenarios/*.template.json`
- save scenario files
- create `scenario_sample`
- apply `sample_count`, `base_seed`, or `seed`
- require `experiment_id` or `run_id`
- create UE runtime payload
- modify result analysis agent logic
- modify RAG or policy recommendation logic
- change the v1 scenario generation endpoint

## 9. 테스트 결과

Reproducibility:

- Scenario Generation v2 does not accept `seed`, `base_seed`, or `sample_count`.
- These fields are not part of the request model, external response model, or generated scenario.
- The deterministic parser/selector/builder path is tested to return the same raw `scenario` for the same prompt.
- Tests do not depend on LLM output; graph-path reproducibility tests run with `V2_AGENT_LLM_ENABLED=false`.

Latest verification commands:

```text
uv run pytest tests/test_v2_graph_settings.py tests/test_llm_provider_settings.py tests/test_v2_scenario_generation_api.py -q
uv run pytest tests/test_v2_analysis_run_api.py tests/test_v2_result_analysis_graph_runner.py -q
uv run pytest tests/test_v2_graph_settings.py -q
uv run ruff check app tests
uv run pytest -q
```

Latest observed results:

```text
56 passed, 1 warning
24 passed, 1 warning
3 passed
All checks passed!
744 passed, 1 warning
```

Latest OpenAI smoke:

```text
V2_AGENT_LLM_ENABLED=true
POST /api/v2/scenarios/generate
→ HTTP 200 / raw scenario v1 response
→ LLM candidate validator valid true
→ structured output schema used by scenario LLM call
→ final validation.valid true
→ fallback warning 없음
```
