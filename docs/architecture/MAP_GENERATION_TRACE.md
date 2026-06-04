# Map Generation Trace

## 1. 목적

시나리오 생성 API가 어떤 근거로 맵과 좌표를 생성했는지 추적한다.

## 2. 왜 필요한가

* LLM이 임의로 만든 값인지 확인
* 좌표/보도폭/장애물 위치의 근거 설명
* 강사님/UE팀에게 생성 근거 설명 가능
* 디버깅과 재현성 확보

## 3. trace에 남기는 근거

* user_prompt
* scenario_intent
* environment_sampling
* policy_rag
* placement_rule
* post_processing
* episode_spec_adapter
* validation
* scenario_reflection

## 4. 법령 RAG의 역할

법령 RAG는 좌표 생성 근거가 아니라 safety/policy context로 사용한다.

## 5. 좌표 근거

좌표는 아래에서 온다.

* 사용자가 명시한 좌표
* environmentSampling 수치
* route midpoint 같은 deterministic placement rule
* EpisodeSpec adapter의 cm -> m 변환

## 6. 저장 금지 항목

* API key
* rawContent
* full WorldConfig
* full EpisodeSpec

## 7. API 응답 위치

`includeDiagnostics=true`인 handoff 응답은 `diagnostics.generationTrace`에 summary evidence를 포함한다.
`includeDiagnostics=false`인 경우 trace를 생략한다.

## 8. 성공/실패 경로 안정화

generationTrace는 handoff 성공 경로를 깨뜨리면 안 된다.
trace 생성 중 예외가 발생하면 handoff 자체의 성공/실패 기준은 유지하고 `diagnostics.generationTraceError`만 남긴다.

성공 경로에서는 `episode_spec_adapter`와 `scenario_reflection` source가 포함되어야 한다.
실패 경로에서는 `generationTrace.summary.status=failed`, `failureStage`, `errorSummary`를 남겨 어느 단계에서 멈췄는지 확인한다.
`success=false` 응답에서 `failureStage`를 특정하지 못하면 `unknown`으로 기록하고, `traceStatus`는 `success`, `failed`, `partial` 중 하나로 유지한다.
