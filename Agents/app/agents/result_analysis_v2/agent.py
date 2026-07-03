from __future__ import annotations

from collections import defaultdict
from dataclasses import asdict
from pathlib import Path
from typing import Any

from app.agents.common.llm_json_client import AgentLlmClient, AgentLlmJsonClient
from app.agents.common.spec_context_loader import SpecContextLoader
from app.agents.result_analysis_v2.analysis_context_builder import AnalysisContextBuilder
from app.agents.result_analysis_v2.artifact_classifier import ArtifactClassifier
from app.agents.result_analysis_v2.artifact_parser import ArtifactParser, ParsedArtifact
from app.agents.result_analysis_v2.data_coverage import DataCoverageBuilder
from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetricExtractor, EpisodeMetrics
from app.agents.result_analysis_v2.experiment_aggregator import ExperimentAggregator
from app.agents.result_analysis_v2.failure_pattern_detector import FailurePatternDetector
from app.agents.result_analysis_v2.finding_builder import FindingBuilder
from app.agents.result_analysis_v2.llm_failure_analyzer import LlmFailureAnalyzer
from app.agents.result_analysis_v2.rag_context_builder import FileBasedRagRetrieverAdapterV2, RagContextBuilderV2
from app.agents.result_analysis_v2.rag_query_builder import RagQueryBuilderV2
from app.agents.result_analysis_v2.recommendation_artifact_writer import RecommendationArtifactWriter
from app.agents.result_analysis_v2.recommendation_schema import analysis_recommendations_v2_response_schema
from app.agents.result_analysis_v2.recommendation_generator import RecommendationGenerator
from app.agents.result_analysis_v2.recommendation_type_decider import RecommendationTypeDecider
from app.agents.result_analysis_v2.recommendation_validator import RecommendationValidator
from app.agents.result_analysis_v2.representative_selector import RepresentativeFailedEpisodeSelectorV2
from app.agents.result_analysis_v2.response_builder import ResponseBuilder
from app.agents.result_analysis_v2.review_lifecycle import ReviewLifecycleManager, ReviewSession
from app.agents.result_analysis_v2.review_text import INSUFFICIENT_DATA_SUMMARY_MESSAGE
from app.agents.result_analysis_v2.run_aggregator import RunAggregator
from app.agents.result_analysis_v2.run_comparison import PreviousRunComparator
from app.agents.result_analysis_v2.snapshot_hash import SnapshotHashBuilder
from app.agents.result_analysis_v2.summary_row_public_builder import SummaryRowPublicBuilder
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
        spec_context_loader: SpecContextLoader | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.llm_client = llm_client
        self.spec_context_loader = spec_context_loader
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
        self.data_coverage_builder = DataCoverageBuilder()
        self.finding_builder = FindingBuilder()
        self.recommendation_type_decider = RecommendationTypeDecider()
        self.snapshot_hash_builder = SnapshotHashBuilder()
        self.previous_run_comparator = PreviousRunComparator()
        self.review_lifecycle = ReviewLifecycleManager()
        self.recommendation_artifact_writer = RecommendationArtifactWriter()
        self.summary_row_public_builder = SummaryRowPublicBuilder()

    def run(self, request: AnalysisRunV2Request | None = None) -> AnalysisRunV2Response:
        review_session = self.review_lifecycle.start(request)
        try:
            scan = self.scan(request)
            root = scan.root
            parsed = [self.parser.parse(self.classifier.classify(root, path)) for path in scan.files]
            warnings = [*scan.warnings]
            for artifact in parsed:
                warnings.extend(artifact.warnings)

            episodes = self._extract_episodes(parsed)
            public_data = self.summary_row_public_builder.build(self._summary_rows(parsed))
            public_metrics = (
                self._merge_episode_detail_counts(public_data.metrics, episodes)
                if public_data.has_rows
                else public_data.metrics
            )
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
            public_patterns = patterns if public_data.has_rows else []
            public_recommendations = recommendations if public_data.has_rows else []
            public_episodes_count = len(public_data.episodes) if public_data.has_rows else 0

            response = self.response_builder.build(
                experiments_count=experiments_count,
                runs_count=runs_count,
                episodes_count=public_episodes_count,
                metrics=public_metrics,
                run_overview=public_data.run_overview,
                episodes=public_data.episodes if public_data.has_rows else None,
                patterns=public_patterns,
                recommendations=public_recommendations,
                warnings=warnings,
                analysis_mode=analysis_mode,
                run_id=request.run_id if request is not None else None,
                review_id=review_session.review_id if review_session is not None else None,
            )
            response = self._with_prompt_focus(response, request)
            self._complete_review_if_started(
                session=review_session,
                request=request,
                response=response,
                parsed=parsed,
                episodes=episodes,
                warnings=warnings,
                detailed_recommendations=public_recommendations,
            )
            return response
        except Exception as exc:
            if review_session is not None:
                self.review_lifecycle.fail(session=review_session, code=exc.__class__.__name__, message=str(exc))
            raise

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
        warnings = self._requested_run_warnings(project_root=project_root, warnings=scan.warnings, run_id=request.run_id)
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

    def _requested_run_warnings(self, *, project_root: Path, warnings: list[str], run_id: str) -> list[str]:
        """Keep scan warnings scoped to the requested run while preserving root-level failures."""
        run_prefix = f"runs/{run_id}/"
        scoped: list[str] = []
        for warning in warnings:
            normalized = warning.replace("\\", "/")
            if "runs/" not in normalized or run_prefix in normalized:
                scoped.append(warning)
        return scoped

    def _is_requested_run_file(self, *, project_root: Path, path: Path, run_id: str) -> bool:
        try:
            parts = path.relative_to(project_root).parts
        except ValueError:
            return False
        return len(parts) >= 2 and parts[0] == "runs" and parts[1] == run_id and (len(parts) < 3 or parts[2] != "review")

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
            static_obstacle_collision_count=sum(episode.static_obstacle_collision_count for episode in episodes),
            pedestrian_collision_count=sum(episode.pedestrian_collision_count for episode in episodes),
            near_miss_count=sum(episode.near_miss_count for episode in episodes),
            repath_count=sum(episode.repath_count for episode in episodes),
            robot_tip_over_count=sum(episode.robot_tip_over_count for episode in episodes),
            blocked_region_violation_count=sum(episode.blocked_region_violation_count for episode in episodes),
            penalty_region_violation_count=sum(episode.penalty_region_violation_count for episode in episodes),
        )

    def _merge_episode_detail_counts(
        self,
        public_metrics: AnalysisMetricsV2,
        episodes: list[EpisodeMetrics],
    ) -> AnalysisMetricsV2:
        """Fill public detail counts from episode results while preserving dashboard summary counts."""
        if not episodes:
            return public_metrics
        episode_totals = self._totals(episodes)
        values = public_metrics.model_dump()
        for key in (
            "static_obstacle_collision_count",
            "pedestrian_collision_count",
            "near_miss_count",
            "repath_count",
            "robot_tip_over_count",
            "blocked_region_violation_count",
            "penalty_region_violation_count",
        ):
            values[key] = max(0, int(values.get(key) or 0), int(getattr(episode_totals, key, 0) or 0))
        return AnalysisMetricsV2(**values)

    def _summary_metrics(self, artifacts: list[ParsedArtifact]) -> AnalysisMetricsV2 | None:
        """Extract conservative run-level metrics from summary.json when no episodes parse."""
        summary = self._first_run_summary(artifacts)
        if summary is None:
            return None
        metrics = summary.get("metrics") if isinstance(summary.get("metrics"), dict) else {}
        event_summary = summary.get("event_summary") if isinstance(summary.get("event_summary"), dict) else {}
        return AnalysisMetricsV2(
            success_count=self._summary_int(summary, metrics, event_summary, "success_count", set()),
            failure_count=self._summary_int(summary, metrics, event_summary, "failure_count", set()),
            collision_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "collision_count",
                {"collision", "static_obstacle_collision", "pedestrian_collision"},
            ),
            static_obstacle_collision_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "static_obstacle_collision_count",
                {"static_obstacle_collision"},
            ),
            pedestrian_collision_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "pedestrian_collision_count",
                {"pedestrian_collision"},
            ),
            near_miss_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "near_miss_count",
                {"near_miss", "pedestrian_near_miss"},
            ),
            repath_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "repath_count",
                {"repath"},
            ),
            robot_tip_over_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "robot_tip_over_count",
                {"robot_tip_over"},
            ),
            blocked_region_violation_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "blocked_region_violation_count",
                {"blocked_region_violation", "blocked_region_collision"},
            ),
            penalty_region_violation_count=self._summary_int(
                summary,
                metrics,
                event_summary,
                "penalty_region_violation_count",
                {"penalty_region_violation"},
            ),
        )

    def _summary_episode_count(self, artifacts: list[ParsedArtifact]) -> int:
        """Read common episode count field names from summary.json without assuming one schema."""
        summary = self._first_run_summary(artifacts)
        if summary is None:
            return 0
        rows = summary.get("rows")
        if isinstance(rows, list):
            return sum(1 for row in rows if isinstance(row, dict))
        for key in ("episode_count", "episodes_count", "total_episodes"):
            value = summary.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        episodes = summary.get("episodes")
        return len(episodes) if isinstance(episodes, list) else 0

    def _summary_rows(self, artifacts: list[ParsedArtifact]) -> list[dict[str, Any]]:
        """Return valid summary.json rows used by the public dashboard response."""
        summary = self._first_run_summary(artifacts)
        if summary is None:
            return []
        rows = summary.get("rows")
        if not isinstance(rows, list):
            return []
        return [row for row in rows if isinstance(row, dict)]

    def _first_run_summary(self, artifacts: list[ParsedArtifact]) -> dict[str, Any] | None:
        """Return the first parsed run summary object, ignoring malformed summaries."""
        for artifact in artifacts:
            if artifact.info.artifact_type == "run_summary" and isinstance(artifact.data, dict):
                return artifact.data
        return None

    def _summary_int(
        self,
        summary: dict[str, Any],
        metrics: dict[str, Any],
        event_summary: dict[str, Any],
        key: str,
        event_names: set[str],
    ) -> int:
        """Read a non-negative integer metric from summary root, metrics, or events."""
        for source in (summary, metrics):
            value = source.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        if event_names:
            return self.metric_extractor._event_summary_count(event_summary, event_names)
        return 0

    def _prompt_focus(self, request: AnalysisRunV2Request | None) -> list[str]:
        """Extract lightweight analysis focus labels from the optional user prompt."""
        prompt = request.prompt if request else None
        if not prompt:
            return []

        focus_terms = (
            ("obstacle", ("obstacle", "장애물", "정적 장애물", "static obstacle")),
            ("collision", ("collision", "collide", "crash", "충돌", "부딪")),
            ("stop", ("stop", "stopped", "정지", "멈춤", "멈추", "감속", "속도")),
            ("timeout", ("timeout", "time out", "시간초과", "시간 초과", "지연")),
            ("blocked", ("blocked", "block", "막힘", "통로", "차단")),
            ("penalty", ("penalty", "페널티", "벌점")),
            ("near_miss", ("near miss", "near_miss", "아슬", "근접", "위험")),
            ("route_deviation", ("route deviation", "deviation", "보도이탈", "보도 이탈", "경로 이탈", "이탈")),
        )
        lowered = prompt.casefold()
        return [label for label, terms in focus_terms if any(term.casefold() in lowered for term in terms)]

    def _with_prompt_focus(
        self,
        response: AnalysisRunV2Response,
        request: AnalysisRunV2Request | None,
    ) -> AnalysisRunV2Response:
        """Reflect prompt focus in the existing summary message without changing metrics."""
        focus = self._prompt_focus(request)
        if not focus:
            return response
        suffix = f" 사용자 요청 관점: {', '.join(focus)} 중심으로 확인했습니다."
        response.summary.message = f"{response.summary.message}{suffix}"
        return response

    def _complete_review_if_started(
        self,
        *,
        session: ReviewSession | None,
        request: AnalysisRunV2Request | None,
        response: AnalysisRunV2Response,
        parsed: list[ParsedArtifact],
        episodes: list[EpisodeMetrics],
        warnings: list[str],
        detailed_recommendations: list[dict[str, Any]],
    ) -> None:
        """Finalize review artifacts when this request has a valid run directory."""
        if session is None or request is None:
            return
        try:
            run_path = Path(request.project_path) / "runs" / request.run_id
            data_coverage = self.data_coverage_builder.build(
                run_path=run_path,
                parsed_artifacts=parsed,
                warnings=warnings,
            )
            findings, evidence = self.finding_builder.build(
                episodes=episodes,
                parsed_artifacts=parsed,
                prompt_focus=self._prompt_focus(request),
            )
            setup_failure_details = self._setup_failure_details(episodes)
            decision = self.recommendation_type_decider.decide(
                summary_judgement=response.summary.overall_judgement if response.summary is not None else "insufficient_data",
                findings=findings,
                data_coverage=data_coverage,
                metrics=response.metrics,
            )
            response.recommendation_type = decision.recommendation_type
            response.run_id = request.run_id
            response.review_id = session.review_id
            self._align_response_summary_with_review_decision(
                response=response,
                recommendation_type=decision.recommendation_type,
                findings=findings,
                prompt_focus=self._prompt_focus(request),
                setup_failure_details=setup_failure_details,
            )
            review_recommendations = self.recommendation_generator.ensure_for_review(
                recommendations=detailed_recommendations,
                recommendation_type=decision.recommendation_type,
                findings=findings,
                metrics=response.metrics,
            )
            modified_policy_json = self.response_builder.modified_candidate_payloads(
                recommendations=review_recommendations,
                target="policy",
            )
            modified_environment_json = self.response_builder.modified_candidate_payloads(
                recommendations=review_recommendations,
                target="environment",
            )
            artifact_write = self.recommendation_artifact_writer.write(
                session=session,
                recommendation_type=decision.recommendation_type,
            )
            if artifact_write.warnings:
                warnings.extend(artifact_write.warnings)
                response.warnings.extend(
                    warning for warning in artifact_write.warnings if warning not in response.warnings
                )
            detailed_insights = self._build_insights(
                findings=findings,
                evidence=evidence,
                metrics=response.metrics,
                recommendation_type=decision.recommendation_type,
            )
            response.insights = self.response_builder.public_insights(detailed_insights)
            response.recommendations = self.response_builder.public_recommendations(review_recommendations)
            snapshot_hashes = self.snapshot_hash_builder.build(project_path=Path(request.project_path), run_id=request.run_id)
            comparison = self.previous_run_comparator.compare(
                project_path=Path(request.project_path),
                run_id=request.run_id,
                current_snapshot_hashes=snapshot_hashes,
            )
            report = {
                "summary": response.summary.model_dump() if response.summary is not None else None,
                "metrics": response.metrics.model_dump() if response.metrics is not None else None,
                "data_coverage": data_coverage,
                "insights": detailed_insights,
                "findings": findings,
                "evidence": evidence,
                "patterns": response.patterns or [],
            }
            recommendation_artifact = {
                "recommendation_type": decision.recommendation_type,
                "recommendations": review_recommendations,
                "reason": decision.reason,
                "evidence_ids": decision.evidence_ids,
                "modified_policy_json": modified_policy_json,
                "modified_environment_json": modified_environment_json,
                "artifacts": artifact_write.artifacts,
                "artifact_warnings": artifact_write.warnings,
            }
            manifest = {
                "snapshot_hashes": snapshot_hashes,
                "comparison": comparison,
                "artifacts": artifact_write.artifacts,
            }
            self.review_lifecycle.complete(
                session=session,
                response=response,
                report=report,
                recommendations=recommendation_artifact,
                manifest=manifest,
                source_run_files=self._source_run_files(parsed),
            )
        except Exception as exc:
            self.review_lifecycle.fail(session=session, code=exc.__class__.__name__, message=str(exc))
            response.warnings.append(f"review artifact write failed: {exc}")
            response.recommendation_type = (
                "insufficient_data"
                if response.summary is not None and response.summary.overall_judgement == "insufficient_data"
                else "none"
            )

    def _source_run_files(self, parsed: list[ParsedArtifact]) -> list[str]:
        """Return source run files used by analysis, excluding generated review artifacts."""
        return sorted(
            artifact.info.relative_path
            for artifact in parsed
            if self._is_source_run_file(artifact.info.relative_path)
        )

    def _is_source_run_file(self, relative_path: str) -> bool:
        """Exclude review outputs and runtime/cache files from manifest source inputs."""
        parts = relative_path.split("/")
        if "__pycache__" in parts or relative_path.endswith(".pyc") or parts[-1] == ".DS_Store":
            return False
        return not relative_path.startswith("runs/") or "/review/" not in relative_path

    def _setup_failure_details(self, episodes: list[EpisodeMetrics]) -> list[Any]:
        """Return setup failure details recorded on setup-stage episodes."""
        return [episode.setup_failure_details for episode in episodes if episode.setup_failure_details is not None]

    def _build_insights(
        self,
        *,
        findings: list[dict[str, Any]],
        evidence: list[dict[str, Any]],
        metrics: AnalysisMetricsV2 | None,
        recommendation_type: str,
    ) -> list[dict[str, Any]]:
        """Build detailed report insights from findings and public metrics."""
        if recommendation_type == "insufficient_data":
            return []
        finding_types = {str(finding.get("type")) for finding in findings}
        evidence_by_id = {
            str(item.get("evidence_id")): item for item in evidence if item.get("evidence_id")
        }
        insights: list[dict[str, Any]] = []
        if (
            metrics is not None
            and metrics.collision_count == 0
            and {"timeout", "goal_not_reached", "stuck"} & finding_types
        ):
            evidence_ids = self._finding_evidence_ids(findings, {"timeout", "goal_not_reached", "stuck"})
            title = "충돌 없이 제한 시간 초과"
            detail = "충돌은 발생하지 않았지만 목표 도달 실패, 제한 시간 초과, 또는 정체 신호가 확인되었습니다."
            if {"timeout", "stuck"} <= finding_types:
                title = "정체 후 제한 시간 초과"
                detail = "충돌은 발생하지 않았지만 정체 신호가 확인되었고, 제한 시간 내 목표에 도달하지 못했습니다."
            insights.append(
                self._insight(
                    index=len(insights) + 1,
                    insight_type="timeout_without_collision",
                    severity="high",
                    title=title,
                    detail=detail,
                    evidence_ids=evidence_ids,
                    evidence_by_id=evidence_by_id,
                )
            )
        if {"static_obstacle_collision", "blocked_region_collision", "pedestrian_collision"} & finding_types:
            evidence_ids = self._finding_evidence_ids(
                findings,
                {"static_obstacle_collision", "blocked_region_collision", "pedestrian_collision"},
            )
            insights.append(
                self._insight(
                    index=len(insights) + 1,
                    insight_type="collision_observed",
                    severity="high",
                    title="충돌 관련 이벤트 확인",
                    detail="정적 장애물, 보행자, 또는 차단 구역 충돌 이벤트가 확인되었습니다.",
                    evidence_ids=evidence_ids,
                    evidence_by_id=evidence_by_id,
                )
            )
        if recommendation_type == "policy_review":
            evidence_ids = self._finding_evidence_ids(
                findings,
                {
                    "penalty_region_violation",
                    "timeout",
                    "goal_not_reached",
                    "near_miss",
                    "repath",
                    "policy_decision_error",
                    "stuck",
                    "robot_tip_over",
                },
            )
            insights.append(
                self._insight(
                    index=len(insights) + 1,
                    insight_type="policy_review_priority",
                    severity="medium",
                    title="정책 검토 우선",
                    detail="경로 추종, 감속/정지, 재경로 탐색 조건 확인이 우선입니다.",
                    evidence_ids=evidence_ids,
                    evidence_by_id=evidence_by_id,
                )
            )
        if "setup_failed" in finding_types:
            evidence_ids = self._finding_evidence_ids(findings, {"setup_failed"})
            insights.append(
                self._insight(
                    index=len(insights) + 1,
                    insight_type="setup_failed",
                    severity="high",
                    title="세팅 단계 중단",
                    detail="주행 시작 전 scenario, prop, catalog, asset, map/segment 참조 확인이 필요한 중단이 기록되었습니다.",
                    evidence_ids=evidence_ids,
                    evidence_by_id=evidence_by_id,
                )
            )
        return insights[:3]

    def _insight(
        self,
        *,
        index: int,
        insight_type: str,
        severity: str,
        title: str,
        detail: str,
        evidence_ids: list[str],
        evidence_by_id: dict[str, dict[str, Any]],
    ) -> dict[str, Any]:
        """Create one detailed report insight record."""
        related_episode_ids = sorted(
            {
                str(evidence_by_id[evidence_id].get("episode_id"))
                for evidence_id in evidence_ids
                if evidence_id in evidence_by_id and evidence_by_id[evidence_id].get("episode_id")
            }
        )
        return {
            "id": f"INS-{index:03d}",
            "type": insight_type,
            "severity": severity,
            "title": title,
            "detail": detail,
            "description": detail,
            "related_episode_ids": related_episode_ids,
            "evidence_ids": evidence_ids,
        }

    def _finding_evidence_ids(self, findings: list[dict[str, Any]], finding_types: set[str]) -> list[str]:
        """Collect evidence ids from matching findings."""
        evidence_ids: list[str] = []
        for finding in findings:
            if finding.get("type") not in finding_types:
                continue
            evidence_ids.extend(str(evidence_id) for evidence_id in finding.get("evidence_ids", []))
        return evidence_ids

    def _sync_modified_candidate_payloads(self, response: AnalysisRunV2Response) -> None:
        """Legacy hook kept so older tests can call it without restoring public candidate fields."""
        _ = response

    def _align_response_summary_with_review_decision(
        self,
        *,
        response: AnalysisRunV2Response,
        recommendation_type: str,
        findings: list[dict[str, Any]],
        prompt_focus: list[str],
        setup_failure_details: list[Any],
    ) -> None:
        """Keep user-facing summary consistent with review artifact recommendation type."""
        if response.summary is None:
            return
        if recommendation_type == "none":
            if self._has_setup_failure(findings=findings, setup_failure_details=setup_failure_details):
                response.summary.overall_judgement = "change_recommended"
                response.summary.message = self._setup_summary_message(setup_failure_details)
                self._append_prompt_focus_message(response=response, prompt_focus=prompt_focus)
                return
            response.summary.overall_judgement = "no_change_needed"
            return
        if recommendation_type == "insufficient_data":
            response.summary.overall_judgement = "insufficient_data"
            response.summary.message = INSUFFICIENT_DATA_SUMMARY_MESSAGE
            return

        response.summary.overall_judgement = "change_recommended"
        finding_types = {str(finding.get("type")) for finding in findings}
        has_setup_failure = self._has_setup_failure(findings=findings, setup_failure_details=setup_failure_details)
        if recommendation_type == "environment_review":
            response.summary.message = "환경 또는 장애물 관련 충돌이 발생해 환경 검토가 필요합니다."
            if has_setup_failure:
                response.summary.message = f"{response.summary.message} 일부 episode는 세팅 단계에서 중단되어 환경 판단에서 제외했습니다."
            self._append_prompt_focus_message(response=response, prompt_focus=prompt_focus)
            return
        if response.metrics is not None and response.metrics.success_count > 0 and response.metrics.failure_count == 0:
            response.summary.message = (
                "주행은 성공했지만, 패널티 구역 침범 등 안전/정책 검토가 필요한 신호가 나타났습니다."
            )
            self._append_prompt_focus_message(response=response, prompt_focus=prompt_focus)
            return
        if (
            "penalty_region_violation" in finding_types
            and ("obstacle" in prompt_focus or "collision" in prompt_focus)
            and not {"static_obstacle_collision", "blocked_region_collision"} & finding_types
        ):
            response.summary.message = (
                "사용자 요청 관점에서 요청한 장애물/충돌 문제는 확인되지 않았고, "
                "패널티 구역 침범이 발생해 주행 정책 검토가 필요합니다."
            )
        elif {"timeout", "stuck"} <= finding_types:
            response.summary.message = "정체 이후 제한 시간 초과로 종료되어 주행 정책 검토가 필요합니다."
        else:
            response.summary.message = "주행 정책 검토가 필요한 실패가 발생했습니다."
        if has_setup_failure:
            response.summary.message = f"{response.summary.message} 일부 episode는 세팅 단계에서 중단되어 정책 판단에서 제외했습니다."
        self._append_prompt_focus_message(response=response, prompt_focus=prompt_focus)

    def _has_setup_failure(self, *, findings: list[dict[str, Any]], setup_failure_details: list[Any]) -> bool:
        """Return whether this run contains setup-stage failures."""
        return bool(setup_failure_details) or any(finding.get("type") == "setup_failed" for finding in findings)

    def _setup_summary_message(self, setup_failure_details: list[Any]) -> str:
        """Build setup-failure summary text without exposing diagnostic source paths."""
        if not setup_failure_details:
            return (
                "주행 시작 전 세팅 단계에서 실행이 중단되었습니다. "
                "scenario, prop, catalog, asset, 환경 설정 확인이 필요합니다."
            )
        detail = setup_failure_details[0]
        resource_type = str(getattr(detail, "resource_type", "") or "").strip()
        resource_id = str(getattr(detail, "resource_id", "") or "").strip()
        message = str(getattr(detail, "message", "") or "").strip()
        if resource_type == "prop" and resource_id:
            return f"{resource_id}가 환경 카탈로그에 등록되지 않아 주행 시작 전 세팅 단계에서 실행이 중단되었습니다."
        if resource_type == "asset" and resource_id:
            return f"{resource_id} asset을 불러오지 못해 주행 시작 전 세팅 단계에서 실행이 중단되었습니다."
        if resource_type in {"map", "segment"} and resource_id:
            return f"{resource_type} 참조 ID {resource_id}를 확인하지 못해 주행 시작 전 세팅 단계에서 실행이 중단되었습니다."
        if message:
            return f"주행 시작 전 세팅 단계에서 실행이 중단되었습니다. 기록된 setup 오류: {message}"
        return (
            "주행 시작 전 세팅 단계에서 실행이 중단되었습니다. "
            "scenario, prop, catalog, asset, 환경 설정 확인이 필요합니다."
        )

    def _append_prompt_focus_message(self, *, response: AnalysisRunV2Response, prompt_focus: list[str]) -> None:
        """Append prompt focus text when summary alignment replaced the original message."""
        if response.summary is not None and prompt_focus and "사용자 요청 관점" not in response.summary.message:
            response.summary.message = (
                f"{response.summary.message} 사용자 요청 관점: {', '.join(prompt_focus)} 중심으로 확인했습니다."
            )

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
                response_schema=analysis_recommendations_v2_response_schema(),
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
                self._spec_context_prompt_block(),
                "전체 raw log를 사용하지 말고 제공된 요약 metrics, aggregates, patterns에 근거해서만 추천한다.",
                "target은 policy 또는 environment만 허용한다. evidence는 실제 제공된 episode만 참조한다.",
                json.dumps(context, ensure_ascii=False, indent=2),
            ]
        )

    def _spec_context_prompt_block(self) -> str:
        """Load v2 Agent spec context lazily for LLM-only recommendation prompts."""
        if self.spec_context_loader is None:
            self.spec_context_loader = SpecContextLoader()
        return self.spec_context_loader.build_prompt_block()

    def _read_prompt(self, filename: str) -> str:
        return (Path(__file__).parent / "prompts" / filename).read_text(encoding="utf-8")

    def _llm_client(self) -> AgentLlmClient:
        return AgentLlmJsonClient(settings=self.settings)
