from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from app.models.generation import WorldConfigGenerationConstraints, WorldConfigGenerationRequest
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider, LlmError
from app.services import world_config_prompt_builder as prompt_builder_module
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.world_config_prompt_builder import build_world_config_prompt_package


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_ollama_world_config_smoke.py"


class FakeTimeoutClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=False,
            content=None,
            rawContent=None,
            error=LlmError(code="ollama_timeout", message="timed out"),
            warnings=[],
        )


def _request(max_repairs: int = 1) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-TIMEOUT-001",
        generationType="world_config",
        prompt="좁은 보도에서 공유 킥보드가 로봇 경로를 막고 보행자가 횡단",
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=42,
            requireValidation=True,
        ),
        maxRepairAttempts=max_repairs,
    )


def test_live_smoke_help_exposes_timeout_tuning_options() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )

    assert completed.returncode == 0
    assert "--timeout-sec" in completed.stdout
    assert "--warm-up" in completed.stdout
    assert "--compact-prompt" in completed.stdout
    assert "--context-top-k" in completed.stdout


def test_provider_timeout_becomes_final_timeout_classification() -> None:
    result = generate_world_config(
        _request(max_repairs=1),
        provider=LlmProvider.ollama,
        client_override=FakeTimeoutClient(),
    )

    assert result.success is False
    assert result.error is not None
    assert result.error.code == "ollama_timeout"
    assert result.validation.status == "skipped"
    assert all(attempt.providerErrorCode == "ollama_timeout" for attempt in result.attempts)


def test_compact_prompt_limits_context_text(monkeypatch) -> None:
    monkeypatch.setattr(prompt_builder_module, "build_policy_context_for_world_config", lambda *args, **kwargs: [])

    normal = build_world_config_prompt_package(_request(), context_top_k=3, compact_prompt=False)
    compact = build_world_config_prompt_package(_request(), context_top_k=2, compact_prompt=True)

    assert normal.retrievedContexts == []
    assert "No related policy RAG chunks were retrieved." in normal.warnings
    assert "No related policy RAG chunks were retrieved." in compact.warnings
    assert len(compact.retrievedContexts) <= 2
    assert compact.userPrompt


def test_no_forbidden_artifacts_from_timeout_tuning_tests() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
