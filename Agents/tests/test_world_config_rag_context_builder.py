from __future__ import annotations

from app.models.generation import (
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
)
from app.services.world_config_rag_context_builder import build_policy_context_for_world_config


KOREAN_SCENARIO_PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


def _request(prompt: str) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-RAG-001",
        generationType="world_config",
        prompt=prompt,
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
        maxRepairAttempts=2,
    )


def test_rag_context_builder_returns_related_policy_chunks() -> None:
    prompt = "\ube44\uc0c1\uc815\uc9c0\uac00 \ud544\uc694\ud55c \ucda9\ub3cc \uc704\ud5d8 \uc0c1\ud669"

    contexts = build_policy_context_for_world_config(_request(prompt))

    assert contexts
    assert any(context.category == "emergency_stop" for context in contexts)
    assert all(context.shortText for context in contexts)


def test_rag_context_builder_limits_results_to_top_five() -> None:
    prompt = "\uc18d\ub3c4 \uc815\uc9c0 \uc7a5\uc560\ubb3c \ubcf4\ub3c4 \uad00\uc81c \uacbd\uc0ac"

    contexts = build_policy_context_for_world_config(_request(prompt))

    assert len(contexts) <= 5


def test_korean_scenario_prompt_returns_policy_contexts() -> None:
    contexts = build_policy_context_for_world_config(_request(KOREAN_SCENARIO_PROMPT))

    assert contexts
    assert any(
        context.category in {"perception_requirement", "sidewalk_operation", "speed_policy"}
        for context in contexts
    )
