# Scenario Post-Processing

## 1. 목적

LLM이 schema-valid World Config를 만들었지만 사용자의 자연어 시나리오 조건이 누락된 경우, 명확하게 추출된 scenario intent를 기반으로 deterministic하게 누락 요소를 보강한다.

## 2. 적용 대상

* narrow sidewalk
* Kickboard obstacle
* path blocking
* pedestrian
* pedestrian crossing
* crosswalk context

## 3. 원칙

* schema에 없는 필드는 추가하지 않는다.
* 기존 유효 payload를 최대한 보존한다.
* 모든 patch를 기록한다.
* post-processing 후 schema validation과 scenario reflection을 다시 수행한다.
* 공식 인증 또는 법적 안전 보장 표현을 사용하지 않는다.

## 4. 현재 규칙

* 좁은 보도 intent가 있고 `map.sidewalkWidthCm`가 없거나 넓으면 `120.0`으로 보정한다.
* Kickboard intent가 있고 장애물에 Kickboard가 없으면 `obstacles`에 `kickboard_001`을 추가한다.
* 경로 차단 intent가 있으면 관련 obstacle의 `blockingRatio`를 `0.6`으로 설정한다.
* 보행자 intent가 있고 `pedestrians`가 비어 있으면 `pedestrian_001`을 추가한다.
* 횡단 intent가 있으면 보행자 `behavior`를 `Crossing`으로 설정한다.
* 현재 schema는 crosswalk 전용 필드가 없으므로, 전용 context는 warning으로 남긴다.

Post-processing 결과는 UE5 handoff response의 `postProcessing` 필드에 포함된다. UE5 실행 기준은 항상 검증된 `worldConfig`이며 patch 목록은 디버깅용이다.

## 5. OpenAI Fallback과의 관계

OpenAI fallback은 post-processing과 scenario repair 후에도 실패가 반복될 때 검토한다. 현재 단계에서는 OpenAI API를 호출하지 않는다.
