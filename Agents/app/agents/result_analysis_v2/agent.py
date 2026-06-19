from __future__ import annotations

from collections import defaultdict
from dataclasses import asdict
from pathlib import Path
from typing import Any

from app.agents.common.llm_json_client import AgentLlmClient, AgentLlmJsonClient
from app.agents.result_analysis_v2.analysis_context_builder import AnalysisContextBuilder
from app.agents.result_analysis_v2.artifact_classifier import ArtifactClassifier
from app.agents.result_analysis_v2.artifact_parser import ArtifactParser, ParsedArtifact
from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetricExtractor, EpisodeMetrics
from app.agents.result_analysis_v2.experiment_aggregator import ExperimentAggregator
from app.agents.result_analysis_v2.failure_pattern_detector import FailurePatternDetector
from app.agents.result_analysis_v2.llm_failure_analyzer import LlmFailureAnalyzer
from app.agents.result_analysis_v2.rag_context_builder import FileBasedRagRetrieverAdapterV2, RagContextBuilderV2
from app.agents.result_analysis_v2.rag_query_builder import RagQueryBuilderV2
from app.agents.result_analysis_v2.recommendation_generator import RecommendationGenerator
from app.agents.result_analysis_v2.recommendation_validator import RecommendationValidator
from app.agents.result_analysis_v2.representative_selector import RepresentativeFailedEpisodeSelectorV2
from app.agents.result_analysis_v2.response_builder import ResponseBuilder
from app.agents.result_analysis_v2.run_aggregator import RunAggregator
from app.agents.result_analysis_v2.timeline_builder import EventTimelineBuilderV2
from app.agents.result_analysis_v2.workspace_scanner import WorkspaceScan, WorkspaceScanner
from app.core.settings import Settings
from app.models.analysis_v2 import AnalysisMetricsV2, AnalysisRunV2Request, AnalysisRunV2Response


