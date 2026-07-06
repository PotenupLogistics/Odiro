from __future__ import annotations

from pathlib import Path
from typing import Any

from app.agents.result_analysis_v2.agent import ResultAnalysisV2Agent
from app.agents.result_analysis_v2.graph_state import ResultAnalysisGraphStateV2
from app.agents.result_analysis_v2.routes import (
    ANALYSIS_ROUTE_INSUFFICIENT_DATA,
    ANALYSIS_ROUTE_NO_CHANGE_NEEDED,
    ANALYSIS_ROUTE_PATTERNS_FOUND,
    RAG_ROUTE_SKIPPED,
    RECOMMENDATION_ROUTE_NONE,
    RECOMMENDATION_ROUTE_RULE_BASED_FALLBACK,
    RECOMMENDATION_VALIDATION_FALLBACK,
    RECOMMENDATION_VALIDATION_VALID,
    AnalysisRouteV2,
    RecommendationValidationRouteV2,
    decide_analysis_route,
    default_rag_route_for_analysis,
    default_recommendation_route_for_analysis,
    route_for_valid_recommendations,
)
from app.agents.result_analysis_v2.recommendation_schema import analysis_recommendations_v2_response_schema
from app.core.settings import Settings
from app.models.analysis_v2 import AnalysisRunV2Response

try:
    from langgraph.graph import END, START, StateGraph
except ImportError:  # pragma: no cover - depends on optional local dependency
    END = START = None
    StateGraph = None


