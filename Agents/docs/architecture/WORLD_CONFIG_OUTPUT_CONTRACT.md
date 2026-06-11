# World Config Output Contract

## 1. 목적

이 문서는 LLM이 생성하는 WorldConfig JSON의 필수 구조와 허용 field를 설명한다.

## 2. Required Nested Paths

현재 output contract는 `schemas/world_config.schema.json`에서 파생된다. 필수 nested path는 다음과 같다.

* `schemaVersion`
* `worldId`
* `scenarioId`
* `seed`
* `map.type`
* `map.lengthCm`
* `map.sidewalkWidthCm`
* `map.surfaceCondition`
* `map.slopeDegree`
* `robot.botId`
* `robot.spawn.x`
* `robot.spawn.y`
* `robot.spawn.z`
* `robot.goal.x`
* `robot.goal.y`
* `robot.goal.z`
* `robot.policyId`
* `runtime.maxDurationSec`
* `runtime.captureReplay`
* `runtime.emitEventLog`

`robot.spawn.yawDegree`는 현재 JSON Schema와 Pydantic model이 허용하지 않으므로 포함하지 않는다.

## 3. Allowed Top-Level Fields

* `schemaVersion`
* `worldId`
* `scenarioId`
* `seed`
* `map`
* `robot`
* `runtime`
* `obstacles`
* `pedestrians`
* `environmentObjects`

## 4. Extra Field Prohibition

LLM은 schema 밖의 field를 생성하면 안 된다. JSON response에는 markdown, comment, explanation, extra key를 포함하지 않는다.

## 5. Scenario Requirement Binding

* `narrow_sidewalk` -> `map.sidewalkWidthCm`
* `Kickboard` -> `obstacles[].type`
* `pathBlockingHints` -> `obstacles[].blockingRatio`
* `Pedestrian` -> `pedestrians[]`
* `pedestrian_crossing` -> `pedestrians[].behavior`
