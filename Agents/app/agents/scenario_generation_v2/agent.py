from __future__ import annotations

import json
from copy import deepcopy
from pathlib import Path
from typing import Any

from app.agents.common.llm_json_client import AgentLlmClient, AgentLlmJsonClient
from app.agents.common.spec_context_loader import SpecContextLoader
from app.core.settings import Settings
from app.agents.scenario_generation_v2.intent_parser import IntentParser, ScenarioIntent
from app.agents.scenario_generation_v2.repair_handler import RepairHandler
from app.agents.scenario_generation_v2.request_normalizer import RequestNormalizer
from app.agents.scenario_generation_v2.response_builder import ResponseBuilder
from app.agents.scenario_generation_v2.scenario_template_schema import project_scenario_v1_response_schema
from app.agents.scenario_generation_v2.scenario_preset_loader import ScenarioPresetLoader
from app.agents.scenario_generation_v2.scenario_type_selector import ScenarioTypeSelector
from app.agents.scenario_generation_v2.template_json_writer import TemplateJsonWriter
from app.agents.scenario_generation_v2.template_planner import TemplatePlanner
from app.agents.scenario_generation_v2.template_validator import TemplateValidator
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
        self.normalizer = RequestNormalizer()
        self.intent_parser = IntentParser()
        self.type_selector = ScenarioTypeSelector()
        self.planner = TemplatePlanner()
        self.writer = TemplateJsonWriter()
        self.validator = TemplateValidator()
        self.repair_handler = RepairHandler()
        self.response_builder = ResponseBuilder()

    def generate(self, request: ScenarioGenerateV2Request) -> ScenarioGenerateV2Response:
        """Generate a project scenario JSON object from a single natural-language prompt."""
        normalized = self.normalizer.normalize(request.prompt)
        if self.settings.v2AgentLlmEnabled:
            response = self._generate_with_llm(normalized.normalized_prompt)
            if response is not None:
                return response

        intent = self.intent_parser.parse(normalized.normalized_prompt)
        scenario_type = self.type_selector.select(intent)
        plan = self.planner.plan(intent, scenario_type)

        if self.settings.v2AgentLlmEnabled:
            return self._generate_deterministic(
                plan,
                intent=intent,
                generation_mode="fallback",
                fallback_warning=V2ValidationIssue(
                    field="scenario",
                    message="LLM output validation failed; deterministic fallback scenario was used.",
                ),
            )
        return self._generate_deterministic(plan, intent=intent, generation_mode="deterministic")

    def _generate_deterministic(
        self,
        plan,
        *,
        intent: ScenarioIntent,
        generation_mode: str,
        fallback_warning: V2ValidationIssue | None = None,
    ) -> ScenarioGenerateV2Response:
        scenario = self.writer.write(plan)
        scenario = self.repair_handler.repair(scenario)
        scenario = self._postprocess_scenario_for_intent(scenario, intent)
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
    ) -> ScenarioGenerateV2Response | None:
        """Try LLM generation and at most one repair before deterministic fallback."""
        client = self.llm_client or AgentLlmJsonClient(settings=self.settings)
        try:
            scenario = self._generate_llm_template(prompt, client=client, response_name="scenario")
            intent = self.intent_parser.parse(prompt)
            scenario = self.repair_handler.repair(scenario)
            scenario = self._postprocess_scenario_for_intent(scenario, intent)
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
                repaired = self.repair_handler.repair(repaired)
                intent = self.intent_parser.parse(prompt)
                repaired = self._postprocess_scenario_for_intent(repaired, intent)
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

    def _postprocess_scenario_for_intent(self, scenario: dict, intent: ScenarioIntent) -> dict:
        """Apply prompt-specific quality corrections after structured LLM generation."""
        self._apply_alpha_pedestrian_policy(scenario)
        self._prefer_corridor_pose_robot_anchors(scenario)
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
        if intent.corridor_profile == "curved-road":
            self._apply_curved_road_preset_policy(scenario, intent)
        return scenario

    def _apply_alpha_pedestrian_policy(self, scenario: dict) -> None:
        """Keep pedestrian generation out of the external alpha scenario body."""
        scenario["pedestrians"] = {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}

    def _apply_curved_road_preset_policy(self, scenario: dict, intent: ScenarioIntent) -> None:
        """Force curved-road intent to use the bundled curved-road corridor contract."""
        preset = self.scenario_preset_loader.load_scenario_preset("curved-road")
        scenario["schema"] = "scenario"
        scenario["version"] = 1
        scenario["corridor"] = deepcopy(preset["corridor"])
        scenario["robot"] = deepcopy(preset["robot"])
        self._apply_alpha_pedestrian_policy(scenario)

        has_obstacle_intent = self._has_static_obstacle_intent(intent)
        scenario["scenario_id"] = "curved_road_static_obstacle" if has_obstacle_intent else "curved_road_sidewalk"
        if not isinstance(scenario.get("intent"), str) or not scenario["intent"]:
            scenario["intent"] = str(preset.get("intent") or "Evaluate route following on a curved road sidewalk.")
        self._normalize_curved_road_obstacles(scenario, preset, intent, include_obstacle=has_obstacle_intent)

    def _has_static_obstacle_intent(self, intent: ScenarioIntent) -> bool:
        """Return whether the prompt asked for static obstacle placement."""
        return "static_obstacle_ahead" in intent.risk_factors or intent.requested_gate_obstacle_count == 2

    def _normalize_curved_road_obstacles(
        self,
        scenario: dict,
        preset: dict[str, Any],
        intent: ScenarioIntent,
        *,
        include_obstacle: bool,
    ) -> None:
        """Map obstacle rules onto the curved-road preset segment ids and ranges."""
        preset_obstacles = preset.get("obstacles") if isinstance(preset.get("obstacles"), dict) else {}
        source_obstacles = scenario.get("obstacles") if isinstance(scenario.get("obstacles"), dict) else {}
        min_clear_width = source_obstacles.get("min_clear_width_m", preset_obstacles.get("min_clear_width_m", 0.9))
        if not include_obstacle:
            scenario["obstacles"] = {"min_clear_width_m": min_clear_width, "placements": []}
            return

        source_placements = source_obstacles.get("placements")
        placements = [deepcopy(placement) for placement in source_placements if isinstance(placement, dict)] if isinstance(source_placements, list) else []
        if not placements:
            placements = [self._default_curved_road_obstacle(intent)]

        road_curve_range = self._segment_range(scenario["corridor"], "road_curve")
        scenario["obstacles"] = {
            "min_clear_width_m": min_clear_width,
            "placements": [
                self._remap_obstacle_to_curved_road(placement, road_curve_range)
                for placement in placements
            ],
        }

    def _default_curved_road_obstacle_along_range(self) -> dict[str, float]:
        """Return the stable obstacle band used for curved-road demo scenarios."""
        return {"min": 6.5, "max": 8.5}

    def _default_curved_road_obstacle(self, intent: ScenarioIntent) -> dict[str, Any]:
        """Build the default static obstacle used when a curved-road LLM candidate omits one."""
        placement: dict[str, Any] = {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "obstacle.crate_01",
            "at": {
                "segment": "road_curve",
                "along_m": self._default_curved_road_obstacle_along_range(),
                "offset_m": {"min": 0.45, "max": 0.75},
                "lane": "walkway",
            },
            "yaw_deg": 0,
        }
        if intent.explicit_blocking:
            placement["allow_blocking"] = True
        return placement

    def _remap_obstacle_to_curved_road(
        self,
        placement: dict[str, Any],
        road_curve_range: tuple[float, float],
    ) -> dict[str, Any]:
        """Rewrite obstacle placement anchors to the curved-road conflict segment."""
        kind = placement.get("kind")
        if kind in {"fixed", "pattern"}:
            at = placement.get("at")
            if not isinstance(at, dict):
                at = {}
            at["segment"] = "road_curve"
            at["along_m"] = self._curved_road_obstacle_along_value(at.get("along_m"), road_curve_range)
            at.setdefault("offset_m", 0.0)
            at.setdefault("lane", "walkway")
            placement["at"] = at
        elif kind == "scatter":
            zone = placement.get("zone")
            if not isinstance(zone, dict):
                zone = {}
            zone["segments"] = ["road_curve"]
            placement["zone"] = zone
        return placement

    def _segment_range(self, corridor: dict[str, Any], segment_id: str) -> tuple[float, float]:
        """Return the fixed along-range for a segment in a scenario corridor."""
        for segment in corridor.get("segments", []):
            if not isinstance(segment, dict) or segment.get("id") != segment_id:
                continue
            along_range = segment.get("along_range_m")
            if isinstance(along_range, list) and len(along_range) == 2:
                start, end = along_range
                if isinstance(start, int | float) and isinstance(end, int | float):
                    return (float(start), float(end))
        return (4.0, 9.6)

    def _curved_road_obstacle_along_value(
        self,
        value: object,
        allowed_range: tuple[float, float],
    ) -> object:
        """Keep valid curved-road obstacle positions while avoiding collapsed edge ranges."""
        minimum, maximum = allowed_range
        if isinstance(value, dict):
            value_min = value.get("min")
            value_max = value.get("max")
            if isinstance(value_min, int | float) and isinstance(value_max, int | float):
                clamped_min = min(max(float(value_min), minimum), maximum)
                clamped_max = min(max(float(value_max), minimum), maximum)
                if clamped_min < clamped_max:
                    return {"min": clamped_min, "max": clamped_max}
                if clamped_min == clamped_max and clamped_min not in {minimum, maximum}:
                    return {"min": clamped_min, "max": clamped_max}
        if isinstance(value, int | float):
            return min(max(float(value), minimum), maximum)
        return self._default_curved_road_obstacle_along_range()

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
                along_m = float(start) + min(2.0, max(0.5, span * (0.25 if prefer_start else 0.75)))
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
