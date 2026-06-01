# Ollama Live Smoke Guide

## 1. Purpose

`scripts/run_ollama_world_config_smoke.py` is a manual smoke runner for checking the local Ollama World Config generation path.

It is not part of automated pytest or harness live execution. Automated checks only run `--help` and `--dry-run`, so they do not call `localhost:11434`.

## 2. Dry Run

Use dry-run first to verify prompt package construction and deterministic RAG context retrieval without calling Ollama:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단" --dry-run
```

Dry-run prints:

- provider and model
- retrieved policy contexts
- schema summary
- validation policy
- warnings

Dry-run does not create files unless `--report` is provided.

## 3. Live Smoke

Run a manual live smoke only when Ollama is running locally:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단"
```

Optional overrides:

```bash
uv run python scripts/run_ollama_world_config_smoke.py \
  --prompt "경사로와 턱이 있는 보도 주행 상황" \
  --model llama3.1:8b \
  --base-url http://localhost:11434 \
  --max-repair-attempts 2
```

## 4. Report Option

No report is written by default. To write a report:

```bash
uv run python scripts/run_ollama_world_config_smoke.py \
  --prompt "장애물이 있는 보도 주행 상황" \
  --report harness/reports/manual_ollama_world_config_smoke.json
```

The report includes summary fields only. Add `--include-payload` only when the full generated payload is explicitly needed.

Detailed diagnostics options:

```bash
uv run python scripts/run_ollama_world_config_smoke.py \
  --prompt "좁은 보도에서 공유 킥보드가 경로를 막고, 오른쪽에서 보행자가 횡단" \
  --model qwen2.5:7b \
  --include-extracted-json \
  --report harness/reports/manual_ollama_world_config_smoke_qwen_detailed.json
```

- `--include-raw-attempts`: stores full raw model output in the report. Do not use by default.
- `--include-extracted-json`: stores full extracted JSON for each attempt.
- `--raw-preview-chars`: controls the preview length; default is `1000`.

Default reports include attempt-level previews, validation error summaries, extraction summaries, repair prompt previews, and recommended next action.

Timeout tuning options:

- `--timeout-sec`: override Ollama request timeout.
- `--context-top-k`: limit retrieved policy contexts.
- `--compact-prompt`: reduce context text in the prompt.
- `--warm-up`: run a small JSON-only request before live smoke.

## 5. Output Rules

- The runner does not create sample JSON.
- The runner does not create fixture files.
- The runner does not create vector DB or embedding index files.
- The generated payload is printed only with `--print-payload`.
- A generated payload is useful only after JSON extraction and `world_config` validation pass.

## 6. Automated Test Policy

Automated tests and harness checks must not call a real Ollama server. They validate only:

- CLI help
- dry-run behavior
- report writing to a temporary path
- absence of forbidden generated artifacts

Manual live execution remains an operator action.
