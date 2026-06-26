# v2 Agent Testing And Operations Guide

## 1. v2 API 테스트 명령

```powershell
uv run pytest tests/test_v2_scenario_generation_api.py tests/test_v2_analysis_run_api.py -q
uv run pytest tests/test_v2_result_analysis_graph_runner.py -q
uv run pytest tests/test_v2_result_analysis_timeline_builder.py tests/test_v2_rag_query_builder.py -q
```

이 테스트는 v2 request/response 계약, deterministic/rule-based 기본 경로, fake/mock client 기반 LLM 경로, fallback warning을 검증합니다.

## 2. v1 회귀 테스트 명령

```powershell
uv run pytest tests/test_scenario_generation_api.py tests/test_analysis_run_endpoint.py -q
```

v2 작업 후에도 v1 scenario generation과 v1 analysis endpoint가 유지되는지 확인합니다.

## 3. 전체 테스트 명령

```powershell
uv run ruff check app tests
uv run pytest -q
```

문서 중심 변경이어도 전체 회귀를 실행합니다.

## 4. LLM disabled 기본 테스트 방법

기본 설정은 아래와 같습니다.

```text
V2_AGENT_LLM_ENABLED=false
```

이 상태에서는 외부 LLM provider를 호출하지 않습니다.

* scenario generation v2는 항상 LangGraph runner를 사용하며 deterministic node path로 template을 생성합니다.
* analysis는 rule-based failure pattern detector와 recommendation generator를 사용합니다.
* 외부 API key 없이 테스트가 통과해야 합니다.

## 5. fake/mock client 기반 LLM 경로 테스트

LLM mode 테스트는 실제 provider를 호출하지 않고 fake JSON client를 agent에 주입합니다.

검증 대상:

* valid LLM JSON output
* Markdown JSON code block parsing
* invalid scenario 후 repair 성공
* LLM generation/repair 모두 실패 후 fallback
* LLM recommendation evidence validation
* invalid recommendation 후 rule-based fallback

## 6. 외부 API key 없이 테스트가 통과해야 하는 이유

v2 LLM mode는 optional 기능입니다. CI와 로컬 개발 기본 경로는 deterministic/rule-based여야 하며, 네트워크 상태나 provider key 존재 여부가 기본 테스트 성공 여부를 좌우하면 안 됩니다.

운영에서 LLM을 켤 때만 provider 설정과 API key를 준비합니다.

## 7. analysis run 입력

`/api/v2/analysis/run`은 사용자가 직접 만든 project root와 run id를 요청 body로 받습니다. 자동 생성되는 기본 root나 environment variable을 사용하지 않습니다.

요청 예:

```json
{
  "project_path": "D:/OdiroProjects/DeliveryBotBaseline",
  "run_id": "000001"
}
```

## 8. LLM mode 운영 설정

```text
V2_AGENT_LLM_ENABLED=true
V2_AGENT_LLM_REPAIR_ENABLED=true
V2_AGENT_LLM_MAX_REPAIR_ATTEMPTS=1
```

LLM mode를 켜면 scenario generation v2는 `project_scenario_v1` JSON Schema structured output을 우선 사용합니다. 그래도 API는 fallback 정책을 유지합니다. LLM 호출 실패, JSON 파싱 실패, validator 실패, LLM-assisted repair 실패는 내부 fallback 경로로 처리한 뒤 raw `scenario` response를 반환합니다.

Scenario generation contract tests cover raw `scenario` response shape, abstract `entry`/`exit` robot anchors, concrete `corridor_pose` robot anchors, mixed-anchor repair, fixed number or min/max range values, fixed/pattern/scatter obstacle placements, Unreal-supported persona override fields, optional placement `allow_blocking`, optional background `spawn_zone` segment references, optional `meet_offset_m`, and limited `corridor.segments[].replaced_by` string choices.

## 9. Runner 운영 확인

Scenario generation v2는 항상 LangGraph runner를 사용합니다. ResultAnalysisV2도 항상 `ResultAnalysisGraphRunnerV2`를 사용하며, `langgraph`가 설치되지 않은 환경에서도 graph-compatible node pipeline으로 동작해야 합니다. response schema는 기존 `analysis_run_response_v2`를 유지해야 합니다.

```powershell
uv run pytest tests/test_v2_result_analysis_graph_runner.py -q
```

이 테스트는 import 가능성, insufficient data response, 반복 blocked region violation recommendation, API response schema, timeline/RAG 내부 state 유지와 response schema 비노출을 검증합니다.

## 10. timeline/RAG builder 테스트

timeline builder는 event field 정규화, key event filtering, representative failed episode selection을 검증합니다. RAG query builder는 반복 실패 pattern과 query type mapping을 검증합니다.

```powershell
uv run pytest tests/test_v2_result_analysis_timeline_builder.py tests/test_v2_rag_query_builder.py -q
```

## 11. 운영 확인 체크리스트

1. `/health`가 200을 반환하는지 확인합니다.
2. `/api/v2/scenarios/generate`가 prompt-only request로 200을 반환하는지 확인합니다.
3. 실행 개수 필드가 422로 거부되는지 확인합니다.
4. `/api/v2/analysis/run`이 `project_path`, `run_id` 없이 호출되면 422를 반환하는지 확인합니다.
5. 데이터가 없을 때 `overall_judgement="insufficient_data"`인지 확인합니다.
6. LLM mode를 켠 경우 scenario generation은 raw `scenario` root field를, analysis는 `analysis_mode`와 `warnings`를 함께 확인합니다.
