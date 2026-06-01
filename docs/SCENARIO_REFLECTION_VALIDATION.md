# Scenario Reflection Validation

Scenario reflection now runs before and after deterministic scenario post-processing. If a schema-valid payload already contains a Kickboard obstacle, a positive `blockingRatio`, and a pedestrian with `Crossing` behavior, the corresponding semantic issues are cleared.

Post-processing does not replace manual policy evidence or schema validation. It only fills clearly implied World Config scenario elements inside the existing schema.

## 1. Purpose

Scenario reflection validation checks whether a schema-valid World Config also reflects the user's natural-language scenario.

## 2. Difference From Schema Validation

Schema validation checks structure, required fields, types, and allowed fields. Scenario reflection validation checks semantic intent, such as whether a requested Kickboard obstacle or pedestrian crossing is represented.

## 3. Current Rules

- If the prompt mentions a narrow sidewalk, `map.sidewalkWidthCm` must exist and be in a narrow-sidewalk range.
- If the prompt explicitly states `sidewalkWidthCm` such as `120cm`, `map.sidewalkWidthCm` must preserve that value.
- If the prompt mentions Kickboard, `obstacles` or `environmentObjects` must include type `Kickboard`.
- If the prompt mentions a generic/static obstacle, `obstacles[]` must include at least one schema-valid obstacle.
- If the prompt gives an obstacle coordinate such as `x=400, y=0, z=0`, `obstacles[].position` must match it.
- If the prompt says the path is blocked, an obstacle must include a positive `blockingRatio`.
- If the prompt explicitly states `blockingRatio 0.6`, `obstacles[].blockingRatio` must preserve that value.
- If the prompt says there are no pedestrians, `pedestrians` must be empty or absent.
- If the prompt mentions pedestrian, `pedestrians` must contain at least one item.
- If the prompt mentions crossing, a pedestrian must include crossing-like behavior.
- If the prompt mentions terrain risk, map slope should reflect that risk.

## 4. Generation Flow

`generatedPayload` is considered successful only after both schema validation and scenario reflection validation pass when request validation is required.

## 5. Scope

This layer is heuristic and rule-based. It does not create sample JSON, fixtures, vector DB, embedding index, or OpenAI fallback behavior.

## 6. Binding To Schema Paths

Scenario requirements are bound to concrete schema paths such as `map.sidewalkWidthCm`, `obstacles[].type`, `obstacles[].position`, `obstacles[].blockingRatio`, `pedestrians[]`, and `pedestrians[].behavior`. The prompt builder includes these paths so the model can satisfy semantic requirements without inventing new schema fields.

## 7. Detailed Issues

Reflection issues include `requirementId`, `issueType`, `expectedPath`, `expectedValueHint`, `actualValueSummary`, and `repairInstruction`. These fields are used by the scenario repair prompt to fix only missing semantic requirements.

## Route-relative placement validation

* `obstacle_route_midpoint` requirement가 있으면 WorldConfig의 obstacle position이 robot.spawn과 robot.goal의 midpoint 50cm 이내인지 검사한다.
* spawn=(0,0,0), goal=(800,0,0)이면 expected midpoint는 (400,0,0)이다.
* obstacle이 midpoint에서 벗어나면 `obstacle_not_near_route_midpoint` issue로 `passed=false` 처리한다.
