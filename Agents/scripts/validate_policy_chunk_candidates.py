from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SOURCE_INVENTORY_FILE = Path("data") / "sources" / "source_inventory.json"
CANDIDATES_DIR = Path("data") / "sources" / "review" / "candidates"
TEMPLATES_DIR = Path("data") / "sources" / "review" / "templates"

ALLOWED_CANDIDATE_SOURCE_STATUSES = (
    "candidate_active",
    "supporting_candidate",
    "reference_only",
    "review_candidate",
)
PROMOTED_RUNTIME_SOURCE_STATUS = "active"
ALLOWED_REVIEW_STATUSES = (
    "draft",
    "candidate",
    "needs_revision",
    "confirmed",
    "rejected",
)


@dataclass(frozen=True)
class CandidateValidationResult:
    candidate_count: int
    validated_files: list[str]
    warnings: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.errors


def _display_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _read_json(path: Path, root: Path, errors: list[str]) -> dict[str, Any]:
    display_path = _display_path(path, root)
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        errors.append(f"{display_path}: invalid JSON ({exc.msg})")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{display_path}: JSON root must be an object")
        return {}
    return value


def _load_inventory(root: Path, errors: list[str]) -> dict[str, str]:
    path = root / SOURCE_INVENTORY_FILE
    display_path = _display_path(path, root)
    if not path.exists():
        errors.append(f"{display_path}: missing file")
        return {}

    payload = _read_json(path, root, errors)
    sources = payload.get("sources")
    if not isinstance(sources, list):
        errors.append(f"{display_path}: sources must be a list")
        return {}

    status_by_source_id: dict[str, str] = {}
    for index, source in enumerate(sources, start=1):
        if not isinstance(source, dict):
            errors.append(f"{display_path} sources[{index}]: source must be an object")
            continue
        source_id = source.get("source_id")
        status = source.get("status")
        if isinstance(source_id, str) and isinstance(status, str):
            status_by_source_id[source_id] = status
    return status_by_source_id


def _candidate_files(root: Path) -> list[Path]:
    candidates_root = root / CANDIDATES_DIR
    if not candidates_root.exists():
        return []
    template_root = root / TEMPLATES_DIR
    files: list[Path] = []
    for path in candidates_root.rglob("*.json"):
        if template_root in path.parents:
            continue
        if "template" in path.name.lower():
            continue
        files.append(path)
    return sorted(files)


def _require_non_empty_string(
    payload: dict[str, Any],
    field_name: str,
    display_path: str,
    errors: list[str],
) -> None:
    value = payload.get(field_name)
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{display_path}: {field_name} must be a non-empty string")


