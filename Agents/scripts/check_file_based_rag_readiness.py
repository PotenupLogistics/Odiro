from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.validate_file_based_rag_store import validate_file_based_rag_store
from scripts.validate_policy_chunk_candidates import validate_policy_chunk_candidates


@dataclass(frozen=True)
class ReadinessResult:
    runtime_validation_passed: bool
    source_inventory_validation_passed: bool
    runtime_source_status_guard_passed: bool
    candidate_validation_passed: bool | None
    vector_db_directories_absent: bool
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.errors and all(
            [
                self.runtime_validation_passed,
                self.source_inventory_validation_passed,
                self.runtime_source_status_guard_passed,
                self.vector_db_directories_absent,
                self.candidate_validation_passed is not False,
            ]
        )


def check_file_based_rag_readiness(
    root: Path = ROOT,
    skip_candidates: bool = False,
) -> ReadinessResult:
    runtime_result = validate_file_based_rag_store(root)
    errors = list(runtime_result.errors)
    warnings: list[str] = []

    candidate_passed: bool | None = None
    if not skip_candidates:
        candidate_result = validate_policy_chunk_candidates(root)
        candidate_passed = candidate_result.passed
        errors.extend(candidate_result.errors)
        warnings.extend(candidate_result.warnings)

    return ReadinessResult(
        runtime_validation_passed=runtime_result.passed,
        source_inventory_validation_passed=runtime_result.source_inventory_exists
        and not any("source_inventory.json" in error for error in runtime_result.errors),
        runtime_source_status_guard_passed=runtime_result.runtime_source_status_guard_passed,
        candidate_validation_passed=candidate_passed,
        vector_db_directories_absent=runtime_result.vector_db_directories_absent,
        errors=errors,
        warnings=warnings,
    )


def _status(value: bool | None) -> str:
    if value is None:
        return "SKIPPED"
    return "PASS" if value else "FAIL"


def _print_summary(result: ReadinessResult, verbose: bool = False) -> None:
    print("file-based RAG readiness check")
    print(f"runtime store validation: {_status(result.runtime_validation_passed)}")
    print(f"source inventory validation: {_status(result.source_inventory_validation_passed)}")
    print(f"runtime source status guard: {_status(result.runtime_source_status_guard_passed)}")
    print(f"candidate chunk validation: {_status(result.candidate_validation_passed)}")
    print(
        "vector DB directories: "
        + ("absent" if result.vector_db_directories_absent else "present")
    )

    if verbose and result.warnings:
        print("warnings:")
        for warning in result.warnings:
            print(f"- {warning}")
    if result.errors:
        print("errors:")
        for error in result.errors:
            print(f"- {error}")
    print(f"result: {'PASS' if result.passed else 'FAIL'}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the file-based RAG readiness check.")
    parser.add_argument(
        "--root",
        default=str(ROOT),
        help="Repository root. Defaults to the current Proto-AI checkout.",
    )
    parser.add_argument(
        "--skip-candidates",
        action="store_true",
        help="Skip policy chunk candidate validation.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print validator warnings and detailed failure output.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = check_file_based_rag_readiness(
        Path(args.root),
        skip_candidates=args.skip_candidates,
    )
    _print_summary(result, verbose=args.verbose)
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
