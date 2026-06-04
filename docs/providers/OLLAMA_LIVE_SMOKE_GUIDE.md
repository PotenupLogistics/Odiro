# Ollama Live Smoke Guide

## 1. 목적

`scripts/run_ollama_world_config_smoke.py`는 로컬 Ollama WorldConfig generation 경로를 사람이 수동으로 확인하기 위한 smoke runner다.

이 runner는 자동 pytest 또는 harness live execution의 일부가 아니다. 자동 checks는 `--help`와 `--dry-run`만 실행하므로 `localhost:11434`를 호출하지 않는다.

## 2. Dry Run

먼저 dry-run으로 prompt package 구성과 deterministic RAG context retrieval을 확인한다. Dry-run은 Ollama를 호출하지 않는다.

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단" --dry-run
```

Dry-run 출력:

* provider와 model
* retrieved policy contexts
* schema summary
* validation policy
* warnings

`--report`를 지정하지 않으면 dry-run도 파일을 생성하지 않는다.

## 3. Live Smoke

Ollama가 로컬에서 실행 중일 때만 manual live smoke를 실행한다.

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

기본적으로 report는 저장하지 않는다. Report 저장이 필요하면 `--report`를 지정한다.

```bash
uv run python scripts/run_ollama_world_config_smoke.py \
  --prompt "장애물이 있는 보도 주행 상황" \
  --report harness/reports/manual_ollama_world_config_smoke.json
```

Report는 summary field 중심으로 저장한다. Full generated payload가 명시적으로 필요할 때만 `--include-payload`를 사용한다.

상세 diagnostics option:

```bash
uv run python scripts/run_ollama_world_config_smoke.py \
  --prompt "좁은 보도에서 공유 킥보드가 경로를 막고, 오른쪽에서 보행자가 횡단" \
  --model qwen2.5:7b \
  --include-extracted-json \
  --report harness/reports/manual_ollama_world_config_smoke_qwen_detailed.json
```

* `--include-raw-attempts`: full raw model output을 report에 저장한다. 기본으로 사용하지 않는다.
* `--include-extracted-json`: 각 attempt의 full extracted JSON을 저장한다.
* `--raw-preview-chars`: preview 길이를 조정한다. 기본값은 `1000`이다.

기본 report는 attempt-level preview, validation error summary, extraction summary, repair prompt preview, recommended next action을 포함한다.

Timeout tuning option:

* `--timeout-sec`: Ollama request timeout override
* `--context-top-k`: retrieved policy context 제한
* `--compact-prompt`: prompt context text 축소
* `--warm-up`: live smoke 전에 작은 JSON-only request 실행

## 5. Output Rules

* Runner는 sample JSON을 생성하지 않는다.
* Runner는 fixture file을 생성하지 않는다.
* Runner는 vector DB 또는 embedding index file을 생성하지 않는다.
* Generated payload는 `--print-payload`를 지정한 경우에만 출력한다.
* Generated payload는 JSON extraction과 `world_config` validation을 통과한 뒤에만 의미가 있다.

## 6. Automated Test Policy

자동 tests와 harness checks는 실제 Ollama server를 호출하면 안 된다. 자동 검증 대상은 다음으로 제한한다.

* CLI help
* dry-run behavior
* temporary path report writing
* forbidden generated artifact absence

Manual live execution은 operator action으로 남긴다.
