from __future__ import annotations

import json
from copy import deepcopy
from pathlib import Path
from typing import Any

from app.agents.common.llm_json_client import AgentLlmClient, AgentLlmJsonClient
from app.agents.common.spec_context_loader import SpecContextLoader
from app.core.settings import Settings
from app.agents.scenario_generation_v2.intent_parser import IntentParser, ScenarioIntent
from app.agents.scenario_generation_v2.repair_diagnostics import RepairDiagnosticCollector
from app.agents.scenario_generation_v2.repair_handler import RepairHandler
from app.agents.scenario_generation_v2.request_normalizer import RequestNormalizer
from app.agents.scenario_generation_v2.response_builder import ResponseBuilder
from app.agents.scenario_generation_v2.scenario_template_schema import project_scenario_v1_response_schema
from app.agents.scenario_generation_v2.scenario_preset_loader import ScenarioPresetLoader
from app.agents.scenario_generation_v2.prop_normalizer import LEGACY_STATIC_OBSTACLE_PROP_ALIASES
from app.agents.scenario_generation_v2.scenario_preset_patcher import ScenarioPresetPatcher
from app.agents.scenario_generation_v2.scenario_preset_registry import ScenarioPresetRegistry
from app.agents.scenario_generation_v2.scenario_type_selector import ScenarioTypeSelector
from app.agents.scenario_generation_v2.template_json_writer import TemplateJsonWriter
from app.agents.scenario_generation_v2.template_planner import TemplatePlanner
from app.agents.scenario_generation_v2.template_validator import ALLOWED_PROPS, FORBIDDEN_ROOT_FIELDS, TemplateValidator
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request, ScenarioGenerateV2Response, V2ValidationIssue


