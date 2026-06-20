from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from app.agents.result_analysis_v2.artifact_classifier import ArtifactInfo


@dataclass(frozen=True)
class ParsedArtifact:
    info: ArtifactInfo
    data: Any
    warnings: list[str]


class ArtifactParser:
    def parse(self, info: ArtifactInfo) -> ParsedArtifact:
        if info.artifact_type in {"episode_preview", "episode_capture"}:
            return ParsedArtifact(info=info, data=self._metadata(info.path), warnings=[])
        if info.path.suffix.lower() == ".json":
            return self._parse_json(info)
        if info.path.suffix.lower() == ".jsonl":
            return self._parse_jsonl(info)
        if info.path.suffix.lower() == ".py":
            return self._parse_text(info)
        return ParsedArtifact(info=info, data=None, warnings=[])

    def _parse_json(self, info: ArtifactInfo) -> ParsedArtifact:
        try:
            return ParsedArtifact(info=info, data=json.loads(info.path.read_text(encoding="utf-8-sig")), warnings=[])
        except Exception as exc:
            return ParsedArtifact(info=info, data=None, warnings=[f"{info.relative_path}: JSON parse failed: {exc}"])

    def _parse_jsonl(self, info: ArtifactInfo) -> ParsedArtifact:
        rows: list[Any] = []
        warnings: list[str] = []
        try:
            lines = info.path.read_text(encoding="utf-8-sig").splitlines()
        except Exception as exc:
            return ParsedArtifact(info=info, data=[], warnings=[f"{info.relative_path}: read failed: {exc}"])

        for line_number, line in enumerate(lines, start=1):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                warnings.append(f"{info.relative_path}:{line_number}: JSONL parse failed: {exc.msg}")
        return ParsedArtifact(info=info, data=rows, warnings=warnings)

    def _parse_text(self, info: ArtifactInfo) -> ParsedArtifact:
        try:
            text = info.path.read_text(encoding="utf-8")
        except Exception as exc:
            return ParsedArtifact(info=info, data="", warnings=[f"{info.relative_path}: read failed: {exc}"])
        return ParsedArtifact(info=info, data=text[:20_000], warnings=[])

    def _metadata(self, path: Path) -> dict[str, Any]:
        stat = path.stat()
        return {"path": str(path), "size_bytes": stat.st_size, "modified_time": stat.st_mtime}
