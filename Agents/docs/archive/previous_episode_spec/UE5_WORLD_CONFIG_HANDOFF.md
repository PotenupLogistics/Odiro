> Archived document.
> This document is kept for historical reference and is not the current UE contract.
> Current UE contracts live under `docs/ue_contracts/`.

# UE5 World Config Handoff

## 1. 목적

AI가 생성하고 검증한 World Config를 UE5가 받을 수 있는 handoff 계약으로 정의한다.

## 2. Handoff Flow

Natural Language Prompt
-> World Config Generation
-> Schema Validation
-> Scenario Reflection
-> Scenario Post-Processing
-> UE5 Handoff Response
-> UE5 JSON Parser

## 3. Handoff Endpoint

* `POST /api/v1/ue5/world-config/handoff`

## 4. Response 구조

* `handoffId`
* `worldConfig`
* `metadata`
* `validation`
* `scenarioReflection`
* `postProcessing`
* `diagnostics`
* `warnings`
* `error`

## 5. UE5가 우선 읽어야 하는 필드

`worldConfig`:

* `schemaVersion`
* `worldId`
* `scenarioId`
* `seed`
* `map`
* `robot`
* `obstacles`
* `pedestrians`
* `runtime`

## 6. UE5 구현 참고

* `map.lengthCm`, `map.sidewalkWidthCm`로 보도 환경을 구성한다.
* `robot.spawn`, `robot.goal`로 로봇과 목적지를 배치한다.
* `obstacles[]`를 UE5 Actor로 생성한다.
* `pedestrians[]`를 이동 Actor로 생성한다.
* `runtime.maxDurationSec` 기준으로 실행 시간을 제한한다.
* `postProcessing.patches`는 디버깅용이며 UE5는 `worldConfig`를 실행 기준으로 사용한다.

## 7. 실패 처리

* `success=false`이면 UE5는 `worldConfig`를 실행하지 않는다.
* `error.code`를 확인한다.
* `validation`이 false이면 UE5 handoff 대상이 아니다.

## 8. 주의

* 이 handoff는 프로젝트 내부 시뮬레이션 계약이다.
* 실제 안전 보장 또는 인증 보장을 의미하지 않는다.
* sample JSON과 fixture는 이 단계에서 생성하지 않는다.
# EpisodeSpec response format

`/api/v1/ue5/world-config/handoff`는 `responseFormat`으로 출력 형태를 선택할 수 있다.

* `world_config`: 기존 호환성을 위해 `worldConfig` 중심으로 반환한다.
* `episode_spec`: UE 실행 계약인 `episodeSpec`과 `episodeValidation`을 반환한다.
* `both`: `worldConfig`와 `episodeSpec`을 함께 반환한다.

`episodeSpec`은 `WorldConfig`가 생성되고 contract validation을 통과한 뒤에만 변환된다. 변환 과정의 임시 prop mapping 등은 `conversionWarnings`에 포함된다.

UE 연동 테스트에서는 `responseFormat=episode_spec`을 우선 사용하고, 디버깅이 필요한 경우 `responseFormat=both`를 사용한다.

Export CLI도 동일한 선택지를 제공한다.

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider ollama --format episode_spec
```

`--out`을 지정하지 않으면 파일을 생성하지 않는다.
