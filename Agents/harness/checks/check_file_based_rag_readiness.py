from __future__ import annotations

from scripts.check_file_based_rag_readiness import check_file_based_rag_readiness


def run_check() -> dict:
    result = check_file_based_rag_readiness()
    return {
        "passed": result.passed,
        "runtimeStoreValidationPassed": result.runtime_validation_passed,
        "sourceInventoryValidationPassed": result.source_inventory_validation_passed,
        "runtimeSourceStatusGuardPassed": result.runtime_source_status_guard_passed,
        "candidateChunkValidationPassed": result.candidate_validation_passed is not False,
        "vectorDbDirectoriesAbsent": result.vector_db_directories_absent,
        "errors": result.errors,
        "warnings": result.warnings,
    }


if __name__ == "__main__":
    raise SystemExit(0 if run_check()["passed"] else 1)

