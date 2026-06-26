from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import shutil

from app.core.settings import Settings
from app.models.generation import WorldConfigGenerationRequest
from app.models.llm import LlmProvider
from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services.run_queue_export_service import RunQueueExportResult, export_run_queue_package
from app.services.setup_pair_queue_generator import SetupPairQueueResult
from app.services.setup_pair_queue_generator import generate_setup_pair_queue
from app.services.scenario_consistency_checker import check_setup_pair_queue_consistency
from app.services.environment_parameter_catalog import get_allowed_values
from app.utils.json_sanitizer import remove_json_nulls
from app.utils.report_serialization import write_json_report
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


@dataclass(frozen=True)
class ScenarioGenerationArtifacts:
    queue: SetupPairQueueResult
    export: RunQueueExportResult


@dataclass(frozen=True)
class ScenarioArtifactStorageError(Exception):
    code: str
    message: str
    filename: str | None = None


_PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _prompt_seed(prompt: str) -> int:
    digest = hashlib.sha256(prompt.strip().encode("utf-8")).hexdigest()
    return 1000 + int(digest[:8], 16) % 900000


def _scenario_type_from_prompt(prompt: str) -> str:
    intent = extract_scenario_intent(prompt)
    has_kickboard = "Kickboard" in intent.obstacleHints
    has_narrow_sidewalk = "narrow_sidewalk" in intent.mapHints
    has_crossing = "pedestrian_crossing" in intent.crossingHints
    if has_kickboard and (has_narrow_sidewalk or has_crossing):
        return "narrow_sidewalk_kickboard_crossing"
    if intent.pathBlockingHints or "Obstacle" in intent.obstacleHints:
        return "obstacle_ahead"
    if has_crossing:
        return "pedestrian_crossing"
    if intent.terrainHints:
        return "terrain_risk"
    return "generic_sidewalk"


def _semantic_fixed_constraints_from_prompt(prompt: str) -> dict[str, int | float | str | list[float] | list[str]]:
    intent = extract_scenario_intent(prompt)
    fixed: dict[str, int | float | str | list[float] | list[str]] = {}
    if intent.sidewalkWidthCm is not None:
        fixed["sidewalkWidthCm"] = intent.sidewalkWidthCm
    elif "narrow_sidewalk" in intent.mapHints:
        fixed["sidewalkWidthCm"] = 150
    if intent.goalDistanceM is not None:
        fixed["goalDistanceM"] = intent.goalDistanceM
    if intent.obstacleCount is not None:
        fixed["obstacleCount"] = intent.obstacleCount
    if intent.obstacleType is not None:
        fixed["obstacleType"] = intent.obstacleType
    if intent.obstacleTypes:
        fixed["obstacleTypes"] = intent.obstacleTypes
        fixed["obstacleCount"] = len(intent.obstacleTypes)
    if intent.obstaclePositionsFromStartM:
        fixed["obstaclePositionsFromStartM"] = intent.obstaclePositionsFromStartM
    if intent.obstacleLateralPosition is not None:
        fixed["obstacleLateralPosition"] = intent.obstacleLateralPosition
    if intent.pedestrianCount is not None:
        fixed["pedestrianCount"] = intent.pedestrianCount
    if intent.pedestrianDirection is not None:
        fixed["pedestrianDirection"] = intent.pedestrianDirection
    if intent.expectedRobotBehavior:
        fixed["expectedRobotBehavior"] = intent.expectedRobotBehavior
    if intent.obstacleBlockingRatio is not None:
        fixed["obstacleBlockingRatio"] = intent.obstacleBlockingRatio
    return fixed


def _sampler_fixed_parameters_from_constraints(
    constraints: dict[str, int | float | str | list[float] | list[str]],
) -> dict[str, int | float]:
    sampler_fixed: dict[str, int | float] = {}
    sampler_candidate_keys = {"sidewalkWidthCm", "obstacleBlockingRatio"}
    for key, value in constraints.items():
        if key not in sampler_candidate_keys:
            continue
        if not isinstance(value, int | float) or isinstance(value, bool):
            continue
        try:
            allowed_values = get_allowed_values(key)
        except KeyError:
            continue
        if value in allowed_values:
            sampler_fixed[key] = value
    return sampler_fixed


