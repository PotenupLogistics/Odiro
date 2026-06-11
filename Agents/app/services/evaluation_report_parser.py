from __future__ import annotations

import json
from pathlib import Path

from pydantic import ValidationError

from app.models.evaluation_report import EvaluationReport


class EvaluationReportParseError(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)
        self.message = message


def parse_evaluation_report(path: str | Path) -> EvaluationReport:
    p = Path(path)
    if not p.exists():
        raise EvaluationReportParseError(f"파일을 찾을 수 없음: {path}")

    try:
        raw = p.read_text(encoding="utf-8").strip()
    except OSError as exc:
        raise EvaluationReportParseError(f"파일 읽기 실패: {exc}") from exc

    if not raw:
        raise EvaluationReportParseError(f"파일이 비어 있음: {path}")

    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise EvaluationReportParseError(f"JSON 파싱 실패: {exc}") from exc

    if not isinstance(data, dict):
        raise EvaluationReportParseError("최상위 JSON이 object가 아님")

    try:
        return EvaluationReport.model_validate(data)
    except ValidationError as exc:
        raise EvaluationReportParseError(f"스키마 검증 실패: {exc}") from exc
