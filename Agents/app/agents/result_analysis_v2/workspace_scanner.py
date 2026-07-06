from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


# Maximum byte size for artifacts that must be loaded directly by parsers.
MAX_DIRECT_PARSE_BYTES = 5_000_000


@dataclass(frozen=True)
class WorkspaceScan:
    root: Path
    files: list[Path]
    warnings: list[str]


class WorkspaceScanner:
    """Scans run workspaces while keeping streamable artifacts available."""

    def scan(self, root: Path) -> WorkspaceScan:
        """Return parseable files and non-fatal collection warnings for one root."""
        if not root.exists():
            return WorkspaceScan(root=root, files=[], warnings=[f"experiments root does not exist: {root}"])
        if not root.is_dir():
            return WorkspaceScan(root=root, files=[], warnings=[f"experiments root is not a directory: {root}"])

        files: list[Path] = []
        warnings: list[str] = []
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            relative_path = path.relative_to(root)
            if any(part.startswith(".") for part in relative_path.parts):
                continue
            try:
                if path.stat().st_size > MAX_DIRECT_PARSE_BYTES and not self._is_streamable_large_file(path):
                    warnings.append(f"skipped large file: {relative_path}")
                    continue
            except OSError as exc:
                warnings.append(f"could not stat file {path}: {exc}")
                continue
            files.append(path)
        return WorkspaceScan(root=root, files=sorted(files), warnings=warnings)

    def _is_streamable_large_file(self, path: Path) -> bool:
        """Allow large artifacts only when a parser handles them without raw loading."""
        return path.name == "actions.jsonl"
