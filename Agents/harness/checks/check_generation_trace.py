from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
MODEL = ROOT / "app" / "models" / "generation_trace.py"
BUILDER = ROOT / "app" / "services" / "generation_trace_builder.py"
HANDOFF = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
SUMMARY = ROOT / "app" / "utils" / "handoff_response_summary.py"
DOC = ROOT / "docs" / "architecture" / "MAP_GENERATION_TRACE.md"
README = ROOT / "README.md"
DOCS_README = ROOT / "docs" / "README.md"

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
    model_text = _read(MODEL)
    builder_text = _read(BUILDER)
    handoff_text = _read(HANDOFF)
    summary_text = _read(SUMMARY)
    trace_tests = _read(ROOT / "tests" / "test_ue5_handoff_generation_trace.py")
    builder_tests = _read(ROOT / "tests" / "test_generation_trace_builder.py")
    doc_text = _read(DOC)
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "generation_trace",
        "passed": False,
        "warning": False,
        "modelExists": MODEL.exists(),
        "builderExists": BUILDER.exists(),
        "docExists": DOC.exists(),
        "modelHasTraceTypes": all(term in model_text for term in ["GenerationTrace", "GenerationTraceItem", "TraceSourceType"]),
        "builderHasTraceSources": all(term in builder_text for term in ["environment_sampling", "placement_rule", "episode_spec_adapter", "policy_rag"]),
        "handoffIncludesGenerationTrace": "generationTrace" in handoff_text and "build_generation_trace" in handoff_text,
        "summaryIncludesGenerationTraceFields": all(term in summary_text for term in ["generationTraceExists", "traceItemCount", "coordinateSource", "policyRagUsedFor", "traceFailureStage", "generationTraceError"]),
        "builderHasFailureStage": "failure_stage" in builder_text and "error_summary" in builder_text and "infer_failure_stage" in builder_text,
        "handoffSeparatesTraceError": "generationTraceError" in handoff_text and "except Exception as exc" in handoff_text,
        "handoffDiagnosticsIncludesFailureStage": "failureStage" in handoff_text and "errorSummary" in handoff_text,
        "summaryFlagsMissingFailureStage": "missingFailureStage" in summary_text and "episodeSpecMissingReason" in summary_text,
        "testsCoverEpisodeSpecAdapterTrace": "episode_spec_adapter" in trace_tests,
        "testsCoverScenarioReflectionTrace": "scenario_reflection" in trace_tests,
        "testsCoverFailureStages": all(term in (trace_tests + builder_tests) for term in ["world_config_validation", "episode_spec_adapter", "episode_scenario_reflection"]),
        "docStatesNoFullPayload": all(term in doc_text for term in ["full WorldConfig", "full EpisodeSpec", "rawContent", "API key"]),
        "readmesLinkTraceDoc": "MAP_GENERATION_TRACE.md" in readme_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }
    for key, message in [
        ("modelExists", "app/models/generation_trace.py is missing."),
        ("builderExists", "app/services/generation_trace_builder.py is missing."),
        ("docExists", "docs/architecture/MAP_GENERATION_TRACE.md is missing."),
        ("modelHasTraceTypes", "Generation trace model must define trace item/source types."),
        ("builderHasTraceSources", "Trace builder must cover key generation source types."),
        ("handoffIncludesGenerationTrace", "UE5 handoff diagnostics must include generationTrace."),
        ("summaryIncludesGenerationTraceFields", "Handoff response summary must include generationTrace summary fields."),
        ("builderHasFailureStage", "Trace builder must record failureStage/errorSummary."),
        ("handoffSeparatesTraceError", "Handoff service must separate trace generation errors from handoff success."),
        ("handoffDiagnosticsIncludesFailureStage", "Handoff diagnostics must include failureStage/errorSummary for failed responses."),
        ("summaryFlagsMissingFailureStage", "Handoff response summary must flag missing failureStage and EpisodeSpec missing reason."),
        ("testsCoverEpisodeSpecAdapterTrace", "Tests must verify episode_spec_adapter trace evidence."),
        ("testsCoverScenarioReflectionTrace", "Tests must verify scenario_reflection trace evidence."),
        ("testsCoverFailureStages", "Tests must verify world_config_validation, episode_spec_adapter, and episode_scenario_reflection failure stages."),
        ("docStatesNoFullPayload", "Trace docs must state that secrets/raw/full payloads are not stored."),
        ("readmesLinkTraceDoc", "README docs must link MAP_GENERATION_TRACE.md."),
        ("noLiveProviderCallsInHarness", "Harness check must not perform live provider calls."),
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
