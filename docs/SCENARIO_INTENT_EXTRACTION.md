# Scenario Intent Extraction

## 1. Purpose

Scenario intent extraction converts a Korean natural-language World Config prompt into explicit scenario hints before RAG retrieval and prompt generation.

## 2. Extracted Intent

The extractor identifies:

- narrow sidewalk map hints
- Kickboard or obstacle hints
- path blocking hints
- pedestrian hints
- pedestrian crossing hints
- terrain risk hints
- operator/control hints

## 3. Keyword Mapping

- `좁은 보도`, `보도 폭`, `좁은 길` -> `narrow_sidewalk`, `sidewalk_operation`, `speed_policy`
- `킥보드`, `공유 킥보드`, `전동킥보드` -> `Kickboard`, `perception_requirement`
- `막고`, `막힘`, `경로를 막`, `차단` -> path blocking, `LocalAvoidance`, `ReplanPath`, `Stop`
- `보행자`, `사람` -> `Pedestrian`, `sidewalk_operation`
- `횡단`, `건너`, `가로질러` -> pedestrian crossing, `YieldWait`, `Stop`
- `경사`, `턱`, `넘어짐`, `기울어짐` -> `terrain_or_dynamic_safety`
- `관제`, `원격`, `수동` -> `operator_control`

## 4. RAG Connection

Intent categories, actions, and policy parameters are used as retrieval queries and fallback filters. This prevents Korean prompts from returning zero policy contexts when plain keyword search is insufficient.

## 5. Scope

This is a rule-based extraction layer. It does not call an external LLM and does not modify JSON Schema.
