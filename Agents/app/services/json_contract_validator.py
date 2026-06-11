from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator
from pydantic import BaseModel, ValidationError

from app.core.contract_types import CONTRACT_MODELS, CONTRACT_SCHEMA_FILES, ContractType


class ValidationResult(BaseModel):
    valid: bool
    contractType: str
    errors: list[str]
    warnings: list[str]
    structuredErrors: list[dict[str, Any]] = []
    errorSummary: dict[str, Any] = {}
    normalizedPayload: dict[str, Any] | None = None


def load_schema(contract_type: ContractType) -> dict[str, Any]:
    schema_path = CONTRACT_SCHEMA_FILES[contract_type]
    return json.loads(schema_path.read_text(encoding="utf-8-sig"))


def parse_model(contract_type: ContractType, payload: dict[str, Any]) -> BaseModel:
    model = CONTRACT_MODELS[contract_type]
    return model.model_validate(payload)


def _error_path(path_parts: object) -> str:
    parts = list(path_parts) if path_parts is not None else []
    return ".".join(str(part) for part in parts) or "<root>"


def _schema_error_type(error: Any) -> str:
    if error.validator == "required":
        return "missing_required"
    if error.validator == "enum":
        return "enum_error"
    if error.validator == "type":
        return "type_error"
    if error.validator in {"minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum"}:
        return "range_error"
    if error.validator == "additionalProperties":
        return "additional_property"
    return str(error.validator or "validation_error")


def _schema_structured_error(error: Any) -> dict[str, Any]:
    extra_fields: list[str] = []
    if error.validator == "additionalProperties":
        extra_fields = re.findall(r"'([^']+)'", error.message)
    return {
        "path": _error_path(error.path),
        "message": error.message,
        "errorType": _schema_error_type(error),
        "expected": getattr(error, "validator_value", None),
        "actual": error.instance,
        "extraFields": extra_fields,
        "source": "schema",
    }


def _pydantic_error_type(error: dict[str, Any]) -> str:
    error_type = str(error.get("type", "validation_error"))
    if error_type == "missing":
        return "missing_required"
    if "enum" in error_type or "literal" in error_type:
        return "enum_error"
    if any(marker in error_type for marker in ["int", "float", "bool", "str", "list", "dict"]):
        return "type_error"
    return error_type


def _summarize_errors(structured_errors: list[dict[str, Any]]) -> dict[str, Any]:
    missing = [error["path"] for error in structured_errors if error.get("errorType") == "missing_required"]
    enum_errors = [error["path"] for error in structured_errors if error.get("errorType") == "enum_error"]
    type_errors = [error["path"] for error in structured_errors if error.get("errorType") == "type_error"]
    extra_fields: list[str] = []
    for error in structured_errors:
        if error.get("errorType") == "additional_property":
            location = error.get("path", "<root>")
            for field in error.get("extraFields", []):
                extra_fields.append(field if location == "<root>" else f"{location}.{field}")
        elif error.get("errorType") == "extra_forbidden":
            extra_fields.append(str(error.get("path", "<root>")))
    return {
        "totalErrors": len(structured_errors),
        "missingRequiredFields": sorted(set(missing)),
        "extraFields": sorted(set(extra_fields)),
        "enumErrors": sorted(set(enum_errors)),
        "typeErrors": sorted(set(type_errors)),
        "otherErrors": sorted(
            set(
                error["path"]
                for error in structured_errors
                if error.get("errorType")
                not in {"missing_required", "enum_error", "type_error", "additional_property", "extra_forbidden"}
            )
        ),
        "errorsByType": {
            error_type: sum(1 for error in structured_errors if error.get("errorType") == error_type)
            for error_type in sorted({str(error.get("errorType")) for error in structured_errors})
        },
    }


