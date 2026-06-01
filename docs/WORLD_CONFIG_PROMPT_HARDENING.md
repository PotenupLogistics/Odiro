# World Config Prompt Hardening

## 1. Purpose

This document records the prompt hardening added after Ollama live smoke runs produced extractable JSON that still failed `world_config` validation.

## 2. Problem Observed

The previous live smoke reports showed that JSON extraction could succeed, but validation failed because generated payloads omitted required nested fields and sometimes added schema-extra fields.

Common missing required paths included:

- `map.lengthCm`
- `map.sidewalkWidthCm`
- `map.slopeDegree`
- `map.surfaceCondition`
- `robot.botId`
- `robot.spawn`
- `robot.goal`
- `robot.policyId`
- `runtime.maxDurationSec`
- `runtime.captureReplay`
- `runtime.emitEventLog`

## 3. Hardening Strategy

- Build a required field checklist directly from `schemas/world_config.schema.json`.
- Include allowed top-level field guidance in the prompt.
- Explicitly prohibit extra keys at the top level and inside nested objects.
- Tell the model not to use `null` for required fields.
- Include schema-derived checklist content in both initial and repair prompts.
- Group validation failures into missing required fields, extra fields, enum errors, and type errors.

## 4. Scope Limits

- The JSON Schema is not changed.
- Policy cards and RAG chunks are not changed.
- No sample JSON or fixture file is created.
- No vector DB or embedding index is created.
- No OpenAI call is added.

## 5. Next Step

Run Ollama smoke with the hardened prompt first on `qwen2.5:7b`. If validation still fails, compare against `llama3.1:8b` and decide whether more prompt repair or provider fallback is needed.

## 6. V2 Output Contract Hardening

Prompt hardening v2 adds an explicit World Config output contract to the system prompt and repair prompt. The contract lists required nested paths, allowed top-level fields, extra field prohibition, and scenario requirement bindings.

This does not change the JSON Schema. It only makes the existing contract more explicit to the model.
