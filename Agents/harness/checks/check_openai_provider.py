from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "run_openai_world_config_smoke.py"

REQUIRED_FILES = [
    ROOT / "app" / "services" / "llm_openai_client.py",
    ROOT / "docs" / "providers" / "OPENAI_PROVIDER_GUIDE.md",
    SCRIPT,
]

FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "data" / "rag" / "vector_db",
    ROOT / "data" / "rag" / "embeddings",
    ROOT / "data" / "rag" / "chroma",
    ROOT / "ue",
    ROOT / "UE",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _detect_hardcoded_keys() -> list[str]:
    found: list[str] = []
    for root in [ROOT / "app", ROOT / "scripts"]:
        if not root.exists():
            continue
        for path in root.rglob("*.py"):
            text = _read(path)
            for term in ["sk-", "api_key=\"", "api_key='"]:
                if term in text:
                    found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def run_check() -> dict[str, Any]:
    env_text = _read(ROOT / ".env.example")
    client_text = _read(ROOT / "app" / "services" / "llm_openai_client.py")
    dry_run = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--prompt",
            "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘.",
            "--dry-run",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]

    result: dict[str, Any] = {
        "check": "openai_provider",
        "passed": False,
        "warning": False,
        "requiredPathsExist": {
            path.relative_to(ROOT).as_posix(): path.exists() for path in REQUIRED_FILES
        },
        "envProviderOpenAi": "LLM_PROVIDER=openai" in env_text,
        "envProviderChainOpenAiOllama": "LLM_PROVIDER_CHAIN=openai,ollama" in env_text,
        "envOpenAiKeyEmpty": "OPENAI_API_KEY=\n" in env_text,
        "usesResponsesEndpoint": "https://api.openai.com/v1/responses" in client_text,
        "usesJsonSchemaFormat": '"type": "json_schema"' in client_text,
        "dryRunWorks": dry_run.returncode == 0 and '"openaiCalled": false' in dry_run.stdout,
        "hardcodedKeyWarnings": _detect_hardcoded_keys(),
        "forbiddenArtifacts": forbidden,
        "schemaFilesPresent": (ROOT / "schemas" / "world_config.schema.json").exists(),
        "errors": [],
        "warnings": [],
    }

    missing = [path for path, exists in result["requiredPathsExist"].items() if not exists]
    if missing:
        result["errors"].append(f"Missing OpenAI provider files: {missing}")
    for key, message in [
        ("envProviderOpenAi", ".env.example must set LLM_PROVIDER=openai."),
        ("envProviderChainOpenAiOllama", ".env.example must set LLM_PROVIDER_CHAIN=openai,ollama."),
        ("envOpenAiKeyEmpty", ".env.example OPENAI_API_KEY must be empty."),
        ("usesResponsesEndpoint", "OpenAI client must use the Responses endpoint."),
        ("usesJsonSchemaFormat", "OpenAI client must request JSON Schema structured output."),
        ("dryRunWorks", "OpenAI smoke dry-run failed or attempted a live call."),
        ("schemaFilesPresent", "world_config JSON Schema file is missing."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    if result["hardcodedKeyWarnings"]:
        result["errors"].append("Potential hardcoded OpenAI API key detected.")
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
