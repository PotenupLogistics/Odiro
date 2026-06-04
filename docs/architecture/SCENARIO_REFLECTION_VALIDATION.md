# Scenario Reflection Validation

Scenario reflection은 deterministic scenario post-processing 전후에 실행된다. schema-valid payload에 Kickboard obstacle, 양수 `blockingRatio`, `Crossing` behavior가 있는 pedestrian이 이미 포함되어 있으면 해당 semantic issue는 해결된 것으로 본다.

post-processing은 manual policy evidence나 schema validation을 대체하지 않는다. 기존 schema 안에서 자연어 요구사항으로부터 명확히 유도되는 World Config scenario element만 보강한다.

## 1. 목적 (Purpose)

Scenario reflection validation은 schema-valid World Config가 사용자의 자연어 scenario도 실제로 반영했는지 확인한다.

## 2. Difference From Schema Validation

Schema validation은 구조, required field, type, allowed field를 확인한다. Scenario reflection validation은 요청된 Kickboard obstacle이나 pedestrian crossing이 실제 payload에 표현됐는지 같은 semantic intent를 확인한다.

## 3. 현재 규칙 (Current Rules)

- prompt가 narrow sidewalk를 언급하면 `map.sidewalkWidthCm`이 존재하고 좁은 보도 범위에 있어야 한다.
- prompt가 `120cm`처럼 `sidewalkWidthCm` 값을 명시하면 `map.sidewalkWidthCm`은 그 값을 보존해야 한다.
- prompt가 Kickboard를 언급하면 `obstacles` 또는 `environmentObjects`에 type `Kickboard`가 포함되어야 한다.
- prompt가 generic/static obstacle을 언급하면 `obstacles[]`에 schema-valid obstacle이 하나 이상 있어야 한다.
- prompt가 `x=400, y=0, z=0` 같은 obstacle coordinate를 제공하면 `obstacles[].position`이 그 값과 일치해야 한다.
- prompt가 path blocked 상황을 말하면 obstacle에는 양수 `blockingRatio`가 있어야 한다.
- prompt가 `blockingRatio 0.6`을 명시하면 `obstacles[].blockingRatio`는 그 값을 보존해야 한다.
- prompt가 pedestrian이 없다고 명시하면 `pedestrians`는 비어 있거나 없어야 한다.
- prompt가 pedestrian을 언급하면 `pedestrians`에는 하나 이상의 item이 있어야 한다.
- prompt가 crossing을 언급하면 pedestrian에는 crossing-like behavior가 있어야 한다.
- prompt가 terrain risk를 언급하면 map slope가 그 위험을 반영해야 한다.

## 4. Generation Flow

request validation이 필요한 경우 `generatedPayload`는 schema validation과 scenario reflection validation을 모두 통과해야 성공으로 본다.

## 5. Scope

이 계층은 heuristic/rule-based layer다. sample JSON, fixture, vector DB, embedding index, OpenAI fallback behavior를 생성하지 않는다.

## 6. Binding To Schema Paths

scenario requirement는 `map.sidewalkWidthCm`, `obstacles[].type`, `obstacles[].position`, `obstacles[].blockingRatio`, `pedestrians[]`, `pedestrians[].behavior` 같은 구체적인 schema path에 연결된다. prompt builder는 이 path를 포함해 model이 새 schema field를 만들지 않고 semantic requirement를 만족하도록 유도한다.

## 7. Detailed Issues

reflection issue는 `requirementId`, `issueType`, `expectedPath`, `expectedValueHint`, `actualValueSummary`, `repairInstruction`을 포함한다. scenario repair prompt는 이 field를 사용해 누락된 semantic requirement만 수정한다.

## Route-relative placement validation

* `obstacle_route_midpoint` requirement가 있으면 WorldConfig의 obstacle position이 robot.spawn과 robot.goal의 midpoint 50cm 이내인지 검사한다.
* spawn=(0,0,0), goal=(800,0,0)이면 expected midpoint는 (400,0,0)이다.
* obstacle이 midpoint에서 벗어나면 `obstacle_not_near_route_midpoint` issue로 `passed=false` 처리한다.