def _schema_errors(schema: dict[str, Any], payload: dict[str, Any]) -> tuple[list[str], list[dict[str, Any]]]:
    validator = Draft202012Validator(schema)
    errors = sorted(validator.iter_errors(payload), key=lambda error: list(error.path))
    formatted: list[str] = []
    structured: list[dict[str, Any]] = []
    for error in errors:
        location = _error_path(error.path)
        formatted.append(f"schema:{location}: {error.message}")
        structured.append(_schema_structured_error(error))
    return formatted, structured


def validate_payload(contract_type: ContractType, payload: dict[str, Any]) -> ValidationResult:
    errors: list[str] = []
    warnings: list[str] = []
    structured_errors: list[dict[str, Any]] = []
    normalized: dict[str, Any] | None = None

    schema = load_schema(contract_type)
    schema_errors, schema_structured_errors = _schema_errors(schema, payload)
    errors.extend(schema_errors)
    structured_errors.extend(schema_structured_errors)

    try:
        model = parse_model(contract_type, payload)
        normalized = model.model_dump(mode="json", exclude_none=True)
    except ValidationError as exc:
        for error in exc.errors():
            location = _error_path(error.get("loc", ()))
            errors.append(f"pydantic:{location}: {error.get('msg', 'validation error')}")
            structured_errors.append(
                {
                    "path": location,
                    "message": error.get("msg", "validation error"),
                    "errorType": _pydantic_error_type(error),
                    "expected": error.get("ctx"),
                    "actual": error.get("input"),
                    "extraFields": [location] if _pydantic_error_type(error) == "extra_forbidden" else [],
                    "source": "pydantic",
                }
            )

    return ValidationResult(
        valid=not errors,
        contractType=contract_type.value,
        errors=errors,
        warnings=warnings,
        structuredErrors=structured_errors,
        errorSummary=_summarize_errors(structured_errors),
        normalizedPayload=normalized if not errors else None,
    )


def validate_json_file(contract_type: ContractType, file_path: str) -> ValidationResult:
    path = Path(file_path)
    if not path.exists():
        return ValidationResult(
            valid=False,
            contractType=contract_type.value,
            errors=[f"file does not exist: {path}"],
            warnings=[],
            structuredErrors=[
                {
                    "path": "<file>",
                    "message": f"file does not exist: {path}",
                    "errorType": "file_not_found",
                    "expected": "existing file",
                    "actual": str(path),
                    "source": "file",
                }
            ],
            errorSummary={
                "totalErrors": 1,
                "missingRequiredFields": [],
                "extraFields": [],
                "enumErrors": [],
                "typeErrors": [],
                "otherErrors": ["<file>"],
                "errorsByType": {"file_not_found": 1},
            },
            normalizedPayload=None,
        )
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        return ValidationResult(
            valid=False,
            contractType=contract_type.value,
            errors=[f"json parse error: {exc}"],
            warnings=[],
            structuredErrors=[
                {
                    "path": "<root>",
                    "message": f"json parse error: {exc}",
                    "errorType": "json_parse_error",
                    "expected": "JSON object",
                    "actual": None,
                    "source": "json",
                }
            ],
            errorSummary={
                "totalErrors": 1,
                "missingRequiredFields": [],
                "extraFields": [],
                "enumErrors": [],
                "typeErrors": [],
                "otherErrors": ["<root>"],
                "errorsByType": {"json_parse_error": 1},
            },
            normalizedPayload=None,
        )
    if not isinstance(payload, dict):
        return ValidationResult(
            valid=False,
            contractType=contract_type.value,
            errors=["root JSON value must be an object"],
            warnings=[],
            structuredErrors=[
                {
                    "path": "<root>",
                    "message": "root JSON value must be an object",
                    "errorType": "type_error",
                    "expected": "object",
                    "actual": type(payload).__name__,
                    "source": "json",
                }
            ],
            errorSummary={
                "totalErrors": 1,
                "missingRequiredFields": [],
                "extraFields": [],
                "enumErrors": [],
                "typeErrors": ["<root>"],
                "otherErrors": [],
                "errorsByType": {"type_error": 1},
            },
            normalizedPayload=None,
        )
    return validate_payload(contract_type, payload)
