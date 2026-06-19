from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from app.models.analysis_v2 import AnalysisRunV2Request, AnalysisRunV2Response


def utc_now_iso() -> str:
    """Return a UTC timestamp suitable for persisted review artifacts."""
    return datetime.now(UTC).isoformat()


@dataclass
class ReviewSession:
    """Holds paths and ids for one analysis review lifecycle."""

    project_path: Path
    run_id: str
    review_id: str
    review_dir: Path
    started_at: str
    based_on_review_id: str | None
    generated_files: list[str] = field(default_factory=list)


@dataclass(frozen=True)
class ReviewLifecycleManager:
    """Creates and finalizes alpha analysis review artifacts under the user project."""

    def start(self, request: AnalysisRunV2Request | None) -> ReviewSession | None:
        """Create a running review session when the requested run directory exists."""
        if request is None:
            return None
        project_path = Path(request.project_path)
        run_path = project_path / "runs" / request.run_id
        if not run_path.is_dir():
            return None

        review_root = run_path / "review"
        review_root.mkdir(parents=True, exist_ok=True)
        session = self._allocate_session(project_path=project_path, run_id=request.run_id, review_root=review_root)
        session.review_dir.mkdir()
        self._write_json(
            session,
            "status.json",
            {
                "review_id": session.review_id,
                "run_id": session.run_id,
                "status": "running",
                "started_at": session.started_at,
                "completed_at": None,
                "error": None,
            },
        )
        self._write_json(
            session,
            "request.json",
            {
                "project_path": str(project_path),
                "run_id": request.run_id,
                "prompt": request.prompt,
                "requested_at": session.started_at,
            },
        )
        return session

    def complete(
        self,
        *,
        session: ReviewSession,
        response: AnalysisRunV2Response,
        report: dict[str, Any],
        recommendations: dict[str, Any],
        manifest: dict[str, Any],
        source_run_files: list[str],
    ) -> None:
        """Persist final review artifacts and update the project analysis index."""
        completed_at = utc_now_iso()
        manifest = {
            **manifest,
            "review_id": session.review_id,
            "run_id": session.run_id,
            "generated_files": self._generated_file_names(session),
            "source_run_files": source_run_files,
            "based_on_review_id": session.based_on_review_id,
        }
        self._write_json(session, "report.json", report)
        self._write_json(session, "recommendations.json", recommendations)
        self._write_json(session, "manifest.json", manifest)
        self._write_json(
            session,
            "status.json",
            {
                "review_id": session.review_id,
                "run_id": session.run_id,
                "status": "completed",
                "started_at": session.started_at,
                "completed_at": completed_at,
                "error": None,
            },
        )
        self._update_index(
            session=session,
            response=response,
            recommendation_type=str(recommendations.get("recommendation_type", "insufficient_data")),
            completed_at=completed_at,
        )

    def fail(self, *, session: ReviewSession, code: str, message: str) -> None:
        """Persist a failed status for a session that already started."""
        self._write_json(
            session,
            "status.json",
            {
                "review_id": session.review_id,
                "run_id": session.run_id,
                "status": "failed",
                "started_at": session.started_at,
                "completed_at": utc_now_iso(),
                "error": {"code": code, "message": message},
            },
        )

    def _allocate_session(self, *, project_path: Path, run_id: str, review_root: Path) -> ReviewSession:
        """Allocate the next numeric review id with one retry for alpha concurrency."""
        based_on_review_id = self._based_on_review_id(review_root)
        for _ in range(2):
            review_id = self._next_review_id(review_root)
            session = ReviewSession(
                project_path=project_path,
                run_id=run_id,
                review_id=review_id,
                review_dir=review_root / review_id,
                started_at=utc_now_iso(),
                based_on_review_id=based_on_review_id,
            )
            if not session.review_dir.exists():
                return session
        raise FileExistsError(f"could not allocate review id under {review_root}")

    def _next_review_id(self, review_root: Path) -> str:
        """Return the next four-digit review id from existing numeric folders."""
        ids = [int(path.name) for path in review_root.iterdir() if path.is_dir() and path.name.isdecimal()]
        return f"{(max(ids) if ids else 0) + 1:04d}"

    def _based_on_review_id(self, review_root: Path) -> str | None:
        """Return the highest completed review id under a review root."""
        completed_ids: list[int] = []
        if not review_root.is_dir():
            return None
        for path in review_root.iterdir():
            if not (path.is_dir() and path.name.isdecimal()):
                continue
            status_path = path / "status.json"
            if not status_path.is_file():
                continue
            try:
                status = json.loads(status_path.read_text(encoding="utf-8"))
            except Exception:
                continue
            if status.get("status") == "completed":
                completed_ids.append(int(path.name))
        return f"{max(completed_ids):04d}" if completed_ids else None

    def _write_json(self, session: ReviewSession, filename: str, payload: dict[str, Any]) -> None:
        """Write one review artifact with deterministic formatting."""
        path = session.review_dir / filename
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")
        relative = path.relative_to(session.project_path).as_posix()
        if relative not in session.generated_files:
            session.generated_files.append(relative)

    def _generated_file_names(self, session: ReviewSession) -> list[str]:
        """Return generated files after all final artifacts are included."""
        expected = [
            f"runs/{session.run_id}/review/{session.review_id}/status.json",
            f"runs/{session.run_id}/review/{session.review_id}/request.json",
            f"runs/{session.run_id}/review/{session.review_id}/report.json",
            f"runs/{session.run_id}/review/{session.review_id}/recommendations.json",
            f"runs/{session.run_id}/review/{session.review_id}/manifest.json",
        ]
        return sorted(set(session.generated_files) | set(expected))

    def _update_index(
        self,
        *,
        session: ReviewSession,
        response: AnalysisRunV2Response,
        recommendation_type: str,
        completed_at: str,
    ) -> None:
        """Update the project-level analysis_index.json."""
        index_path = session.project_path / "analysis_index.json"
        if index_path.is_file():
            try:
                index = json.loads(index_path.read_text(encoding="utf-8"))
            except Exception:
                index = {}
        else:
            index = {}

        entry = {
            "review_id": session.review_id,
            "run_id": session.run_id,
            "status": "completed",
            "created_at": session.started_at,
            "completed_at": completed_at,
            "summary_judgement": response.summary.overall_judgement,
            "recommendation_type": recommendation_type,
            "based_on_review_id": session.based_on_review_id,
            "path": f"runs/{session.run_id}/review/{session.review_id}",
        }
        reviews = [
            self._normalize_index_entry(item)
            for item in index.get("reviews", [])
            if item.get("path") != entry["path"]
        ]
        reviews.append(entry)
        runs = index.get("runs", {})
        run_entry = runs.get(session.run_id, {"reviews": []})
        run_reviews = [
            self._normalize_index_entry(item)
            for item in run_entry.get("reviews", [])
            if item.get("review_id") != session.review_id
        ]
        run_reviews.append(entry)
        runs[session.run_id] = {**run_entry, "reviews": sorted(run_reviews, key=lambda item: item["review_id"])}
        index = {
            "latest_review_id": session.review_id,
            "reviews": sorted(reviews, key=lambda item: (item["run_id"], item["review_id"])),
            "runs": runs,
        }
        index_path.write_text(json.dumps(index, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")

    def _normalize_index_entry(self, entry: dict[str, Any]) -> dict[str, Any]:
        """Return an index entry with alpha review lineage fields present."""
        return {**entry, "based_on_review_id": entry.get("based_on_review_id")}
