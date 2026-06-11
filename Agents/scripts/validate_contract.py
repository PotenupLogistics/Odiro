from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.core.contract_types import ContractType
from app.services.json_contract_validator import validate_json_file


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate an external JSON payload against a contract schema.")
    parser.add_argument("--type", required=True, choices=[item.value for item in ContractType])
    parser.add_argument("--file", required=True, help="Path to the JSON file to validate.")
    parser.add_argument("--report", help="Optional path to write a validation report JSON.")
    return parser


def _write_report(report_path: str, contract_type: ContractType, file_path: str, result: object) -> None:
    payload = {
        "contractType": contract_type.value,
        "filePath": file_path,
        "valid": result.valid,
        "errors": result.errors,
        "warnings": result.warnings,
        "checkedAt": datetime.now(timezone.utc).isoformat(),
    }
    path = Path(report_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    contract_type = ContractType(args.type)
    result = validate_json_file(contract_type, args.file)

    print(f"{'PASS' if result.valid else 'FAIL'}: {contract_type.value}")
    if result.errors:
        print("Errors:")
        for error in result.errors:
            print(f"- {error}")
    if result.warnings:
        print("Warnings:")
        for warning in result.warnings:
            print(f"- {warning}")

    if args.report:
        _write_report(args.report, contract_type, args.file, result)
        print(f"Report written to: {args.report}")

    return 0 if result.valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