def _validate_candidate(
    path: Path,
    root: Path,
    status_by_source_id: dict[str, str],
    warnings: list[str],
    errors: list[str],
) -> None:
    display_path = _display_path(path, root)
    payload = _read_json(path, root, errors)
    if not payload:
        return

    for field_name in (
        "schema",
        "version",
        "source_id",
        "source_status_at_review",
        "candidate_id",
        "review_status",
        "category",
        "chunkText",
        "relatedActions",
        "relatedPolicyParams",
        "sourceEvidence",
        "promotionDecision",
    ):
        if field_name not in payload:
            errors.append(f"{display_path}: missing {field_name}")

    if payload.get("schema") != "policy_chunk_candidate":
        errors.append(f"{display_path}: schema must be policy_chunk_candidate")

    source_id = payload.get("source_id")
    if not isinstance(source_id, str) or not source_id.strip():
        errors.append(f"{display_path}: source_id must be a non-empty string")
    elif source_id not in status_by_source_id:
        errors.append(f"{display_path}: source_id {source_id} is not registered in source inventory")
    else:
        inventory_status = status_by_source_id[source_id]
        promotion_decision = payload.get("promotionDecision")
        promoted_confirmed_candidate = (
            inventory_status == PROMOTED_RUNTIME_SOURCE_STATUS
            and payload.get("review_status") == "confirmed"
            and isinstance(promotion_decision, dict)
            and promotion_decision.get("can_promote_to_runtime") is True
            and payload.get("source_status_at_review") in ALLOWED_CANDIDATE_SOURCE_STATUSES
        )
        if inventory_status not in ALLOWED_CANDIDATE_SOURCE_STATUSES and not promoted_confirmed_candidate:
            errors.append(
                f"{display_path}: source_id {source_id} has status {inventory_status}, "
                "but candidate chunks require candidate_active, supporting_candidate, "
                "reference_only, review_candidate, or a confirmed promoted active source"
            )
        if payload.get("source_status_at_review") != inventory_status and not promoted_confirmed_candidate:
            errors.append(
                f"{display_path}: source_status_at_review must match inventory status {inventory_status}"
            )

    review_status = payload.get("review_status")
    if review_status not in ALLOWED_REVIEW_STATUSES:
        errors.append(f"{display_path}: invalid review_status {review_status}")

    _require_non_empty_string(payload, "candidate_id", display_path, errors)
    _require_non_empty_string(payload, "category", display_path, errors)
    _require_non_empty_string(payload, "chunkText", display_path, errors)

    if not isinstance(payload.get("relatedActions"), list):
        errors.append(f"{display_path}: relatedActions must be a list")
    if not isinstance(payload.get("relatedPolicyParams"), list):
        errors.append(f"{display_path}: relatedPolicyParams must be a list")

    source_evidence = payload.get("sourceEvidence")
    if not isinstance(source_evidence, dict):
        errors.append(f"{display_path}: sourceEvidence must be an object")
    else:
        for field_name in ("document_path", "page", "section", "evidence_text"):
            value = source_evidence.get(field_name)
            if not isinstance(value, str) or not value.strip():
                errors.append(f"{display_path}: sourceEvidence.{field_name} must be a non-empty string")

    promotion_decision = payload.get("promotionDecision")
    if not isinstance(promotion_decision, dict):
        errors.append(f"{display_path}: promotionDecision must be an object")
    else:
        can_promote = promotion_decision.get("can_promote_to_runtime")
        if not isinstance(can_promote, bool):
            errors.append(f"{display_path}: promotionDecision.can_promote_to_runtime must be a boolean")
        if review_status == "confirmed" and can_promote is False:
            errors.append(
                f"{display_path}: confirmed candidates must set "
                "promotionDecision.can_promote_to_runtime to true or change review_status"
            )
        if can_promote is True:
            warnings.append(
                f"{display_path}: can_promote_to_runtime=true is advisory only; "
                "this validator never modifies policy_rag_chunks.jsonl"
            )


def validate_policy_chunk_candidates(root: Path = ROOT) -> CandidateValidationResult:
    root = root.resolve()
    errors: list[str] = []
    warnings: list[str] = []
    status_by_source_id = _load_inventory(root, errors)
    files: list[Path] = []

    for path in _candidate_files(root):
        payload = _read_json(path, root, errors)
        if payload.get("schema") != "policy_chunk_candidate":
            continue
        files.append(path)
        _validate_candidate(path, root, status_by_source_id, warnings, errors)

    return CandidateValidationResult(
        candidate_count=len(files),
        validated_files=[_display_path(path, root) for path in files],
        warnings=warnings,
        errors=errors,
    )


def _print_summary(result: CandidateValidationResult) -> None:
    print(f"candidate files: {result.candidate_count}")
    if result.validated_files:
        print("validated files:")
        for path in result.validated_files:
            print(f"- {path}")
    if result.warnings:
        print("warnings:")
        for warning in result.warnings:
            print(f"- {warning}")
    if result.errors:
        print("errors:")
        for error in result.errors:
            print(f"- {error}")
    print(f"result: {'PASS' if result.passed else 'FAIL'}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate policy chunk candidate files.")
    parser.add_argument(
        "--root",
        default=str(ROOT),
        help="Repository root. Defaults to the current Proto-AI checkout.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = validate_policy_chunk_candidates(Path(args.root))
    _print_summary(result)
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
