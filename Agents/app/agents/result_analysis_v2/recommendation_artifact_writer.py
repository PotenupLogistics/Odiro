"""Coordinates optional recommendation artifact creation for review sessions."""

from __future__ import annotations

from dataclasses import dataclass

from app.agents.result_analysis_v2.environment_candidate_writer import EnvironmentCandidateWriter
from app.agents.result_analysis_v2.policy_candidate_writer import CandidateWriteResult, PolicyCandidateWriter
from app.agents.result_analysis_v2.review_lifecycle import ReviewSession
from app.agents.result_analysis_v2.review_text import default_artifacts


@dataclass(frozen=True)
class RecommendationArtifactWrite:
    """Result of optional policy/environment artifact generation."""

    # Artifact status object persisted into recommendations.json and manifest.json.
    artifacts: dict
    # Non-fatal warnings produced while copying or modifying candidates.
    warnings: list[str]


class RecommendationArtifactWriter:
    """Writes review-only policy or environment candidates based on recommendation type."""

    def __init__(
        self,
        *,
        policy_writer: PolicyCandidateWriter | None = None,
        environment_writer: EnvironmentCandidateWriter | None = None,
    ) -> None:
        """Allow tests to inject candidate writers while production uses defaults."""
        self.policy_writer = policy_writer or PolicyCandidateWriter()
        self.environment_writer = environment_writer or EnvironmentCandidateWriter()

    def write(self, *, session: ReviewSession, recommendation_type: str) -> RecommendationArtifactWrite:
        """Create candidate artifacts for policy_review or environment_review only."""
        artifacts = default_artifacts()
        warnings: list[str] = []
        if recommendation_type == "policy_review":
            result = self.policy_writer.write(project_path=session.project_path, review_dir=session.review_dir)
            self._apply_result(artifacts=artifacts, key="policy", result=result, session=session)
            warnings.extend(result.warnings)
        elif recommendation_type == "environment_review":
            result = self.environment_writer.write(project_path=session.project_path, review_dir=session.review_dir)
            self._apply_result(artifacts=artifacts, key="environment", result=result, session=session)
            warnings.extend(result.warnings)
        return RecommendationArtifactWrite(artifacts=artifacts, warnings=warnings)

    def _apply_result(
        self,
        *,
        artifacts: dict,
        key: str,
        result: CandidateWriteResult,
        session: ReviewSession,
    ) -> None:
        """Record generated artifact status and manifest files on the review session."""
        artifacts[key] = {"generated": result.generated, "path": result.path}
        for generated_file in result.generated_files:
            if generated_file not in session.generated_files:
                session.generated_files.append(generated_file)
