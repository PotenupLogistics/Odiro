from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
GUIDE = ROOT / "docs" / "UE_EPISODE_SPEC_JSON_GUIDE.md"
ADAPTER = ROOT / "app" / "services" / "world_config_to_episode_spec_adapter.py"
README = ROOT / "README.md"
DOCS_README = ROOT / "docs" / "README.md"
ADAPTER_DOC = ROOT / "docs" / "UE5_EPISODE_SPEC_ADAPTER.md"

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


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    guide_text = _read(GUIDE)
    adapter_text = _read(ADAPTER)
    doc_text = _read(ADAPTER_DOC)
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "ue_episode_spec_guide_alignment",
        "passed": False,
        "warning": False,
        "guideExists": GUIDE.exists(),
        "guideHasSchema": "episode_actor_spawn_mvp" in guide_text,
        "guideHasRootSections": all(term in guide_text for term in ["ground_model", "paths", "actors"]),
        "guideHasUnitsAndVectors": all(term in guide_text for term in ["location_m", "size_m", "speed_mps"]),
        "adapterDocReferencesGuide": "UE_EPISODE_SPEC_JSON_GUIDE.md" in doc_text,
        "readmesReferenceGuide": "UE_EPISODE_SPEC_JSON_GUIDE.md" in readme_text,
        "adapterDoesNotEmitPenalties": '"penalties"' not in adapter_text and ".penalties" not in adapter_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("guideExists", "docs/UE_EPISODE_SPEC_JSON_GUIDE.md is missing."),
        ("guideHasSchema", "Guide must include episode_actor_spawn_mvp."),
        ("guideHasRootSections", "Guide must include ground_model, paths, actors."),
        ("guideHasUnitsAndVectors", "Guide must include location_m, size_m, speed_mps."),
        ("adapterDocReferencesGuide", "Adapter docs must reference the UE EpisodeSpec JSON guide."),
        ("readmesReferenceGuide", "README or docs README must reference the UE EpisodeSpec JSON guide."),
        ("adapterDoesNotEmitPenalties", "Adapter must not emit guide-incompatible penalties field."),
        ("noLiveProviderCallsInHarness", "Harness check must not perform live OpenAI/Ollama calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
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
