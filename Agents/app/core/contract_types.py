from __future__ import annotations

from enum import Enum
from pathlib import Path
from typing import Any

from app.models.decision import DecisionRequest, DecisionResponse
from app.models.evaluation import EvaluationSpec
from app.models.policy import PolicyConfig
from app.models.run_result import RunResult
from app.models.world import WorldConfig


AGENTS_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = AGENTS_ROOT.parent
CONTRACT_SCHEMA_DIR = REPO_ROOT / "contracts" / "schemas"


class ContractType(str, Enum):
    policy_config = "policy_config"
    world_config = "world_config"
    decision_request = "decision_request"
    decision_response = "decision_response"
    evaluation_spec = "evaluation_spec"
    run_result = "run_result"


CONTRACT_SCHEMA_FILES: dict[ContractType, Path] = {
    ContractType.policy_config: CONTRACT_SCHEMA_DIR / "policy_config.schema.json",
    ContractType.world_config: CONTRACT_SCHEMA_DIR / "world_config.schema.json",
    ContractType.decision_request: CONTRACT_SCHEMA_DIR / "decision_request.schema.json",
    ContractType.decision_response: CONTRACT_SCHEMA_DIR / "decision_response.schema.json",
    ContractType.evaluation_spec: CONTRACT_SCHEMA_DIR / "evaluation_spec.schema.json",
    ContractType.run_result: CONTRACT_SCHEMA_DIR / "run_result.schema.json",
}

CONTRACT_MODELS: dict[ContractType, type[Any]] = {
    ContractType.policy_config: PolicyConfig,
    ContractType.world_config: WorldConfig,
    ContractType.decision_request: DecisionRequest,
    ContractType.decision_response: DecisionResponse,
    ContractType.evaluation_spec: EvaluationSpec,
    ContractType.run_result: RunResult,
}
