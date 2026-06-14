from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ArtifactInfo:
    path: Path
    relative_path: str
    artifact_type: str
    experiment_id: str | None = None
    run_id: str | None = None
    episode_id: str | None = None


class ArtifactClassifier:
    def classify(self, root: Path, path: Path) -> ArtifactInfo:
        relative = path.relative_to(root)
        parts = relative.parts
        experiment_id = parts[0] if parts else None
        run_id = self._after(parts, "runs")
        episode_id = self._after(parts, "episodes")
        name = path.name.lower()
        artifact_type = "unknown"

        if name == "setting.json":
            artifact_type = "experiment_setting"
        elif name == "profile.json":
            artifact_type = "experiment_profile"
        elif len(parts) >= 3 and parts[1] == "policy" and name.endswith(".json"):
            artifact_type = "experiment_policy_config"
        elif len(parts) >= 3 and parts[1] == "policy" and name.endswith(".py"):
            artifact_type = "experiment_policy_source"
        elif len(parts) >= 3 and parts[1] == "scenarios" and name.endswith(".json"):
            artifact_type = "scenario_sample"
        elif "runs" in parts and name == "summary.json":
            artifact_type = "run_summary"
        elif "runs" in parts and "policy" in parts:
            artifact_type = "run_policy_snapshot"
        elif "episodes" in parts and name == "result.json":
            artifact_type = "episode_result"
        elif "episodes" in parts and name == "events.jsonl":
            artifact_type = "episode_events"
        elif "episodes" in parts and name == "actions.jsonl":
            artifact_type = "episode_actions"
        elif "episodes" in parts and name == "trace.jsonl":
            artifact_type = "episode_trace"
        elif "episodes" in parts and name == "preview.png":
            artifact_type = "episode_preview"
        elif "captures" in parts:
            artifact_type = "episode_capture"

        return ArtifactInfo(
            path=path,
            relative_path=relative.as_posix(),
            artifact_type=artifact_type,
            experiment_id=experiment_id,
            run_id=run_id,
            episode_id=episode_id,
        )

    def _after(self, parts: tuple[str, ...], marker: str) -> str | None:
        if marker not in parts:
            return None
        index = parts.index(marker) + 1
        return parts[index] if index < len(parts) else None