def _fixed_parameters_from_prompt(prompt: str) -> dict[str, int | float | str | list[float] | list[str]]:
    return _semantic_fixed_constraints_from_prompt(prompt)


def _generation_request(request: ScenarioGenerateRequest) -> WorldConfigGenerationRequest:
    seed = _prompt_seed(request.prompt)
    semantic_constraints = _semantic_fixed_constraints_from_prompt(request.prompt)
    return WorldConfigGenerationRequest(
        schemaVersion="1.0.0",
        requestId="GEN-SCENARIO-GENERATE-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=request.prompt,
        policyId="policy_v1_basic_safety",
        maxRepairAttempts=0,
        constraints={
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["Sidewalk", "Crosswalk"],
            "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
            "fixedPolicyId": "policy_v1_basic_safety",
            "defaultSeed": seed,
            "requireValidation": True,
            "semanticFixedConstraints": semantic_constraints,
            "environmentSampling": {
                "enabled": True,
                "seed": seed,
                "scenarioType": _scenario_type_from_prompt(request.prompt),
                "fixedParameters": _sampler_fixed_parameters_from_constraints(semantic_constraints),
            },
        },
    )


def _safe_request_id(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "_" for ch in value)


def _default_artifact_output_root(queue: SetupPairQueueResult) -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path(queue.export_base_dir) / f"{stamp}_{_safe_request_id(queue.request_id)}"


def _configured_artifact_output_root(raw_output_dir: str) -> Path | None:
    value = raw_output_dir.strip()
    if not value:
        return None
    candidate = Path(value).expanduser()
    if not candidate.is_absolute():
        if ".." in candidate.parts:
            raise ScenarioArtifactStorageError(
                "SCENARIO_ARTIFACT_OUTPUT_DIR_INVALID",
                "SCENARIO_ARTIFACT_OUTPUT_DIR must not traverse outside the project root.",
            )
        candidate = _PROJECT_ROOT / candidate
    return candidate.resolve()


def _artifact_output_root(queue: SetupPairQueueResult, settings: Settings) -> Path:
    configured_root = _configured_artifact_output_root(settings.scenario_artifact_output_dir)
    if configured_root is not None:
        return configured_root
    return _default_artifact_output_root(queue)


def _prepare_artifact_output_dir(output_root: Path) -> Path:
    if output_root.exists() and not output_root.is_dir():
        raise ScenarioArtifactStorageError(
            "SCENARIO_ARTIFACT_OUTPUT_DIR_INVALID",
            "SCENARIO_ARTIFACT_OUTPUT_DIR points to a file, not a directory.",
        )
    output_dir = output_root / "Json" / "Input"
    try:
        output_dir.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise ScenarioArtifactStorageError(
            "SCENARIO_ARTIFACT_OUTPUT_DIR_INVALID",
            "Failed to create SCENARIO_ARTIFACT_OUTPUT_DIR.",
        ) from exc
    return output_dir


def _safe_artifact_filename(relative_path: str) -> str:
    return Path(relative_path).name


def _existing_artifact_files(output_dir: Path) -> list[Path]:
    patterns = [
        "EpisodeRunQueue_*.json",
        "EpisodeSetup_*.json",
        "DeliveryBotSetup_*.json",
    ]
    files: list[Path] = []
    for pattern in patterns:
        files.extend(path for path in output_dir.glob(pattern) if path.is_file())
    return sorted(files, key=lambda path: path.name)


def _modified_time(path: Path) -> float | None:
    try:
        return path.stat().st_mtime
    except OSError:
        return None


