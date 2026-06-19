from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class SnapshotHashBuilder:
    """Computes deterministic snapshot hashes for a project run."""

    required_files: tuple[str, ...] = ("scenario.json", "profile.json", "setting.json")

    def build(self, *, project_path: Path, run_id: str) -> dict[str, Any]:
        """Hash run snapshot files first, falling back to project root files."""
        run_snapshot = project_path / "runs" / run_id / "snapshot"
        base_path = run_snapshot if run_snapshot.is_dir() else project_path
        source = "run_snapshot" if run_snapshot.is_dir() else "project_root"
        files = self._collect_files(base_path)
        missing_sources = [name for name in self.required_files if name not in files]
        file_hashes = {
            key: {
                "sha256": self._sha256(path),
                "source_path": path.relative_to(project_path).as_posix(),
            }
            for key, path in sorted(files.items())
        }
        return {
            "source": source,
            "files": file_hashes,
            "overall_hash": self._overall_hash(file_hashes),
            "missing_sources": missing_sources,
        }

    def _collect_files(self, base_path: Path) -> dict[str, Path]:
        """Collect supported snapshot files using stable logical names."""
        files: dict[str, Path] = {}
        for name in self.required_files:
            path = base_path / name
            if path.is_file():
                files[name] = path
        policy_dir = base_path / "policy"
        if policy_dir.is_dir():
            for path in sorted(item for item in policy_dir.rglob("*") if item.is_file()):
                if self._is_ignored_policy_file(path, policy_dir):
                    continue
                files[f"policy/{path.relative_to(policy_dir).as_posix()}"] = path
        return files

    def _is_ignored_policy_file(self, path: Path, policy_dir: Path) -> bool:
        """Exclude runtime/cache files from policy snapshot hashing."""
        relative_parts = path.relative_to(policy_dir).parts
        return "__pycache__" in relative_parts or path.suffix == ".pyc" or path.name == ".DS_Store"

    def _sha256(self, path: Path) -> str:
        """Hash one file using binary-safe sha256."""
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()

    def _overall_hash(self, file_hashes: dict[str, dict[str, str]]) -> str | None:
        """Hash sorted logical paths and file hashes into one snapshot digest."""
        if not file_hashes:
            return None
        digest = hashlib.sha256()
        for key, value in sorted(file_hashes.items()):
            digest.update(key.encode("utf-8"))
            digest.update(value["sha256"].encode("utf-8"))
        return digest.hexdigest()
