# Scenario Repair Prompt

## 1. 목적 (Purpose)

Scenario repair prompt는 생성된 World Config가 schema validation은 통과했지만 사용자의 scenario requirement를 반영하지 못했을 때 사용한다.

## 2. Difference From Schema Repair

- Schema repair는 required field, enum/type error, extra field를 수정한다.
- Scenario repair는 schema-valid JSON을 유지하면서 Kickboard obstacle, path blocking, pedestrian presence, pedestrian crossing behavior 같은 누락된 semantic requirement만 보강한다.

## 3. Repair Targets

- 누락된 Kickboard obstacle
- 누락된 path blocking obstacle
- 누락된 pedestrian
- 누락된 pedestrian crossing behavior
- 누락된 narrow sidewalk
- 누락된 crosswalk context

## 4. Rules

- schema 밖의 field를 추가하지 않는다.
- 이전 payload의 valid JSON structure를 보존한다.
- JSON object 하나만 반환한다.
- markdown, comment, 설명 문장을 포함하지 않는다.
- `obstacles[].type`, `obstacles[].blockingRatio`, `pedestrians[]`, `pedestrians[].behavior` 같은 schema path를 사용한다.
