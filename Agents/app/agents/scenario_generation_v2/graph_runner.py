from __future__ import annotations

import warnings
from typing import Any

from app.agents.common.llm_json_client import AgentLlmClient
from app.agents.scenario_generation_v2.agent import ScenarioGenerationV2Agent
from app.agents.scenario_generation_v2.graph_state import ScenarioGenerationGraphStateV2
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request, ScenarioGenerateV2Response, V2ValidationIssue, V2ValidationResult
from app.core.settings import Settings

try:
    from langgraph.graph import END, START, StateGraph
except ImportError:  # pragma: no cover - depends on optional local dependency
    END = START = None
    StateGraph = None


class ScenarioGenerationGraphRunnerV2:
    """Runs prompt-only project scenario generation through a compiled LangGraph graph."""

    def __init__(
        self,
        *,
        settings: Settings | None = None,
        fallback_agent: ScenarioGenerationV2Agent | None = None,
        llm_client: AgentLlmClient | None = None,
    ) -> None:
        """Own the reusable agent components and compile the graph when LangGraph is available."""
        self.settings = settings or Settings()
        self.fallback_agent = fallback_agent
        self.agent = fallback_agent or ScenarioGenerationV2Agent(settings=self.settings, llm_client=llm_client)
        self.compiled_graph: Any | None = self._compile_graph() if StateGraph is not None else None
        self.last_state: ScenarioGenerationGraphStateV2 = {}

    def run(self, request: ScenarioGenerateV2Request) -> ScenarioGenerateV2Response:
        """Invoke the compiled graph and return its response object."""
        if self.compiled_graph is None:
            warnings.warn(
                "LangGraph is not installed; falling back to ScenarioGenerationV2Agent.",
                RuntimeWarning,
                stacklevel=2,
            )
            return self.agent.generate(request)

        final_state = self.compiled_graph.invoke(self._init_state(request))
        self.last_state = final_state
        output = final_state.get("output")
        if isinstance(output, ScenarioGenerateV2Response):
            return output
        return self.agent.response_builder.failed(
            summary="시나리오 템플릿 생성 그래프 응답을 만들지 못했습니다.",
            validation=V2ValidationResult(
                valid=False,
                errors=[V2ValidationIssue(field="output", message="LangGraph output이 응답 모델이 아닙니다.")],
                warnings=[],
            ),
        )

    def _compile_graph(self) -> Any:
        """Build and compile the scenario generation StateGraph."""
        graph = StateGraph(ScenarioGenerationGraphStateV2)
        graph.add_node("validate_request_node", self.validate_request_node)
        graph.add_node("interpret_user_prompt_node", self.interpret_user_prompt_node)
        graph.add_node("select_scenario_pattern_node", self.select_scenario_pattern_node)
        graph.add_node("build_scenario_template_node", self.build_scenario_template_node)
        graph.add_node("validate_scenario_template_node", self.validate_scenario_template_node)
        graph.add_node("repair_scenario_template_node", self.repair_scenario_template_node)
        graph.add_node("fallback_scenario_template_node", self.fallback_scenario_template_node)
        graph.add_node("build_response_node", self.build_response_node)

        graph.add_edge(START, "validate_request_node")
        graph.add_edge("validate_request_node", "interpret_user_prompt_node")
        graph.add_edge("interpret_user_prompt_node", "select_scenario_pattern_node")
        graph.add_edge("select_scenario_pattern_node", "build_scenario_template_node")
        graph.add_edge("build_scenario_template_node", "validate_scenario_template_node")
        graph.add_conditional_edges(
            "validate_scenario_template_node",
            self.route_validation_node,
            {
                "valid": "build_response_node",
                "repair": "repair_scenario_template_node",
                "fallback": "fallback_scenario_template_node",
            },
        )
        graph.add_edge("repair_scenario_template_node", "validate_scenario_template_node")
        graph.add_edge("fallback_scenario_template_node", "validate_scenario_template_node")
        graph.add_edge("build_response_node", END)
        return graph.compile()

    def _init_state(self, request: ScenarioGenerateV2Request) -> ScenarioGenerationGraphStateV2:
        """Create the initial prompt-only graph state."""
        return {
            "request": request.model_dump(),
            "prompt": request.prompt,
            "interpreted_intent": None,
            "selected_pattern": None,
            "llm_template_candidate": None,
            "llm_validation": None,
            "llm_warnings": [],
            "scenario": None,
            "validation": None,
            "diagnostics": [],
            "repair_count": 0,
            "status": None,
            "summary": None,
            "assumptions": [],
            "response": None,
            "output": None,
        }

    def validate_request_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Validate that the graph received the existing prompt-only request contract."""
        prompt = state.get("prompt")
        if not isinstance(prompt, str) or not prompt.strip():
            validation = V2ValidationResult(
                valid=False,
                errors=[V2ValidationIssue(field="prompt", message="prompt가 필요합니다.")],
                warnings=[],
            )
            return {**state, "status": "failed", "validation": validation}
        return {**state, "prompt": prompt}

    def interpret_user_prompt_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Parse prompt signals and optionally collect an LLM-assisted template candidate."""
        if state.get("status") == "failed":
            return state
        normalized = self.agent.normalizer.normalize(str(state.get("prompt", "")))
        llm_update = self._llm_template_candidate_update(normalized.normalized_prompt) if self.settings.v2AgentLlmEnabled else {}
        if self._has_valid_llm_candidate(llm_update):
            candidate = llm_update["llm_template_candidate"]
            return {
                **state,
                "prompt": normalized.normalized_prompt,
                "interpreted_intent": None,
                "selected_pattern": candidate.get("scenario_id"),
                **llm_update,
            }
        intent = self.agent.intent_parser.parse(normalized.normalized_prompt)
        return {**state, "prompt": normalized.normalized_prompt, "interpreted_intent": intent, **llm_update}

    def select_scenario_pattern_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Select one supported deterministic alpha pattern."""
        if state.get("status") == "failed":
            return state
        if self._state_has_valid_llm_candidate(state):
            candidate = state["llm_template_candidate"]
            return {**state, "selected_pattern": candidate.get("scenario_id") or state.get("selected_pattern")}
        intent = state.get("interpreted_intent")
        scenario_type = self.agent.type_selector.select(intent)
        return {**state, "selected_pattern": scenario_type}

    def build_scenario_template_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Build a project scenario v1 object without sample or runtime payload ownership."""
        if state.get("status") == "failed":
            return state
        llm_candidate = state.get("llm_template_candidate")
        llm_validation = state.get("llm_validation")
        if isinstance(llm_candidate, dict) and llm_validation is not None and llm_validation.valid:
            summary = self.agent._summary_from_template(llm_candidate, "LLM-assisted graph node가 scenario를 생성했습니다.")
            assumptions = [
                *state.get("assumptions", []),
                "LangGraph 내부 LLM-assisted node 결과를 validator 통과 후 사용했습니다.",
            ]
            return {
                **state,
                "scenario": llm_candidate,
                "selected_pattern": llm_candidate.get("scenario_id") or state.get("selected_pattern"),
                "summary": summary,
                "assumptions": assumptions,
            }
        intent = state.get("interpreted_intent")
        scenario_type = str(state.get("selected_pattern") or "narrow_sidewalk_cross_path")
        plan = self.agent.planner.plan(intent, scenario_type)
        scenario = self.agent.writer.write(plan)
        return {
            **state,
            "scenario": scenario,
            "summary": plan.summary,
            "assumptions": plan.assumptions,
        }

    def validate_scenario_template_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Validate the generated project scenario and expose diagnostics to the graph."""
        if state.get("status") == "failed" and state.get("validation") is not None:
            return state
        scenario = state.get("scenario")
        validation = self.agent.validator.validate(scenario or {})
        validation.warnings.extend(state.get("llm_warnings", []))
        diagnostics = [
            *[
                {"level": "error", "field": issue.field, "message": issue.message}
                for issue in validation.errors
            ],
            *[
                {"level": "warning", "field": issue.field, "message": issue.message}
                for issue in validation.warnings
            ],
        ]
        return {
            **state,
            "validation": validation,
            "diagnostics": diagnostics,
            "status": "success" if validation.valid else "failed",
        }

    def _llm_template_candidate_update(self, prompt: str) -> ScenarioGenerationGraphStateV2:
        """Call the LLM inside the graph and return validated candidate state fields."""
        warning = V2ValidationIssue(
            field="scenario",
            message="LLM output validation failed; deterministic fallback scenario was used.",
        )
        try:
            candidate = self.agent._generate_llm_template(prompt, response_name="scenario_graph_intent")
            candidate = self.agent.repair_handler.repair(candidate)
            intent = self.agent.intent_parser.parse(prompt)
            candidate = self.agent._postprocess_scenario_for_intent(candidate, intent)
            validation = self.agent.validator.validate(candidate)
            if validation.valid:
                return {
                    "llm_template_candidate": candidate,
                    "llm_validation": validation,
                    "llm_warnings": [],
                    "diagnostics": [],
                }
            diagnostics = [
                {"level": "warning", "field": issue.field, "message": issue.message}
                for issue in validation.errors
            ]
            repaired_update = self._llm_repair_candidate_update(prompt, candidate, validation, diagnostics)
            if repaired_update is not None:
                return repaired_update
            return {
                "llm_template_candidate": candidate,
                "llm_validation": validation,
                "llm_warnings": [warning],
                "diagnostics": diagnostics,
            }
        except Exception as exc:
            return {
                "llm_template_candidate": None,
                "llm_validation": None,
                "llm_warnings": [
                    V2ValidationIssue(
                        field="scenario",
                        message=f"LLM call failed; deterministic fallback scenario was used: {type(exc).__name__}",
                    )
                ],
                "diagnostics": [
                    {"level": "warning", "field": "llm", "message": "LLM-assisted graph node failed."}
                ],
            }

    def _llm_repair_candidate_update(
        self,
        prompt: str,
        candidate: dict[str, Any],
        validation: V2ValidationResult,
        diagnostics: list[dict[str, Any]],
    ) -> ScenarioGenerationGraphStateV2 | None:
        """Try one validator-error-guided LLM repair for an invalid LLM candidate."""
        if not self.settings.v2AgentLlmRepairEnabled or self.settings.v2AgentLlmMaxRepairAttempts <= 0:
            return None
        try:
            repaired = self.agent._repair_llm_template(
                prompt,
                candidate,
                validation,
                response_name="scenario_graph_repair",
            )
            repaired = self.agent.repair_handler.repair(repaired)
            intent = self.agent.intent_parser.parse(prompt)
            repaired = self.agent._postprocess_scenario_for_intent(repaired, intent)
            repaired_validation = self.agent.validator.validate(repaired)
            repair_diagnostics = [
                *diagnostics,
                {"level": "repair", "field": "scenario", "message": "LLM-assisted repair was attempted."},
            ]
            if repaired_validation.valid:
                return {
                    "llm_template_candidate": repaired,
                    "llm_validation": repaired_validation,
                    "llm_warnings": [
                        V2ValidationIssue(
                            field="scenario",
                            message="LLM-assisted repair produced a valid scenario.",
                        )
                    ],
                    "diagnostics": repair_diagnostics,
                }
            return {
                "llm_template_candidate": candidate,
                "llm_validation": validation,
                "llm_warnings": [
                    V2ValidationIssue(
                        field="scenario",
                        message="LLM-assisted repair failed; deterministic fallback scenario was used.",
                    )
                ],
                "diagnostics": [
                    *repair_diagnostics,
                    *[
                        {"level": "warning", "field": issue.field, "message": issue.message}
                        for issue in repaired_validation.errors
                    ],
                ],
            }
        except Exception:
            return {
                "llm_template_candidate": candidate,
                "llm_validation": validation,
                "llm_warnings": [
                    V2ValidationIssue(
                        field="scenario",
                        message="LLM-assisted repair failed; deterministic fallback scenario was used.",
                    )
                ],
                "diagnostics": [
                    *diagnostics,
                    {"level": "warning", "field": "llm_repair", "message": "LLM-assisted repair failed."},
                ],
            }

    def _has_valid_llm_candidate(self, update: dict[str, Any]) -> bool:
        """Return whether a partial graph update contains a validator-approved LLM candidate."""
        validation = update.get("llm_validation")
        return isinstance(update.get("llm_template_candidate"), dict) and validation is not None and validation.valid

    def _state_has_valid_llm_candidate(self, state: ScenarioGenerationGraphStateV2) -> bool:
        """Return whether current graph state can skip deterministic pattern selection."""
        validation = state.get("llm_validation")
        return isinstance(state.get("llm_template_candidate"), dict) and validation is not None and validation.valid

    def route_validation_node(self, state: ScenarioGenerationGraphStateV2) -> str:
        """Route valid templates to response, invalid templates to repair or fallback."""
        validation = state.get("validation")
        if validation is not None and validation.valid:
            return "valid"
        if int(state.get("repair_count") or 0) < 2:
            return "repair"
        return "fallback"

    def repair_scenario_template_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Apply deterministic local scenario repair and record the attempt."""
        repaired = self.agent.repair_handler.repair(state.get("scenario") or {})
        repair_count = int(state.get("repair_count") or 0) + 1
        diagnostics = [
            *state.get("diagnostics", []),
            {"level": "repair", "field": "scenario", "message": "deterministic repair was applied."},
        ]
        return {**state, "scenario": repaired, "repair_count": repair_count, "diagnostics": diagnostics}

    def fallback_scenario_template_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Build the deterministic fallback scenario after repair attempts are exhausted."""
        prompt = str(state.get("prompt") or "")
        fallback_intent = self.agent.intent_parser.parse(prompt)
        fallback_plan = self.agent.planner.plan(fallback_intent, "narrow_sidewalk_cross_path")
        fallback_scenario = self.agent.writer.write(fallback_plan)
        assumptions = [
            *state.get("assumptions", []),
            "validation 실패 후 deterministic fallback scenario를 사용했습니다.",
        ]
        validation = self.agent.validator.validate(fallback_scenario)
        if not validation.warnings:
            validation.warnings.append(
                V2ValidationIssue(field="scenario", message="deterministic fallback scenario was used.")
            )
        return {
            **state,
            "scenario": fallback_scenario,
            "validation": validation,
            "repair_count": 2,
            "selected_pattern": "narrow_sidewalk_cross_path",
            "summary": fallback_plan.summary,
            "assumptions": assumptions,
            "status": "success" if validation.valid else "failed",
        }

    def build_response_node(self, state: ScenarioGenerationGraphStateV2) -> ScenarioGenerationGraphStateV2:
        """Wrap the final scenario and validation result in the existing response model."""
        validation = state.get("validation")
        scenario = state.get("scenario")
        if validation is None or not validation.valid or not isinstance(scenario, dict):
            response = self.agent.response_builder.failed(
                summary=state.get("summary") or "시나리오 생성에 실패했습니다.",
                validation=validation
                or V2ValidationResult(
                    valid=False,
                    errors=[V2ValidationIssue(field="scenario", message="유효한 scenario가 없습니다.")],
                    warnings=[],
                ),
            )
        else:
            response = self.agent.response_builder.success(
                scenario_id=scenario["scenario_id"],
                summary=state.get("summary") or self.agent._summary_from_template(scenario, "scenario를 생성했습니다."),
                scenario=scenario,
                validation=validation,
                assumptions=state.get("assumptions", []),
                generation_mode="langgraph",
            )
        return {**state, "response": response, "output": response}
