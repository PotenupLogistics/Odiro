# Scenario Repair Prompt

## 1. Purpose

Scenario repair prompt is used when a generated World Config passes schema validation but does not reflect the user's scenario requirements.

## 2. Difference From Schema Repair

- Schema repair fixes required fields, enum/type errors, and extra fields.
- Scenario repair preserves schema-valid JSON and only fills missing semantic requirements such as Kickboard obstacle, path blocking, pedestrian presence, or pedestrian crossing behavior.

## 3. Repair Targets

- missing Kickboard obstacle
- missing path blocking obstacle
- missing pedestrian
- missing pedestrian crossing behavior
- missing narrow sidewalk
- missing crosswalk context

## 4. Rules

- Do not add fields outside the schema.
- Preserve valid JSON structure from the previous payload.
- Return one JSON object only.
- Do not include markdown, comments, or explanations.
- Use schema paths such as `obstacles[].type`, `obstacles[].blockingRatio`, `pedestrians[]`, and `pedestrians[].behavior`.
