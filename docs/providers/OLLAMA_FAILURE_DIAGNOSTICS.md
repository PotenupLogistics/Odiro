# Ollama Failure Diagnostics

## 1. 목적

이 문서는 Ollama live smoke에서 `validation_failed`가 발생했을 때 원인을 진단하는 방법을 설명한다. 목표는 schema 변경, OpenAI fallback 추가, generation API 변경 전에 model output 품질을 먼저 확인하는 것이다.

## 2. 확인할 항목

* `rawContentPreview`가 JSON object처럼 보이는지 확인한다.
* JSON extraction이 성공했는지 확인한다.
* `extractedJsonKeys`에 어떤 key가 있었는지 확인한다.
* required field 누락 여부를 확인한다.
* enum 또는 type validation error가 있었는지 확인한다.
* repair attempt가 error를 줄였는지, 같은 error를 반복했는지 확인한다.
* JSON extraction 전에 provider-level error가 발생했는지 확인한다.

## 3. Report Fields

상세 live smoke report에는 다음 field가 포함될 수 있다.

* `attemptsDetail`
* `rawContentPreview`
* `rawContentLength`
* `jsonExtractionSuccess`
* `extractedJsonPreview`
* `extractedJsonKeys`
* `validationErrors`
* `validationErrorSummary`
* `repairPromptPreview`
* `providerErrorCode`
* `recommendedNextAction`

기본적으로 full raw output은 저장하지 않는다. 명시적으로 필요할 때만 `--include-raw-attempts`를 사용한다.

기본적으로 full extracted JSON도 저장하지 않는다. 자세한 validation 진단이 필요할 때만 `--include-extracted-json`을 사용한다.

## 4. Next Action Rules

* JSON extraction이 실패하면 JSON-only prompt guidance를 강화한다.
* required field가 누락되면 schema summary와 required field checklist를 강화한다.
* enum 또는 type error가 발생하면 prompt에 enum/type constraint를 더 명확히 제공한다.
* repair 이후에도 같은 validation failure가 남으면 fallback을 추가하기 전에 repair prompt 품질을 확인한다.
* local model이 prompt 개선 이후에도 계속 실패하면 OpenAI fallback을 별도 평가한다.
* 모든 attempt가 `providerErrorCode=ollama_timeout`이면 validation은 실제로 실행되지 않은 것이다. timeout 또는 runtime부터 조정한다.

## 5. Constraints

* 단일 model output을 맞추기 위해 JSON Schema를 수정하지 않는다.
* diagnostics 중 sample JSON 또는 fixture 파일을 추가하지 않는다.
* vector DB 또는 embedding index artifact를 만들지 않는다.
* pytest 또는 harness가 실제 Ollama server에 의존하게 만들지 않는다.

## 6. Prompt Hardening Diagnostics

Ollama가 extractable JSON을 반환했지만 `world_config` validation이 실패하면 `validationErrorSummary.missingRequiredFields`와 `validationErrorSummary.extraFields`를 먼저 확인한다. Nested required field 누락은 prompt에 required checklist가 더 필요하다는 신호다. Extra field는 allowed-field guidance가 더 엄격해야 한다는 신호다.

Prompt hardening은 JSON Schema를 변경하지 않고 sample JSON도 만들지 않는다. Prompt package content와 repair prompt diagnostics만 조정한다.
