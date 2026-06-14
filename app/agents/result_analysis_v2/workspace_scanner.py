from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class WorkspaceScan:
    root: Path
    files: list[Path]
    warnings: list[str]


class WorkspaceScanner:
    def scan(self, root: Path) -> WorkspaceScan:
        if not root.exists():
            return WorkspaceScan(root=root, files=[], warnings=[f"experiments root does not exist: {root}"])
        if not root.is_dir():
            return WorkspaceScan(root=root, files=[], warnings=[f"experiments root is not a directory: {root}"])

        files: list[Path] = []
        warnings: list[str] = []
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if any(part.startswith(".") for part in path.relative_to(root).parts):
                continue
            try:
                if path.stat().st_size > 5_000_000:
                    warnings.append(f"skipped large file: {path.relative_to(root)}")
                    continue
            except OSError as exc:
                warnings.append(f"could not stat file {path}: {exc}")
                continue
            files.append(path)
        return WorkspaceScan(root=root, files=sorted(files), warnings=warnings)

