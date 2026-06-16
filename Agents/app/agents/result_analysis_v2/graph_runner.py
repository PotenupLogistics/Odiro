from __future__ import annotations

from pathlib import Path

from app.agents.result_analysis_v2.agent import ResultAnalysisV2Agent
from app.agents.result_analysis_v2.graph_state import ResultAnalysisGraphStateV2
from app.core.settings import Settings

try:
    from langgraph.graph import StateGraph
except ImportError:  # pragma: no cover - depends on optional local dependency
    StateGraph = None


class ResultAnalysisGraphRunnerV2:
    def __init__(
        self,
        *,
        settings: Settings | None = None,
        experiments_root: Path | None = None,
        fallback_agent: ResultAnalysisV2Agent | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.experiments_root = experiments_root
        self.fallback_agent = fallback_agent
        self.agent = fallback_agent or ResultAnalysisV2Agent(experiments_root=experiments_root, settings=self.settings)
        self.last_state: ResultAnalysisGraphStateV2 = {}

    def run(self, request=None):
        state = self._init_state(request)
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
            self.build_rag_queries_node,
            self.retrieve_rag_context_node,
            self.build_analysis_context_node,
            self.analyze_failure_node,
            self.generate_recommendations_node,
            self.validate_recommendations_node,
            self.route_recommendation_validation_node,
            self.build_response_node,
        ):
            state = node(state)
        self.last_state = state
        return state["response"]

    def _init_state(self, request=None) -> ResultAnalysisGraphStateV2:
        return {
            "request": request,
            "experiments_root": self.experiments_root or self.agent._default_root(),
            "warnings": [],
            "parse_warnings": [],
            "validation_errors": [],
            "analysis_mode": "rule_based",
            "overall_judgement": "insufficient_data",
        }

    def scan_workspace_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        scan = self.agent.scanner.scan(Path(state["experiments_root"]))
        return {
            **state,
            "scan": scan,
            "artifacts": scan.files,
            "warnings": [*state.get("warnings", []), *scan.warnings],
        }

    def classify_artifacts_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        root = Path(state["experiments_root"])
        classified = [self.agent.classifier.classify(root, path) for path in state.get("artifacts", [])]
        return {**state, "classified_artifacts": classified}

    def parse_artifacts_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        parsed = [self.agent.parser.parse(info) for info in state.get("classified_artifacts", [])]
        parse_warnings = [warning for artifact in parsed for warning in artifact.warnings]
        return {
            **state,
            "parsed_artifacts": parsed,
            "parse_warnings": parse_warnings,
            "warnings": [*state.get("warnings", []), *parse_warnings],
        }

    def extract_episode_metrics_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        episodes = self.agent._extract_episodes(state.get("parsed_artifacts", []))
        return {**state, "episode_metrics": episodes}

    def build_event_timelines_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        episode_dicts = [self.agent._episode_as_dict(episode) for episode in state.get("episode_metrics", [])]
        timelines = self.agent.timeline_builder.build_episode_timelines(
            parsed_artifacts=self.agent._timeline_inputs(state.get("parsed_artifacts", [])),
            episode_metrics=episode_dicts,
        )
        return {**state, "episode_timelines": timelines}

    def select_representative_failed_episodes_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        episode_dicts = [self.agent._episode_as_dict(episode) for episode in state.get("episode_metrics", [])]
        representative = self.agent.representative_selector.select(
            episode_metrics=episode_dicts,
            episode_timelines=state.get("episode_timelines", []),
        )
        return {**state, "representative_failed_episodes": representative}

    def aggregate_runs_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        return {**state, "run_aggregates": self.agent.run_aggregator.aggregate(state.get("episode_metrics", []))}

    def aggregate_experiments_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        aggregates = self.agent.experiment_aggregator.aggregate(state.get("run_aggregates", []))
        return {**state, "experiment_aggregates": aggregates}

    def detect_failure_patterns_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        patterns = self.agent.pattern_detector.detect(state.get("episode_metrics", []))
        return {**state, "failure_patterns": patterns}

    def route_analysis_need_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        if not state.get("episode_metrics", []):
            judgement = "insufficient_data"
        elif state.get("failure_patterns", []):
            judgement = "patterns_found"
        else:
            judgement = "no_change_needed"
        return {**state, "overall_judgement": judgement}

    def build_rag_queries_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        queries = self.agent.rag_query_builder.build_queries(
            failure_patterns=state.get("failure_patterns", []),
            experiment_aggregates=state.get("experiment_aggregates", []),
            representative_failed_episodes=state.get("representative_failed_episodes", []),
        )
        return {**state, "rag_queries": queries}

    def retrieve_rag_context_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        retrieved = self.agent.rag_retriever.retrieve(state.get("rag_queries", []))
        rag_context = self.agent.rag_context_builder.build_context(
            queries=state.get("rag_queries", []),
            retrieved_contexts=retrieved,
        )
        return {**state, "retrieved_context": retrieved, "rag_context": rag_context}

    def build_analysis_context_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        parsed = state.get("parsed_artifacts", [])
        episodes = state.get("episode_metrics", [])
        experiment_ids = {artifact.info.experiment_id for artifact in parsed if artifact.info.experiment_id}
        run_ids = {(episode.experiment_id, episode.run_id) for episode in episodes}
        context = self.agent.context_builder.build(
            experiments_count=len(experiment_ids),
            runs_count=len(run_ids),
            episodes_count=len(episodes),
            experiment_summaries=state.get("experiment_aggregates", []),
            run_summaries=state.get("run_aggregates", []),
            failure_patterns=state.get("failure_patterns", []),
            episode_timelines=state.get("episode_timelines", []),
            representative_failed_episodes=state.get("representative_failed_episodes", []),
            rag_context=state.get("rag_context", {}),
        )
        return {**state, "analysis_context": context}

    def analyze_failure_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        if not self.settings.v2AgentLlmEnabled:
            analysis = self.agent.llm_analyzer.analyze(state.get("analysis_context", {}))
            return {**state, "llm_analysis": analysis, "analysis_mode": "rule_based"}

        try:
            payload = (self.agent.llm_client or self.agent._llm_client()).generate_json(
                system_prompt=self.agent._read_prompt("system_prompt.md"),
                user_prompt=self.agent._analysis_user_prompt(state.get("analysis_context", {})),
                response_name="analysis_recommendations_v2",
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
        recommendations = self.agent.recommendation_generator.generate(
            state.get("llm_analysis", {}),
            state.get("failure_patterns", []),
        )
        return {**state, "recommendations": recommendations}

    def validate_recommendations_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        refs = self._known_episode_refs(state)
        recommendations = state.get("recommendations", [])
        validated = self.agent.recommendation_validator.validate(recommendations, refs)
        errors = []
        if len(validated) != len(recommendations):
            errors.append("Recommendation validation failed.")
        return {**state, "recommendations": validated, "validation_errors": errors}

    def route_recommendation_validation_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        if not state.get("validation_errors"):
            mode = "llm" if state.get("analysis_mode") == "llm_candidate" else state.get("analysis_mode", "rule_based")
            return {**state, "analysis_mode": mode}

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
        }

    def build_response_node(self, state: ResultAnalysisGraphStateV2) -> ResultAnalysisGraphStateV2:
        parsed = state.get("parsed_artifacts", [])
        episodes = state.get("episode_metrics", [])
        experiment_ids = {artifact.info.experiment_id for artifact in parsed if artifact.info.experiment_id}
        run_ids = {(episode.experiment_id, episode.run_id) for episode in episodes}
        response = self.agent.response_builder.build(
            experiments_count=len(experiment_ids),
            runs_count=len(run_ids),
            episodes_count=len(episodes),
            metrics=self.agent._totals(episodes),
            patterns=state.get("failure_patterns", []),
            recommendations=state.get("recommendations", []),
            warnings=state.get("warnings", []),
            analysis_mode=state.get("analysis_mode", "rule_based"),
        )
        return {
            **state,
            "response": response,
            "modified_policy_json": response.modified_policy_json,
            "modified_environment_json": response.modified_environment_json,
        }

    def _known_episode_refs(self, state: ResultAnalysisGraphStateV2) -> set[tuple[str, str, str]]:
        return {
            (episode.experiment_id, episode.run_id, episode.episode_id)
            for episode in state.get("episode_metrics", [])
        }
