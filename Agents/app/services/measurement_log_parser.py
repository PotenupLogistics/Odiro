from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class MeasurementLogParseError(ValueError):
    def __init__(self, message: str, code: str = "measurement_log_parse_error") -> None:
        super().__init__(message)
        self.code = code
        self.message = message


@dataclass(frozen=True)
class MeasurementLogData:
    header: dict[str, Any]
    ticks: list[dict[str, Any]]
    footer: dict[str, Any]
    sourcePath: str


def parse_measurement_log(path: str | Path) -> MeasurementLogData:
    log_path = Path(path)
    if not log_path.exists():
        raise MeasurementLogParseError(
            f"Measurement log file not found: {log_path}",
            "measurement_log_file_missing",
        )

    raw_lines = log_path.read_text(encoding="utf-8-sig").splitlines()
    records: list[dict[str, Any]] = []
    for index, line in enumerate(raw_lines):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            record = json.loads(stripped)
        except json.JSONDecodeError as exc:
            raise MeasurementLogParseError(
                f"Invalid JSON on line {index + 1}: {exc.msg}",
                "measurement_log_invalid_json",
            ) from exc
        if not isinstance(record, dict):
            raise MeasurementLogParseError(
                f"Non-object record on line {index + 1}",
                "measurement_log_non_object",
            )
        records.append(record)

    if not records:
        raise MeasurementLogParseError(
            "Measurement log is empty.",
            "measurement_log_empty",
        )

    header = records[0]
    if header.get("type") != "header":
        raise MeasurementLogParseError(
            "First record is not a header.",
            "measurement_log_missing_header",
        )

    footer = records[-1]
    if footer.get("type") != "footer":
        raise MeasurementLogParseError(
            "Last record is not a footer.",
            "measurement_log_missing_footer",
        )

    ticks = [record for record in records[1:-1] if record.get("type") == "tick"]
    if not ticks:
        raise MeasurementLogParseError(
            "Measurement log has no tick records.",
            "measurement_log_no_ticks",
        )

    return MeasurementLogData(
        header=header,
        ticks=ticks,
        footer=footer,
        sourcePath=str(log_path),
    )


def parse_measurement_log_lenient(path: str | Path) -> MeasurementLogData:
    """footer가 없는 로그도 허용하는 관대한 파서.

    언리얼이 에피소드 중단 등으로 footer를 기록하지 못한 로그도 분석할 수 있도록,
    header/footer 누락이나 파싱 불가 라인을 에러 대신 건너뛴다.
    type별로 header/footer/tick을 분류만 한다.
    """
    log_path = Path(path)
    if not log_path.exists():
        raise MeasurementLogParseError(
            f"Measurement log file not found: {log_path}",
            "measurement_log_file_missing",
        )

    records: list[dict[str, Any]] = []
    for line in log_path.read_text(encoding="utf-8-sig").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        try:
            record = json.loads(stripped)
        except json.JSONDecodeError:
            continue
        if isinstance(record, dict):
            records.append(record)

    header = next((r for r in records if r.get("type") == "header"), {})
    footer = next((r for r in reversed(records) if r.get("type") == "footer"), {})
    ticks = [r for r in records if r.get("type") == "tick"]
    return MeasurementLogData(
        header=header,
        ticks=ticks,
        footer=footer,
        sourcePath=str(log_path),
    )