class ResultAnalysisGraphRunnerV2:
    """Run result-analysis v2 through LangGraph with a route-equivalent fallback."""

    def __init__(
        self,
        *,
        settings: Settings | None = None,
        experiments_root: Path | None = None,
        fallback_agent: ResultAnalysisV2Agent | None = None,
    ) -> None:
        """Own the reusable analysis agent and compile the graph when available."""
        self.settings = settings or Settings()
        self.experiments_root = experiments_root
        self.fallback_agent = fallback_agent
        self.agent = fallback_agent or ResultAnalysisV2Agent(experiments_root=experiments_root, settings=self.settings)
        self.compiled_graph: Any | None = self._compile_graph() if StateGraph is not None else None
        self.last_state: ResultAnalysisGraphStateV2 = {}

    def run(self, request=None):
        """Invoke the compiled graph or route-equivalent sequential fallback."""
        review_session = self.agent.review_lifecycle.start(request)
        try:
            state = self._init_state(request)
            state = self.compiled_graph.invoke(state) if self.compiled_graph is not None else self._run_sequential(state)
            self.agent._complete_review_if_started(
                session=review_session,
                request=request,
                response=state["response"],
                parsed=state.get("parsed_artifacts", []),
                episodes=state.get("episode_metrics", []),
                warnings=state.get("warnings", []),
                detailed_recommendations=state.get("detailed_recommendations", []),
            )
            self.last_state = state
            return state["response"]
        except Exception as exc:
            if review_session is not None:
                failure_response = self.processing_failed_response(
                    run_id=request.run_id if request is not None else None,
                )
                try:
                    self.agent.review_lifecycle.write_response(session=review_session, response=failure_response)
                except Exception:
                    pass
                self.agent.review_lifecycle.fail(
                    session=review_session,
                    code=exc.__class__.__name__,
                    message=str(exc),
                )
            raise

    @staticmethod
    def processing_failed_response(*, run_id: str | None) -> AnalysisRunV2Response:
        """Build the public failure body shared by the API and review response artifact."""
        return AnalysisRunV2Response(
            status="failed",
            run_id=run_id,
            error={
                "code": "ANALYSIS_PROCESSING_FAILED",
                "message": "분석 결과를 생성하는 중 오류가 발생했습니다.",
                "phase": "build_response",
            },
            warnings=[],
        )

    def _compile_graph(self) -> Any:
        """Build and compile the result-analysis v2 StateGraph."""
        graph = StateGraph(ResultAnalysisGraphStateV2)
        graph.add_node("scan_workspace_node", self.scan_workspace_node)
        graph.add_node("classify_artifacts_node", self.classify_artifacts_node)
        graph.add_node("parse_artifacts_node", self.parse_artifacts_node)
        graph.add_node("extract_episode_metrics_node", self.extract_episode_metrics_node)
        graph.add_node("build_event_timelines_node", self.build_event_timelines_node)
        graph.add_node("select_representative_failed_episodes_node", self.select_representative_failed_episodes_node)
        graph.add_node("aggregate_runs_node", self.aggregate_runs_node)
        graph.add_node("aggregate_experiments_node", self.aggregate_experiments_node)
        graph.add_node("detect_failure_patterns_node", self.detect_failure_patterns_node)
        graph.add_node("route_analysis_need_node", self.route_analysis_need_node)
        graph.add_node("build_rag_queries_node", self.build_rag_queries_node)
        graph.add_node("retrieve_rag_context_node", self.retrieve_rag_context_node)
        graph.add_node("build_analysis_context_node", self.build_analysis_context_node)
        graph.add_node("analyze_failure_node", self.analyze_failure_node)
        graph.add_node("generate_recommendations_node", self.generate_recommendations_node)
        graph.add_node("validate_recommendations_node", self.validate_recommendations_node)
        graph.add_node("route_recommendation_validation_node", self.route_recommendation_validation_node)
        graph.add_node("rule_based_fallback_node", self.rule_based_fallback_node)
        graph.add_node("build_response_node", self.build_response_node)

        graph.add_edge(START, "scan_workspace_node")
        graph.add_edge("scan_workspace_node", "classify_artifacts_node")
        graph.add_edge("classify_artifacts_node", "parse_artifacts_node")
        graph.add_edge("parse_artifacts_node", "extract_episode_metrics_node")
        graph.add_edge("extract_episode_metrics_node", "build_event_timelines_node")
        graph.add_edge("build_event_timelines_node", "select_representative_failed_episodes_node")
        graph.add_edge("select_representative_failed_episodes_node", "aggregate_runs_node")
        graph.add_edge("aggregate_runs_node", "aggregate_experiments_node")
        graph.add_edge("aggregate_experiments_node", "detect_failure_patterns_node")
        graph.add_edge("detect_failure_patterns_node", "route_analysis_need_node")
        graph.add_conditional_edges(
            "route_analysis_need_node",
            self.route_analysis_edge,
            {
                ANALYSIS_ROUTE_INSUFFICIENT_DATA: "build_response_node",
                ANALYSIS_ROUTE_NO_CHANGE_NEEDED: "build_response_node",
                ANALYSIS_ROUTE_PATTERNS_FOUND: "build_rag_queries_node",
            },
        )
        graph.add_edge("build_rag_queries_node", "retrieve_rag_context_node")
        graph.add_edge("retrieve_rag_context_node", "build_analysis_context_node")
        graph.add_edge("build_analysis_context_node", "analyze_failure_node")
        graph.add_edge("analyze_failure_node", "generate_recommendations_node")
        graph.add_edge("generate_recommendations_node", "validate_recommendations_node")
        graph.add_edge("validate_recommendations_node", "route_recommendation_validation_node")
        graph.add_conditional_edges(
            "route_recommendation_validation_node",
            self.route_recommendation_validation_edge,
            {
                RECOMMENDATION_VALIDATION_VALID: "build_response_node",
                RECOMMENDATION_VALIDATION_FALLBACK: "rule_based_fallback_node",
            },
        )
        graph.add_edge("rule_based_fallback_node", "build_response_node")
        graph.add_edge("build_response_node", END)
        return graph.compile()

    def _run_sequential(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Run the same node and route decisions without LangGraph installed."""
        for node in (
            self.scan_workspace_node,
            self.classify_artifacts_node,
            self.parse_artifacts_node,
            self.extract_episode_metrics_node,
            self.build_event_timelines_node,
            self.select_representative_failed_episodes_node,
            self.aggregate_runs_node,
            self.aggregate_experiments_node,
            self.detect_failure_patterns_node,
            self.route_analysis_need_node,
        ):
            state = node(state)

        if self.route_analysis_edge(state) != ANALYSIS_ROUTE_PATTERNS_FOUND:
            return self.build_response_node(state)

        for node in (
            self.build_rag_queries_node,
            self.retrieve_rag_context_node,
            self.build_analysis_context_node,
            self.analyze_failure_node,
            self.generate_recommendations_node,
            self.validate_recommendations_node,
            self.route_recommendation_validation_node,
        ):
            state = node(state)
        if self.route_recommendation_validation_edge(state) == RECOMMENDATION_VALIDATION_FALLBACK:
            state = self.rule_based_fallback_node(state)
        return self.build_response_node(state)

    def _init_state(self, request=None) -> ResultAnalysisGraphStateV2:
        """Create the initial graph state with stable route defaults."""
        return {
            "request": request,
            "experiments_root": self.experiments_root or self.agent._default_root(),
            "warnings": [],
            "parse_warnings": [],
            "validation_errors": [],
            "analysis_mode": "rule_based",
            "overall_judgement": "insufficient_data",
            "analysis_route": ANALYSIS_ROUTE_INSUFFICIENT_DATA,
            "rag_route": RAG_ROUTE_SKIPPED,
            "recommendation_route": RECOMMENDATION_ROUTE_NONE,
            "recommendation_validation_route": RECOMMENDATION_VALIDATION_VALID,
            "rag_diagnostic": {
                "enabled": True,
                "backend": "pdf_vector_hybrid",
                "used": False,
                "query_count": 0,
                "retrieved_chunk_count": 0,
                "route": RAG_ROUTE_SKIPPED,
                "fallback_reason": None,
            },
        }

    def scan_workspace_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Scan the requested workspace for run artifacts."""
        scan = self.agent.scan(state.get("request"))
        return {
            **state,
            "experiments_root": scan.root,
            "scan": scan,
            "artifacts": scan.files,
            "warnings": [*state.get("warnings", []), *scan.warnings],
        }

    def classify_artifacts_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Classify discovered artifacts relative to the selected root."""
        root = Path(state["experiments_root"])
        classified = [self.agent.classifier.classify(root, path) for path in state.get("artifacts", [])]
        return {**state, "classified_artifacts": classified}

    def parse_artifacts_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Parse classified artifacts and merge non-fatal parse warnings."""
        parsed = [self.agent.parser.parse(info) for info in state.get("classified_artifacts", [])]
        parse_warnings = [warning for artifact in parsed for warning in artifact.warnings]
        return {
            **state,
            "parsed_artifacts": parsed,
            "parse_warnings": parse_warnings,
            "warnings": [*state.get("warnings", []), *parse_warnings],
        }

    def extract_episode_metrics_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Extract normalized episode metrics from parsed run artifacts."""
        episodes = self.agent._extract_episodes(state.get("parsed_artifacts", []))
        return {**state, "episode_metrics": episodes}

    def build_event_timelines_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Build compact per-episode event timelines for analysis context."""
        episode_dicts = [self.agent._episode_as_dict(episode) for episode in state.get("episode_metrics", [])]
        timelines = self.agent.timeline_builder.build_episode_timelines(
            parsed_artifacts=self.agent._timeline_inputs(state.get("parsed_artifacts", [])),
            episode_metrics=episode_dicts,
        )
        return {**state, "episode_timelines": timelines}

    def select_representative_failed_episodes_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Select representative failed episodes for prompt and RAG query context."""
        episode_dicts = [self.agent._episode_as_dict(episode) for episode in state.get("episode_metrics", [])]
        representative = self.agent.representative_selector.select(
            episode_metrics=episode_dicts,
            episode_timelines=state.get("episode_timelines", []),
        )
        return {**state, "representative_failed_episodes": representative}

    def aggregate_runs_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Aggregate episode metrics into run-level summaries."""
        return {**state, "run_aggregates": self.agent.run_aggregator.aggregate(state.get("episode_metrics", []))}

    def aggregate_experiments_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Aggregate run summaries into experiment-level summaries."""
        aggregates = self.agent.experiment_aggregator.aggregate(state.get("run_aggregates", []))
        return {**state, "experiment_aggregates": aggregates}

    def detect_failure_patterns_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Detect repeated failure patterns from episode metrics."""
        patterns = self.agent.pattern_detector.detect(state.get("episode_metrics", []))
        return {**state, "failure_patterns": patterns}

    def route_analysis_need_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Record the analysis route and default skipped downstream routes."""
        route = decide_analysis_route(
            episode_count=len(state.get("episode_metrics", [])),
            failure_pattern_count=len(state.get("failure_patterns", [])),
        )
        return {
            **state,
            "overall_judgement": route,
            "analysis_route": route,
            "rag_route": default_rag_route_for_analysis(route),
            "recommendation_route": default_recommendation_route_for_analysis(route),
        }

    def route_analysis_edge(self, state: ResultAnalysisGraphStateV2) -> AnalysisRouteV2:
        """Return the conditional edge label after analysis need routing."""
        return state.get("analysis_route", ANALYSIS_ROUTE_INSUFFICIENT_DATA)

    def build_rag_queries_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Build internal RAG queries only when patterns were found."""
        queries = self.agent.rag_query_builder.build_queries(
            failure_patterns=state.get("failure_patterns", []),
            experiment_aggregates=state.get("experiment_aggregates", []),
            representative_failed_episodes=state.get("representative_failed_episodes", []),
        )
        return {**state, "rag_queries": queries}

    def retrieve_rag_context_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Retrieve file-based RAG context and keep diagnostic state internal."""
        retrieved = self.agent.rag_retriever.retrieve(state.get("rag_queries", []))
        rag_diagnostic = getattr(self.agent.rag_retriever, "last_diagnostic", {})
        rag_context = self.agent.rag_context_builder.build_context(
            queries=state.get("rag_queries", []),
            retrieved_contexts=retrieved,
        )
        return {
            **state,
            "retrieved_context": retrieved,
            "rag_context": rag_context,
            "rag_diagnostic": rag_diagnostic,
            "rag_route": rag_diagnostic.get("route", RAG_ROUTE_SKIPPED),
        }

    def build_analysis_context_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Build the combined context consumed by LLM or rule-based analysis."""
        parsed = state.get("parsed_artifacts", [])
        episodes = state.get("episode_metrics", [])
        experiments_count, runs_count, episodes_count = self.agent._scope_counts(parsed, episodes)
        context = self.agent.context_builder.build(
            experiments_count=experiments_count,
            runs_count=runs_count,
            episodes_count=episodes_count,
            experiment_summaries=state.get("experiment_aggregates", []),
            run_summaries=state.get("run_aggregates", []),
            failure_patterns=state.get("failure_patterns", []),
            episode_timelines=state.get("episode_timelines", []),
            representative_failed_episodes=state.get("representative_failed_episodes", []),
            rag_context=state.get("rag_context", {}),
        )
        return {**state, "analysis_context": context}

    def analyze_failure_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Analyze failures through LLM when enabled, otherwise deterministic rules."""
        if not self.settings.v2AgentLlmEnabled:
            analysis = self.agent.llm_analyzer.analyze(state.get("analysis_context", {}))
            return {**state, "llm_analysis": analysis, "analysis_mode": "rule_based"}

        try:
            payload = (self.agent.llm_client or self.agent._llm_client()).generate_json(
                system_prompt=self.agent._read_prompt("system_prompt.md"),
                user_prompt=self.agent._analysis_user_prompt(state.get("analysis_context", {})),
                response_name="analysis_recommendations_v2",
                response_schema=analysis_recommendations_v2_response_schema(),
            )
            return {**state, "llm_analysis": payload, "analysis_mode": "llm_candidate"}
        except Exception:
            warning = "LLM recommendation failed; rule-based recommendation fallback was used."
            analysis = self.agent.llm_analyzer.analyze({"failure_patterns": state.get("failure_patterns", [])})
            return {
                **state,
                "llm_analysis": analysis,
                "analysis_mode": "fallback",
                "warnings": [*state.get("warnings", []), warning],
            }

    def generate_recommendations_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Generate candidate recommendations from LLM output or patterns."""
        recommendations = self.agent.recommendation_generator.generate(
            state.get("llm_analysis", {}),
            state.get("failure_patterns", []),
        )
        return {**state, "recommendations": recommendations}

    def validate_recommendations_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Validate recommendation candidates against public and episode constraints."""
        refs = self._known_episode_refs(state)
        recommendations = state.get("recommendations", [])
        validated = self.agent.recommendation_validator.validate(recommendations, refs)
        errors = []
        if len(validated) != len(recommendations):
            errors.append("Recommendation validation failed.")
        return {**state, "recommendations": validated, "validation_errors": errors}

    def route_recommendation_validation_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Record recommendation validation routing without creating fallback content."""
        if not state.get("validation_errors"):
            analysis_mode = state.get("analysis_mode", "rule_based")
            mode = "llm" if analysis_mode == "llm_candidate" else analysis_mode
            return {
                **state,
                "analysis_mode": mode,
                "recommendation_validation_route": RECOMMENDATION_VALIDATION_VALID,
                "recommendation_route": route_for_valid_recommendations(analysis_mode=analysis_mode),
            }

        return {
            **state,
            "recommendation_validation_route": RECOMMENDATION_VALIDATION_FALLBACK,
            "recommendation_route": RECOMMENDATION_ROUTE_RULE_BASED_FALLBACK,
        }

    def route_recommendation_validation_edge(
        self,
        state: ResultAnalysisGraphStateV2,
    ) -> RecommendationValidationRouteV2:
        """Return the conditional edge label after recommendation validation."""
        return state.get("recommendation_validation_route", RECOMMENDATION_VALIDATION_VALID)

    def rule_based_fallback_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Create rule-based recommendations after invalid or unusable LLM candidates."""
        fallback_analysis = self.agent.llm_analyzer.analyze({"failure_patterns": state.get("failure_patterns", [])})
        fallback_recommendations = self.agent.recommendation_generator.generate(
            fallback_analysis,
            state.get("failure_patterns", []),
        )
        validated = self.agent.recommendation_validator.validate(fallback_recommendations, self._known_episode_refs(state))
        warning = "LLM recommendation failed; rule-based recommendation fallback was used."
        warnings = state.get("warnings", [])
        if warning not in warnings:
            warnings = [*warnings, warning]
        return {
            **state,
            "llm_analysis": fallback_analysis,
            "recommendations": validated,
            "analysis_mode": "fallback",
            "warnings": warnings,
            "validation_errors": [],
            "recommendation_route": RECOMMENDATION_ROUTE_RULE_BASED_FALLBACK,
        }

    def build_response_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        """Build the public v2 response without exposing internal graph state."""
        parsed = state.get("parsed_artifacts", [])
        episodes = state.get("episode_metrics", [])
        experiments_count, runs_count, episodes_count = self.agent._scope_counts(parsed, episodes)
        public_data = self.agent.summary_row_public_builder.build(self.agent._summary_rows(parsed))
        public_metrics = (
            self.agent._merge_episode_detail_counts(public_data.metrics, episodes)
            if public_data.has_rows
            else public_data.metrics
        )
        request = state.get("request")
        public_patterns = state.get("failure_patterns", []) if public_data.has_rows else []
        detailed_recommendations = state.get("recommendations", []) if public_data.has_rows else []
        response = self.agent.response_builder.build(
            experiments_count=experiments_count,
            runs_count=runs_count,
            episodes_count=len(public_data.episodes) if public_data.has_rows else 0,
            metrics=public_metrics,
            run_overview=public_data.run_overview,
            episodes=public_data.episodes if public_data.has_rows else None,
            patterns=public_patterns,
            recommendations=detailed_recommendations,
            warnings=state.get("warnings", []),
            analysis_mode=state.get("analysis_mode", "rule_based"),
            run_id=request.run_id if request is not None else None,
        )
        response = self.agent._with_prompt_focus(response, state.get("request"))
        return {
            **state,
            "response": response,
            "detailed_recommendations": detailed_recommendations,
        }

    def _known_episode_refs(self, state: ResultAnalysisGraphStateV2) -> set[tuple[str, str, str]]:
        """Return known episode references for recommendation evidence validation."""
        return {
            (episode.experiment_id, episode.run_id, episode.episode_id)
            for episode in state.get("episode_metrics", [])
        }