class ResultAnalysisV2Agent:
    def __init__(
        self,
        experiments_root: Path | None = None,
        *,
        settings: Settings | None = None,
        llm_client: AgentLlmClient | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.llm_client = llm_client
        self.experiments_root = experiments_root
        self.scanner = WorkspaceScanner()
        self.classifier = ArtifactClassifier()
        self.parser = ArtifactParser()
        self.metric_extractor = EpisodeMetricExtractor()
        self.run_aggregator = RunAggregator()
        self.experiment_aggregator = ExperimentAggregator()
        self.pattern_detector = FailurePatternDetector()
        self.timeline_builder = EventTimelineBuilderV2()
        self.representative_selector = RepresentativeFailedEpisodeSelectorV2()
        self.rag_query_builder = RagQueryBuilderV2()
        self.rag_retriever = FileBasedRagRetrieverAdapterV2()
        self.rag_context_builder = RagContextBuilderV2()
        self.context_builder = AnalysisContextBuilder()
        self.llm_analyzer = LlmFailureAnalyzer()
        self.recommendation_generator = RecommendationGenerator()
        self.recommendation_validator = RecommendationValidator()
        self.response_builder = ResponseBuilder()

    def run(self, request: AnalysisRunV2Request | None = None) -> AnalysisRunV2Response:
        scan = self.scan(request)
        root = scan.root
        parsed = [self.parser.parse(self.classifier.classify(root, path)) for path in scan.files]
        warnings = [*scan.warnings]
        for artifact in parsed:
            warnings.extend(artifact.warnings)

        episodes = self._extract_episodes(parsed)
        experiments_count, runs_count, episodes_count = self._scope_counts(parsed, episodes)
        run_summaries = self.run_aggregator.aggregate(episodes)
        experiment_summaries = self.experiment_aggregator.aggregate(run_summaries)
        patterns = self.pattern_detector.detect(episodes)
        episode_metric_dicts = [asdict(episode) for episode in episodes]
        episode_timelines = self.timeline_builder.build_episode_timelines(
            parsed_artifacts=self._timeline_inputs(parsed),
            episode_metrics=episode_metric_dicts,
        )
        representative_episodes = self.representative_selector.select(
            episode_metrics=episode_metric_dicts,
            episode_timelines=episode_timelines,
        )
        rag_queries = self.rag_query_builder.build_queries(
            failure_patterns=patterns,
            experiment_aggregates=experiment_summaries,
            representative_failed_episodes=representative_episodes,
        )
        rag_context = self.rag_context_builder.build_context(
            queries=rag_queries,
            retrieved_contexts=self.rag_retriever.retrieve(rag_queries),
        )
        context = self.context_builder.build(
            experiments_count=experiments_count,
            runs_count=runs_count,
            episodes_count=episodes_count,
            experiment_summaries=experiment_summaries,
            run_summaries=run_summaries,
            failure_patterns=patterns,
            episode_timelines=episode_timelines,
            representative_failed_episodes=representative_episodes,
            rag_context=rag_context,
        )
        refs = {(episode.experiment_id, episode.run_id, episode.episode_id) for episode in episodes}
        recommendations, analysis_mode, llm_warnings = self._recommendations(
            context=context,
            patterns=patterns,
            refs=refs,
        )
        warnings.extend(llm_warnings)

        return self.response_builder.build(
            experiments_count=experiments_count,
            runs_count=runs_count,
            episodes_count=episodes_count,
            metrics=self._response_metrics(episodes, parsed),
            patterns=patterns,
            recommendations=recommendations,
            warnings=warnings,
            analysis_mode=analysis_mode,
        )

    def _default_root(self) -> Path:
        configured_root = self.settings.experiments_dir
        if configured_root:
            return Path(configured_root)
        return Path("data") / "experiments"

    def scan(self, request: AnalysisRunV2Request | None = None) -> WorkspaceScan:
        if request is None:
            root = self.experiments_root or self._default_root()
            return self.scanner.scan(root)

        project_root = Path(request.project_path)
        scan = self.scanner.scan(project_root)
        run_path = project_root / "runs" / request.run_id
        warnings = [*scan.warnings]
        if project_root.exists() and not run_path.is_dir():
            warnings.append(f"run directory does not exist: {run_path}")
        if run_path.is_dir():
            warnings.extend(self._missing_episode_artifact_warnings(run_path))
        files = [
            path
            for path in scan.files
            if self._is_requested_run_file(project_root=project_root, path=path, run_id=request.run_id)
        ]
        return WorkspaceScan(root=project_root, files=files, warnings=warnings)

    def _is_requested_run_file(self, *, project_root: Path, path: Path, run_id: str) -> bool:
        try:
            parts = path.relative_to(project_root).parts
        except ValueError:
            return False
        return len(parts) >= 2 and parts[0] == "runs" and parts[1] == run_id

    def _missing_episode_artifact_warnings(self, run_path: Path) -> list[str]:
        """Report absent optional episode artifacts without failing alpha analysis."""
        episodes_path = run_path / "episodes"
        if not episodes_path.is_dir():
            return []

        expected_files = ("result.json", "events.jsonl", "actions.jsonl", "trace.jsonl")
        warnings: list[str] = []
        for episode_path in sorted(path for path in episodes_path.iterdir() if path.is_dir()):
            relative_episode = episode_path.relative_to(run_path.parent.parent).as_posix()
            for filename in expected_files:
                if not (episode_path / filename).is_file():
                    warnings.append(f"{relative_episode}/{filename} is missing.")
        return warnings

    def _extract_episodes(self, artifacts: list[ParsedArtifact]) -> list[EpisodeMetrics]:
        results: dict[tuple[str, str, str], dict[str, Any]] = {}
        events: dict[tuple[str, str, str], list[Any]] = defaultdict(list)
        for artifact in artifacts:
            info = artifact.info
            if not (info.experiment_id and info.run_id and info.episode_id):
                continue
            key = (info.experiment_id, info.run_id, info.episode_id)
            if info.artifact_type == "episode_result" and isinstance(artifact.data, dict):
                results[key] = artifact.data
            elif info.artifact_type in {"episode_events", "episode_actions", "episode_trace"} and isinstance(artifact.data, list):
                events[key].extend(artifact.data)

        episode_keys = sorted(set(results) | set(events))
        return [
            self.metric_extractor.extract(
                experiment_id=experiment_id,
                run_id=run_id,
                episode_id=episode_id,
                result=results.get((experiment_id, run_id, episode_id)),
                events=events.get((experiment_id, run_id, episode_id), []),
            )
            for experiment_id, run_id, episode_id in episode_keys
        ]

    def _timeline_inputs(self, artifacts: list[ParsedArtifact]) -> dict[str, list[dict[str, Any]]]:
        episodes: dict[tuple[str, str, str], dict[str, Any]] = {}
        for artifact in artifacts:
            info = artifact.info
            if not (info.experiment_id and info.run_id and info.episode_id):
                continue
            key = (info.experiment_id, info.run_id, info.episode_id)
            item = episodes.setdefault(
                key,
                {
                    "experiment_id": info.experiment_id,
                    "run_id": info.run_id,
                    "episode_id": info.episode_id,
                    "events": [],
                    "actions": [],
                    "source_path": info.relative_path,
                },
            )
            if info.artifact_type in {"episode_events", "episode_trace"} and isinstance(artifact.data, list):
                item["events"].extend(
                    {**event, "_source_path": info.relative_path} for event in artifact.data if isinstance(event, dict)
                )
            elif info.artifact_type == "episode_actions" and isinstance(artifact.data, list):
                item["actions"].extend(action for action in artifact.data if isinstance(action, dict))
        return {"episodes": list(episodes.values())}

    def _episode_as_dict(self, episode: EpisodeMetrics) -> dict[str, Any]:
        return asdict(episode)

    def _scope_counts(self, artifacts: list[ParsedArtifact], episodes: list[EpisodeMetrics]) -> tuple[int, int, int]:
        """Prefer episode-derived scope, falling back to tolerant run summary counts."""
        experiment_ids = {artifact.info.experiment_id for artifact in artifacts if artifact.info.experiment_id}
        run_ids = {(episode.experiment_id, episode.run_id) for episode in episodes}
        summary_run_ids = {
            (artifact.info.experiment_id, artifact.info.run_id)
            for artifact in artifacts
            if artifact.info.artifact_type == "run_summary" and artifact.info.experiment_id and artifact.info.run_id
        }
        episode_count = len(episodes) if episodes else self._summary_episode_count(artifacts)
        return len(experiment_ids), len(run_ids | summary_run_ids), episode_count

    def _response_metrics(self, episodes: list[EpisodeMetrics], artifacts: list[ParsedArtifact]) -> AnalysisMetricsV2:
        """Use summary metrics only when episode-level metrics are unavailable."""
        if episodes:
            return self._totals(episodes)
        return self._summary_metrics(artifacts) or self._totals(episodes)

    def _totals(self, episodes: list[EpisodeMetrics]) -> AnalysisMetricsV2:
        return AnalysisMetricsV2(
            success_count=sum(1 for episode in episodes if episode.success is True),
            failure_count=sum(1 for episode in episodes if episode.success is False),
            collision_count=sum(episode.collision_count for episode in episodes),
            near_miss_count=sum(episode.near_miss_count for episode in episodes),
            blocked_region_violation_count=sum(episode.blocked_region_violation_count for episode in episodes),
            penalty_region_violation_count=sum(episode.penalty_region_violation_count for episode in episodes),
        )

    def _summary_metrics(self, artifacts: list[ParsedArtifact]) -> AnalysisMetricsV2 | None:
        """Extract conservative run-level metrics from summary.json when no episodes parse."""
        summary = self._first_run_summary(artifacts)
        if summary is None:
            return None
        metrics = summary.get("metrics") if isinstance(summary.get("metrics"), dict) else {}
        return AnalysisMetricsV2(
            success_count=self._summary_int(summary, metrics, "success_count"),
            failure_count=self._summary_int(summary, metrics, "failure_count"),
            collision_count=self._summary_int(summary, metrics, "collision_count"),
            near_miss_count=self._summary_int(summary, metrics, "near_miss_count"),
            blocked_region_violation_count=self._summary_int(summary, metrics, "blocked_region_violation_count"),
            penalty_region_violation_count=self._summary_int(summary, metrics, "penalty_region_violation_count"),
        )

    def _summary_episode_count(self, artifacts: list[ParsedArtifact]) -> int:
        """Read common episode count field names from summary.json without assuming one schema."""
        summary = self._first_run_summary(artifacts)
        if summary is None:
            return 0
        for key in ("episode_count", "episodes_count", "total_episodes"):
            value = summary.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        episodes = summary.get("episodes")
        return len(episodes) if isinstance(episodes, list) else 0

    def _first_run_summary(self, artifacts: list[ParsedArtifact]) -> dict[str, Any] | None:
        """Return the first parsed run summary object, ignoring malformed summaries."""
        for artifact in artifacts:
            if artifact.info.artifact_type == "run_summary" and isinstance(artifact.data, dict):
                return artifact.data
        return None

    def _summary_int(self, summary: dict[str, Any], metrics: dict[str, Any], key: str) -> int:
        """Read a non-negative integer metric from summary root or nested metrics."""
        for source in (summary, metrics):
            value = source.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        return 0

    def _recommendations(
        self,
        *,
        context: dict[str, Any],
        patterns: list[dict[str, Any]],
        refs: set[tuple[str, str, str]],
    ) -> tuple[list[dict[str, Any]], str, list[str]]:
        if not self.settings.v2AgentLlmEnabled:
            return self._rule_based_recommendations(patterns, refs), "rule_based", []

        fallback_warning = "LLM recommendation failed; rule-based recommendation fallback was used."
        try:
            payload = (self.llm_client or AgentLlmJsonClient(settings=self.settings)).generate_json(
                system_prompt=self._read_prompt("system_prompt.md"),
                user_prompt=self._analysis_user_prompt(context),
                response_name="analysis_recommendations_v2",
            )
            llm_recommendations = payload.get("recommendations", [])
            if not isinstance(llm_recommendations, list):
                raise ValueError("LLM recommendations must be a list.")
            validated = self.recommendation_validator.validate(llm_recommendations, refs)
            if len(validated) != len(llm_recommendations):
                raise ValueError("LLM recommendations failed evidence validation.")
            return validated, "llm", []
        except Exception:
            return self._rule_based_recommendations(patterns, refs), "fallback", [fallback_warning]

    def _rule_based_recommendations(
        self,
        patterns: list[dict[str, Any]],
        refs: set[tuple[str, str, str]],
    ) -> list[dict[str, Any]]:
        llm_analysis = self.llm_analyzer.analyze({"failure_patterns": patterns})
        recommendations = self.recommendation_generator.generate(llm_analysis, patterns)
        return self.recommendation_validator.validate(recommendations, refs)

    def _analysis_user_prompt(self, context: dict[str, Any]) -> str:
        import json

        return "\n\n".join(
            [
                self._read_prompt("recommendation_prompt.md"),
                "전체 raw log를 사용하지 말고 제공된 요약 metrics, aggregates, patterns에 근거해서만 추천한다.",
                "target은 policy 또는 environment만 허용한다. evidence는 실제 제공된 episode만 참조한다.",
                json.dumps(context, ensure_ascii=False, indent=2),
            ]
        )

    def _read_prompt(self, filename: str) -> str:
        return (Path(__file__).parent / "prompts" / filename).read_text(encoding="utf-8")

    def _llm_client(self) -> AgentLlmClient:
        return AgentLlmJsonClient(settings=self.settings)
