from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import UTC, datetime
from enum import Enum
from pathlib import Path

from pydantic import BaseModel, ConfigDict

from app.models.episode_spec import EpisodeConversionWarning
from app.utils.report_serialization import MASKED_VALUE, to_jsonable, write_json_report


ROOT = Path(__file__).resolve().parents[1]


class SampleEnum(str, Enum):
    value_a = "value_a"


class SampleModel(BaseModel):
    model_config = ConfigDict(extra="forbid")

    name: str
    when: datetime


@dataclass
class SampleDataclass:
    path: Path
    state: SampleEnum


def test_pydantic_model_to_jsonable_dict() -> None:
    now = datetime(2026, 6, 1, tzinfo=UTC)
    result = to_jsonable(SampleModel(name="sample", when=now))

    assert result == {"name": "sample", "when": "2026-06-01T00:00:00Z"}


def test_enum_datetime_path_and_nested_values_are_jsonable() -> None:
    now = datetime(2026, 6, 1, tzinfo=UTC)
    result = to_jsonable(
        {
            "enum": SampleEnum.value_a,
            "when": now,
            "path": Path("harness/reports/report.json"),
            "items": [SampleModel(name="nested", when=now)],
            "dataclass": SampleDataclass(path=Path("docs"), state=SampleEnum.value_a),
        }
    )

    json.dumps(result)
    assert result["enum"] == "value_a"
    assert result["when"] == now.isoformat()
    assert result["path"] == "harness\\reports\\report.json" or result["path"] == "harness/reports/report.json"
    assert result["items"][0]["name"] == "nested"
    assert result["dataclass"]["state"] == "value_a"


def test_episode_conversion_warning_is_jsonable() -> None:
    warning = EpisodeConversionWarning(
        code="kickboard_prop_mapping_pending",
        message="Mapped Kickboard to temporary obstacle.",
        sourcePath="obstacles[0].type",
    )

    result = to_jsonable({"warnings": [warning]})

    assert result["warnings"][0]["code"] == "kickboard_prop_mapping_pending"
    json.dumps(result)


def test_secret_like_keys_are_masked() -> None:
    result = to_jsonable(
        {
            "OPENAI_API_KEY": "real-value",
            "apiKey": "real-value",
            "api_key": "real-value",
            "authorization": "Bearer real-value",
            "token": "real-value",
            "secret": "real-value",
            "safe": "visible",
        }
    )

    assert result["safe"] == "visible"
    for key, value in result.items():
        if key != "safe":
            assert value == MASKED_VALUE


def test_write_json_report_writes_json(tmp_path: Path) -> None:
    out = tmp_path / "report.json"
    write_json_report(out, {"warning": EpisodeConversionWarning(code="c", message="m")})

    payload = json.loads(out.read_text(encoding="utf-8"))
    assert payload["warning"]["code"] == "c"


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