def _backup_timestamp(existing_files: list[Path]) -> str:
    run_queue_times = [
        modified_time
        for path in existing_files
        if path.name.startswith("EpisodeRunQueue_") and (modified_time := _modified_time(path)) is not None
    ]
    candidate_times = run_queue_times or [
        modified_time for path in existing_files if (modified_time := _modified_time(path)) is not None
    ]
    timestamp = min(candidate_times) if candidate_times else datetime.now(timezone.utc).timestamp()
    return datetime.fromtimestamp(timestamp, timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def _backup_existing_artifacts(output_dir: Path, request_id: str) -> Path | None:
    existing_files = _existing_artifact_files(output_dir)
    if not existing_files:
        return None

    backup_dir = output_dir / "backup" / f"{_backup_timestamp(existing_files)}_{_safe_request_id(request_id or 'SCENARIO-GENERATE')}"
    try:
        backup_dir.mkdir(parents=True, exist_ok=False)
        for path in existing_files:
            shutil.move(str(path), str(backup_dir / path.name))
    except OSError as exc:
        raise ScenarioArtifactStorageError(
            "SCENARIO_ARTIFACT_BACKUP_FAILED",
            "Failed to back up existing scenario artifact files.",
        ) from exc
    return backup_dir


def write_scenario_artifacts_to_local_dir(
    artifacts: ScenarioGenerationArtifacts,
    *,
    settings: Settings | None = None,
) -> Path | None:
    resolved_settings = settings or Settings()
    if not resolved_settings.scenario_artifact_write_enabled:
        return None

    output_root = _artifact_output_root(artifacts.queue, resolved_settings)
    output_dir = _prepare_artifact_output_dir(output_root)
    _backup_existing_artifacts(output_dir, artifacts.queue.request_id)

    writes = [
        (
            _safe_artifact_filename(artifacts.queue.run_queue_path),
            artifacts.queue.run_queue.model_dump(mode="json", by_alias=True),
        )
    ]
    for item in artifacts.queue.items:
        writes.append(
            (
                _safe_artifact_filename(item.episode_setup_path),
                remove_json_nulls(
                    item.episode_setup.model_dump(mode="json", by_alias=True),
                    drop_empty_object_keys={"properties"},
                ),
            )
        )
        writes.append(
            (
                _safe_artifact_filename(item.delivery_bot_setup_path),
                remove_json_nulls(item.delivery_bot_setup.model_dump(mode="json", by_alias=True)),
            )
        )

    for filename, payload in writes:
        try:
            write_json_report(output_dir / filename, payload)
        except OSError as exc:
            raise ScenarioArtifactStorageError(
                "SCENARIO_ARTIFACT_WRITE_FAILED",
                f"Failed to write scenario artifact file: {filename}.",
                filename=filename,
            ) from exc
    return output_root


def _skipped_export_result(queue: SetupPairQueueResult) -> RunQueueExportResult:
    return RunQueueExportResult(
        exported=False,
        export_root=Path(queue.export_base_dir),
        run_queue_path=None,
        summary_path=None,
        validation_summary_path=None,
        trace_summary_path=None,
    )


def generate_scenario_artifacts(
    request: ScenarioGenerateRequest,
    *,
    write_export: bool = True,
) -> ScenarioGenerationArtifacts:
    settings = Settings(llmProviderChain=["openai"], llmMaxTotalAttempts=1)
    generation = generate_world_config(
        _generation_request(request),
        provider=LlmProvider.openai,
        settings=settings,
    )
    if generation.generatedPayload is None:
        raise RuntimeError(generation.error.message if generation.error else "WorldConfig generation failed.")
    generated_payload = dict(generation.generatedPayload)
    if generation.environmentSampling is not None:
        generated_payload["environmentSampling"] = generation.environmentSampling
    semantic_constraints = _semantic_fixed_constraints_from_prompt(request.prompt)
    generated_payload["semanticFixedConstraints"] = semantic_constraints
    environment_sampling = generated_payload.get("environmentSampling")
    if isinstance(environment_sampling, dict):
        environment_sampling["semanticFixedConstraints"] = semantic_constraints
    queue = generate_setup_pair_queue(
        generated_payload,
        episode_count=request.episode_count,
        request_id="SCENARIO-GENERATE-001",
    )
    if isinstance(queue, SetupPairQueueResult):
        consistency = check_setup_pair_queue_consistency(
            queue,
            fixed_constraints=semantic_constraints,
            expected_episode_count=request.episode_count,
        )
        if not consistency.passed:
            messages = "; ".join(
                f"{issue.code}: {issue.message}" for issue in consistency.issues if issue.severity == "error"
            )
            if messages:
                raise RuntimeError(f"Scenario consistency check failed. {messages}")
    export = export_run_queue_package(queue) if write_export else _skipped_export_result(queue)
    return ScenarioGenerationArtifacts(queue=queue, export=export)


def generate_scenario_run_queue(request: ScenarioGenerateRequest) -> EpisodeRunQueue:
    return generate_scenario_artifacts(request).queue.run_queue
