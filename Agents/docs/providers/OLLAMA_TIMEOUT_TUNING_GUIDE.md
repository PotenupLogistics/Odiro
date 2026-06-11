# Ollama Timeout Tuning Guide

## 1. Purpose

This guide describes how to tune Ollama live smoke runs when provider timeout happens before JSON extraction or validation.

## 2. Tuning Order

1. Warm up the model.
2. Increase timeout.
3. Reduce repair attempts to `0` or `1` to inspect a single response.
4. Reduce `contextTopK`.
5. Use compact prompt mode.
6. Test a smaller or faster local model.
7. If local models still fail after runtime and prompt tuning, evaluate OpenAI fallback separately.

## 3. Recommended Commands

Qwen warm-up plus 180 second timeout:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단" --model qwen2.5:7b --timeout-sec 180 --max-repair-attempts 0 --context-top-k 2 --compact-prompt --warm-up --print-payload --include-extracted-json --report harness/reports/manual_ollama_world_config_smoke_qwen_timeout180_compact.json
```

Llama warm-up plus 180 second timeout:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단" --model llama3.1:8b --timeout-sec 180 --max-repair-attempts 0 --context-top-k 2 --compact-prompt --warm-up --print-payload --include-extracted-json --report harness/reports/manual_ollama_world_config_smoke_llama_timeout180_compact.json
```

## 4. Decision Criteria

- If only timeout occurs, schema or prompt quality cannot be judged yet.
- JSON extraction must succeed before prompt quality can be evaluated.
- Validation errors must be visible before schema or prompt correction is justified.
- If timeout persists after tuning, test a smaller local model or consider fallback later.

## 5. Report Fields

Timeout tuning reports include:

- `timeoutSec`
- `maxRepairAttempts`
- `contextTopK`
- `compactPrompt`
- `warmUpEnabled`
- `warmUpResult`
- `finalErrorClassification`
- `providerErrorSummary`
- `validationActuallyRun`
- `recommendedNextAction`

## Prompt Hardening Follow-up

Timeout tuning can make Ollama complete a response, but a completed response can still fail schema validation. After timeout tuning, use the hardened prompt mode with compact context and inspect missing required fields, extra fields, enum errors, and type errors in the smoke report.