class ScenarioGenerationV2Agent:
    """Coordinates prompt-only project scenario generation and validation."""

    def __init__(
        self,
        *,
        settings: Settings | None = None,
        llm_client: AgentLlmClient | None = None,
        spec_context_loader: SpecContextLoader | None = None,
        scenario_preset_loader: ScenarioPresetLoader | None = None,
    ) -> None:
        """Owns the agent components while leaving template persistence to callers."""
        self.settings = settings or Settings()
        self.llm_client = llm_client
        self.spec_context_loader = spec_context_loader
        self.scenario_preset_loader = scenario_preset_loader or ScenarioPresetLoader()
        self.scenario_preset_registry = ScenarioPresetRegistry()
        self.scenario_preset_patcher = ScenarioPresetPatcher()
        self.normalizer = RequestNormalizer()
        self.intent_parser = IntentParser()
        self.type_selector = ScenarioTypeSelector()
        self.planner = TemplatePlanner()
        self.writer = TemplateJsonWriter()
        self.validator = TemplateValidator()
        self.repair_handler = RepairHandler()
        self.response_builder = ResponseBuilder()
        self.last_repair_events: list[dict[str, object]] = []

    def generate(self, request: ScenarioGenerateV2Request) -> ScenarioGenerateV2Response:
        """Generate a project scenario JSON object from a single natural-language prompt."""
        diagnostics = RepairDiagnosticCollector()
        self.last_repair_events = []
        normalized = self.normalizer.normalize(request.prompt)
        if self.settings.v2AgentLlmEnabled:
            response = self._generate_with_llm(normalized.normalized_prompt, diagnostics=diagnostics)
            if response is not None:
                self.last_repair_events = diagnostics.as_dicts()
                return response

        intent = self.intent_parser.parse(normalized.normalized_prompt)
        scenario_type = self.type_selector.select(intent)
        plan = self.planner.plan(intent, scenario_type)

        if self.settings.v2AgentLlmEnabled:
            response = self._generate_deterministic(
                plan,
                intent=intent,
                generation_mode="fallback",
                fallback_warning=V2ValidationIssue(
                    field="scenario",
                    message="LLM output validation failed; deterministic fallback scenario was used.",
                ),
                diagnostics=diagnostics,
            )
            self.last_repair_events = diagnostics.as_dicts()
            return response
        response = self._generate_deterministic(
            plan,
            intent=intent,
            generation_mode="deterministic",
            diagnostics=diagnostics,
        )
        self.last_repair_events = diagnostics.as_dicts()
        return response

    def _generate_deterministic(
        self,
        plan,
        *,
        intent: ScenarioIntent,
        generation_mode: str,
        fallback_warning: V2ValidationIssue | None = None,
        diagnostics: RepairDiagnosticCollector | None = None,
    ) -> ScenarioGenerateV2Response:
        scenario = self.writer.write(plan)
        scenario = self.repair_handler.repair(
            scenario,
            repair_quality=False,
            diagnostics=diagnostics,
            stage="agent.deterministic.initial",
        )
        scenario = self._postprocess_scenario_for_intent(
            scenario,
            intent,
            diagnostics=diagnostics,
            stage_prefix="agent.deterministic",
        )
        scenario = self.repair_handler.repair(scenario, diagnostics=diagnostics, stage="agent.deterministic.final")
        validation = self.validator.validate(scenario)
        if fallback_warning is not None:
            validation.warnings.append(fallback_warning)

        if not validation.valid:
            return self.response_builder.failed(
                summary="시나리오 템플릿 생성에 실패했습니다.",
                validation=validation,
            )

        return self.response_builder.success(
            scenario_id=scenario["scenario_id"],
            summary=plan.summary,
            scenario=scenario,
            validation=validation,
            assumptions=plan.assumptions,
            generation_mode=generation_mode,
        )

    def _generate_with_llm(
        self,
        prompt: str,
        *,
        diagnostics: RepairDiagnosticCollector | None = None,
    ) -> ScenarioGenerateV2Response | None:
        """Try LLM generation and at most one repair before deterministic fallback."""
        client = self.llm_client or AgentLlmJsonClient(settings=self.settings)
        try:
            scenario = self._generate_llm_template(prompt, client=client, response_name="scenario")
            intent = self.intent_parser.parse(prompt)
            scenario = self.repair_handler.repair(
                scenario,
                repair_quality=False,
                diagnostics=diagnostics,
                stage="agent.llm.initial",
            )
            scenario = self._postprocess_scenario_for_intent(
                scenario,
                intent,
                diagnostics=diagnostics,
                stage_prefix="agent.llm",
            )
            scenario = self.repair_handler.repair(scenario, diagnostics=diagnostics, stage="agent.llm.final")
            validation = self.validator.validate(scenario)
            if validation.valid:
                return self.response_builder.success(
                    scenario_id=scenario["scenario_id"],
                    summary=self._summary_from_template(scenario, "LLM-assisted scenario를 생성했습니다."),
                    scenario=scenario,
                    validation=validation,
                    assumptions=["LLM 후보를 먼저 생성하고 validator 통과 후 사용했습니다."],
                    generation_mode="llm",
                )
        except Exception:
            validation = None
            scenario = None

        if self.settings.v2AgentLlmRepairEnabled and self.settings.v2AgentLlmMaxRepairAttempts > 0:
            try:
                repaired = self._repair_llm_template(
                    prompt,
                    scenario,
                    validation,
                    client=client,
                    response_name="project_scenario_repair",
                )
                repaired = self.repair_handler.repair(
                    repaired,
                    repair_quality=False,
                    diagnostics=diagnostics,
                    stage="agent.llm_repair.initial",
                )
                intent = self.intent_parser.parse(prompt)
                repaired = self._postprocess_scenario_for_intent(
                    repaired,
                    intent,
                    diagnostics=diagnostics,
                    stage_prefix="agent.llm_repair",
                )
                repaired = self.repair_handler.repair(repaired, diagnostics=diagnostics, stage="agent.llm_repair.final")
                repaired_validation = self.validator.validate(repaired)
                if repaired_validation.valid:
                    return self.response_builder.success(
                        scenario_id=repaired["scenario_id"],
                        summary=self._summary_from_template(repaired, "LLM-assisted repair가 scenario를 생성했습니다."),
                        scenario=repaired,
                        validation=repaired_validation,
                        assumptions=["LLM 후보 repair 결과를 validator 통과 후 사용했습니다."],
                        generation_mode="llm_repaired",
                    )
            except Exception:
                return None
        return None

    def _generate_llm_template(
        self,
        prompt: str,
        *,
        client: AgentLlmClient | None = None,
        response_name: str,
    ) -> dict:
        """Request a Project Scenario-shaped JSON object from the configured LLM provider."""
        llm_client = client or self.llm_client or AgentLlmJsonClient(settings=self.settings)
        return llm_client.generate_json(
            system_prompt=self._read_prompt("system_prompt.md"),
            user_prompt=self._template_user_prompt(prompt),
            response_name=response_name,
            response_schema=project_scenario_v1_response_schema(),
        )

    def _repair_llm_template(
        self,
        prompt: str,
        template: object,
        validation: object,
        *,
        client: AgentLlmClient | None = None,
        response_name: str,
    ) -> dict:
        """Request a validator-error-guided project scenario repair from the LLM."""
        llm_client = client or self.llm_client or AgentLlmJsonClient(settings=self.settings)
        return llm_client.generate_json(
            system_prompt=self._read_prompt("system_prompt.md"),
            user_prompt=self._repair_user_prompt(prompt, template, validation),
            response_name=response_name,
            response_schema=project_scenario_v1_response_schema(),
        )

    def _template_user_prompt(self, prompt: str) -> str:
        """Build the LLM instruction for current Project Scenario v1 output."""
        return "\n\n".join(
            [
                self._read_prompt("template_writer_prompt.md"),
                self._spec_context_prompt_block(),
                "Project Scenario schema version 1 JSON 객체만 생성한다.",
                "필수 root: schema, version, scenario_id, intent, corridor, obstacles, pedestrians, robot.",
                "금지: template_id, template_path, current_template, sample_count, episode_count, base_seed, experiment_id, run_id, ground_model, static_obstacles, pedestrians.path.",
                "surface/prop/persona/encounter type은 catalog 허용값만 사용한다.",
                f"사용자 prompt:\n{prompt}",
            ]
        )

    def _repair_user_prompt(self, prompt: str, template: object, validation: object) -> str:
        """Build the one-shot LLM repair instruction for invalid template JSON."""
        return "\n\n".join(
            [
                self._read_prompt("repair_prompt.md"),
                self._spec_context_prompt_block(),
                f"사용자 prompt:\n{prompt}",
                f"검증 결과:\n{validation}",
                f"수정 대상 JSON:\n{json.dumps(template, ensure_ascii=False, default=str)}",
            ]
        )

    def _spec_context_prompt_block(self) -> str:
        """Load v2 Agent spec context lazily for LLM-only prompt paths."""
        if self.spec_context_loader is None:
            self.spec_context_loader = SpecContextLoader()
        return self.spec_context_loader.build_prompt_block()

    def _read_prompt(self, filename: str) -> str:
        """Read a bundled prompt fragment for the scenario generation agent."""
        return (Path(__file__).parent / "prompts" / filename).read_text(encoding="utf-8")

    def _postprocess_scenario_for_intent(
        self,
        scenario: dict,
        intent: ScenarioIntent,
        *,
        diagnostics: RepairDiagnosticCollector | None = None,
        stage_prefix: str = "agent",
    ) -> dict:
        """Apply prompt-specific quality corrections after structured LLM generation."""
        fallback = self._postprocess_base_scenario_for_intent(deepcopy(scenario), intent)
        preset_scenario = self._try_build_preset_scenario(
            intent,
            source_scenario=fallback,
            diagnostics=diagnostics,
            stage_prefix=stage_prefix,
        )
        return preset_scenario if preset_scenario is not None else fallback

    def _postprocess_base_scenario_for_intent(self, scenario: dict, intent: ScenarioIntent) -> dict:
        """Apply prompt-specific corrections that are independent of optional presets."""
        self._apply_alpha_pedestrian_policy(scenario)
        self._prefer_corridor_pose_robot_anchors(scenario)
        self._apply_requested_obstacle_sequence(scenario, intent)
        if intent.robot_anchor_only:
            obstacles = scenario.setdefault("obstacles", {})
            obstacles["placements"] = []
            if intent.robot_start_anchor is not None and intent.robot_goal_anchor is not None:
                scenario["robot"] = {"start": intent.robot_start_anchor, "goal": intent.robot_goal_anchor}
        if intent.requested_gate_obstacle_count == 2:
            obstacles = scenario.setdefault("obstacles", {})
            placements = obstacles.get("placements")
            if isinstance(placements, list) and len(placements) > 2:
                obstacles["placements"] = self._first_gate_pair(placements)
        return scenario

    def _apply_requested_obstacle_sequence(self, scenario: dict, intent: ScenarioIntent) -> None:
        """Preserve explicit catalog prop/count intent on LLM and repair candidates."""
        if intent.explicit_no_obstacles or not intent.requested_props:
            return
        obstacles = scenario.setdefault("obstacles", {})
        if not isinstance(obstacles, dict):
            scenario["obstacles"] = {"min_clear_width_m": 0.9, "placements": []}
            obstacles = scenario["obstacles"]
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            placements = []
        desired_count = intent.requested_obstacle_count or len(intent.requested_props)
        if desired_count <= 0:
            obstacles["placements"] = []
            return
        placement_template = self._placement_template(placements)
        patched: list[dict[str, Any]] = []
        seen_ids: set[str] = set()
        for index in range(desired_count):
            placement = deepcopy(placements[index]) if index < len(placements) and isinstance(placements[index], dict) else deepcopy(placement_template)
            placement["kind"] = placement.get("kind") if placement.get("kind") in {"fixed", "pattern"} else "fixed"
            placement["id"] = self._requested_obstacle_id(placement, index, seen_ids)
            seen_ids.add(placement["id"])
            placement["prop"] = intent.requested_props[index % len(intent.requested_props)]
            placement.setdefault("at", deepcopy(placement_template.get("at", {})))
            placement.setdefault("yaw_deg", 0)
            if intent.explicit_blocking:
                placement["allow_blocking"] = True
            elif placement.get("allow_blocking") is True:
                placement["allow_blocking"] = False
            patched.append(placement)
        obstacles["placements"] = patched

    def _placement_template(self, placements: list[object]) -> dict[str, Any]:
        """Return a placement template for extending underfit LLM outputs."""
        for placement in placements:
            if isinstance(placement, dict):
                return placement
        return {
            "kind": "fixed",
            "id": "requested_obstacle_01",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 0.0, "offset_m": 0.0, "lane": "walkway"},
            "yaw_deg": 0,
        }

    def _requested_obstacle_id(self, placement: dict[str, Any], index: int, seen_ids: set[str]) -> str:
        """Return a stable id while avoiding duplicates for expanded placements."""
        placement_id = placement.get("id")
        if isinstance(placement_id, str) and placement_id and placement_id not in seen_ids:
            return placement_id
        return f"requested_obstacle_{index + 1:02d}"

    def _try_build_preset_scenario(
        self,
        intent: ScenarioIntent,
        *,
        source_scenario: dict[str, Any],
        diagnostics: RepairDiagnosticCollector | None = None,
        stage_prefix: str = "agent",
    ) -> dict[str, Any] | None:
        """Load, patch, validate, repair, and re-validate an optional preset candidate."""
        preset_id = self.scenario_preset_registry.select(intent)
        if preset_id is None:
            return None
        load_result = self.scenario_preset_loader.try_load_scenario_preset(preset_id)
        if load_result.scenario is None:
            return None
        if self._preset_has_blocking_contract_issue(load_result.scenario):
            return None
        try:
            candidate = self.scenario_preset_patcher.patch(
                load_result.scenario,
                intent,
                preset_id=preset_id,
                source_scenario=source_scenario,
            )
            candidate = self.repair_handler.repair(
                candidate,
                diagnostics=diagnostics,
                stage=f"{stage_prefix}.preset.final",
            )
            validation = self.validator.validate(candidate)
            if not validation.valid:
                candidate = self.repair_handler.repair(
                    candidate,
                    diagnostics=diagnostics,
                    stage=f"{stage_prefix}.preset.retry",
                )
                validation = self.validator.validate(candidate)
            return candidate if validation.valid else None
        except Exception:
            return None

    def _preset_has_blocking_contract_issue(self, preset: dict[str, Any]) -> bool:
        """Return whether a loaded preset has catalog or root fields that must fallback."""
        if any(field in preset for field in FORBIDDEN_ROOT_FIELDS):
            return True
        obstacles = preset.get("obstacles")
        if not isinstance(obstacles, dict):
            return False
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            return False
        return any(self._placement_has_invalid_prop(placement) for placement in placements)

    def _placement_has_invalid_prop(self, placement: object) -> bool:
        """Return whether a preset placement references a prop outside the validator catalog."""
        if not isinstance(placement, dict):
            return False
        kind = placement.get("kind")
        if kind not in {"fixed", "pattern"}:
            return False
        prop = placement.get("prop")
        return not isinstance(prop, str) or (
            prop not in ALLOWED_PROPS and prop not in LEGACY_STATIC_OBSTACLE_PROP_ALIASES
        )

    def _apply_alpha_pedestrian_policy(self, scenario: dict) -> None:
        """Keep pedestrian generation out of the external alpha scenario body."""
        scenario["pedestrians"] = {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}

    def _prefer_corridor_pose_robot_anchors(self, scenario: dict) -> None:
        """Replace abstract default robot anchors with UE-friendly corridor poses when possible."""
        corridor = scenario.get("corridor")
        if not isinstance(corridor, dict):
            return
        segments = corridor.get("segments")
        if not isinstance(segments, list) or not segments:
            return
        segment_ranges = {
            segment.get("id"): segment.get("along_range_m")
            for segment in segments
            if isinstance(segment, dict) and isinstance(segment.get("id"), str)
        }
        first_segment = str(segments[0].get("id"))
        last_segment = str(segments[-1].get("id"))
        scenario["robot"] = {
            "start": self._corridor_pose_anchor(first_segment, segment_ranges.get(first_segment), prefer_start=True),
            "goal": self._corridor_pose_anchor(last_segment, segment_ranges.get(last_segment), prefer_start=False),
        }

    def _corridor_pose_anchor(self, segment_id: str, along_range: object, *, prefer_start: bool) -> dict:
        """Build a corridor-local robot anchor inside the referenced segment range."""
        along_m = 1.0 if prefer_start else 16.0
        if isinstance(along_range, list) and len(along_range) == 2:
            start, end = along_range
            if isinstance(start, int | float) and isinstance(end, int | float):
                span = float(end) - float(start)
                inner_offset = min(1.0, max(0.5, span * 0.25))
                if span <= 4.0:
                    inner_offset = min(0.5, max(0.0, span * 0.125))
                along_m = float(start) + inner_offset if prefer_start else float(end) - inner_offset
        return {
            "type": "corridor_pose",
            "segment": segment_id,
            "along_m": along_m,
            "offset_m": 0.0,
            "lane": "walkway",
            "heading": "forward",
        }

    def _first_gate_pair(self, placements: list[object]) -> list[object]:
        """Keep the first left/right gate pair and drop duplicated gate pairs."""
        left = next((placement for placement in placements if self._placement_id(placement).endswith("left")), None)
        right = next((placement for placement in placements if self._placement_id(placement).endswith("right")), None)
        if left is not None and right is not None:
            return [left, right]
        return placements[:2]

    def _placement_id(self, placement: object) -> str:
        """Return a placement id for quality postprocessing comparisons."""
        if isinstance(placement, dict) and isinstance(placement.get("id"), str):
            return placement["id"]
        return ""

    def _summary_from_template(self, template: dict, fallback: str) -> str:
        """Prefer template intent text for summaries and fall back to deterministic plan text."""
        summary = template.get("summary")
        if isinstance(summary, str) and summary:
            return summary
        intent = template.get("intent")
        if isinstance(intent, str) and intent:
            return intent
        return fallback
