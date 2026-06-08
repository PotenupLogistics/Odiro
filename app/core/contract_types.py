from __future__ import annotations

from enum import Enum
from pathlib import Path
from typing import Any

from app.models.decision import DecisionRequest, DecisionResponse
from app.models.evaluation import EvaluationSpec
from app.models.policy import PolicyConfig
from app.models.run_result import RunResult
from app.models.world import WorldConfig


ROOT = Path(__file__).resolve().parents[2]


class ContractType(str, Enum):
    policy_config = "policy_config"
    world_config = "world_config"
    decision_request = "decision_request"
    decision_response = "decision_response"
    evaluation_spec = "evaluation_spec"
    run_result = "run_result"


CONTRACT_SCHEMA_FILES: dict[ContractType, Path] = {
    ContractType.policy_config: ROOT / "schemas" / "policy_config.schema.json",
    ContractType.world_config: ROOT / "schemas" / "world_config.schema.json",
    ContractType.decision_request: ROOT / "schemas" / "decision_request.schema.json",
    ContractType.decision_response: ROOT / "schemas" / "decision_response.schema.json",
    ContractType.evaluation_spec: ROOT / "schemas" / "evaluation_spec.schema.json",
    ContractType.run_result: ROOT / "schemas" / "run_result.schema.json",
}

CONTRACT_MODELS: dict[ContractType, type[Any]] = {
    ContractType.policy_config: PolicyConfig,
    ContractType.world_config: WorldConfig,
    ContractType.decision_request: DecisionRequest,
    ContractType.decision_response: DecisionResponse,
    ContractType.evaluation_spec: EvaluationSpec,
    ContractType.run_result: RunResult,
}
