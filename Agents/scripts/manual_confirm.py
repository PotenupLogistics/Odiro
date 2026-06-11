from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from copy import deepcopy
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RESULTS_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
PAGE_HINTS_PATH = ROOT / "data" / "sources" / "review" / "high_priority" / "page_hints" / "high_priority_page_hints.json"
VALID_STATUSES = {"pending_manual_confirmation", "confirmed", "rejected"}

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(errors="replace")


def load_results(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def save_results(path: Path, payload: dict[str, Any]) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8-sig")


def load_page_hints() -> dict[str, dict[str, Any]]:
    if not PAGE_HINTS_PATH.exists():
        return {}
    payload = json.loads(PAGE_HINTS_PATH.read_text(encoding="utf-8-sig"))
    return {item["candidateId"]: item for item in payload.get("items", [])}


def find_item(payload: dict[str, Any], candidate_id: str) -> dict[str, Any]:
    for item in payload.get("items", []):
        if item.get("candidateId") == candidate_id:
            return item
    raise SystemExit(f"candidateId not found: {candidate_id}")


def short(value: str, limit: int = 120) -> str:
    value = " ".join(value.split())
    return value if len(value) <= limit else value[:limit].rstrip() + "..."


def recompute_summary(payload: dict[str, Any]) -> None:
    counts = Counter(item.get("manualReviewStatus") for item in payload.get("items", []))
    payload["statusSummary"] = {
        "pending_manual_confirmation": counts["pending_manual_confirmation"],
        "confirmed": counts["confirmed"],
        "rejected": counts["rejected"],
    }


def print_item_row(item: dict[str, Any]) -> None:
    print(
        f"{item['candidateId']} | {item['sourceId']} | {item['category']} | "
        f"{item['manualReviewStatus']} | {short(item['extractedText'])}"
    )


def command_list(args: argparse.Namespace) -> int:
    payload = load_results(args.file)
    for item in payload.get("items", []):
        if args.source and item.get("sourceId") != args.source:
            continue
        if args.status and item.get("manualReviewStatus") != args.status:
            continue
        print_item_row(item)
    return 0


def command_show(args: argparse.Namespace) -> int:
    payload = load_results(args.file)
    item = find_item(payload, args.candidate_id)
    page_hints = load_page_hints().get(args.candidate_id)

    print(json.dumps(item, ensure_ascii=False, indent=2))
    if page_hints:
        print("\nPage hints:")
        print(json.dumps(page_hints, ensure_ascii=False, indent=2))
    return 0


def command_summary(args: argparse.Namespace) -> int:
    payload = load_results(args.file)
    counts = Counter(item.get("manualReviewStatus") for item in payload.get("items", []))
    by_source: dict[str, Counter] = defaultdict(Counter)
    for item in payload.get("items", []):
        by_source[item.get("sourceId")][item.get("manualReviewStatus")] += 1

    print(f"pending_manual_confirmation: {counts['pending_manual_confirmation']}")
    print(f"confirmed: {counts['confirmed']}")
    print(f"rejected: {counts['rejected']}")
    print("\nBy source:")
    for source_id in sorted(by_source):
        source_counts = by_source[source_id]
        print(
            f"{source_id}: pending={source_counts['pending_manual_confirmation']}, "
            f"confirmed={source_counts['confirmed']}, rejected={source_counts['rejected']}"
        )
    return 0


def ensure_can_change(item: dict[str, Any], force: bool) -> None:
    if item.get("manualReviewStatus") in {"confirmed", "rejected"} and not force:
        raise SystemExit("candidate is already confirmed/rejected; use --force to change it")


def command_confirm(args: argparse.Namespace) -> int:
    if not (args.page or args.section):
        raise SystemExit("confirm requires --page or --section")
    if not args.text:
        raise SystemExit("confirm requires --text")
    if not args.reviewer:
        raise SystemExit("confirm requires --reviewer")
    if not args.reason:
        raise SystemExit("confirm requires --reason")

    payload = load_results(args.file)
    updated = deepcopy(payload)
    item = find_item(updated, args.candidate_id)
    ensure_can_change(item, args.force)
    before = deepcopy(item)

    item["manualReviewStatus"] = "confirmed"
    item["rawPdfPage"] = args.page or ""
    item["rawPdfSection"] = args.section or ""
    item["confirmedText"] = args.text
    item["reviewer"] = args.reviewer
    item["reviewedAt"] = args.reviewed_at or datetime.now(timezone.utc).date().isoformat()
    item["decisionReason"] = args.reason
    item["nextAction"] = args.next_action
    recompute_summary(updated)

    print("Before:")
    print(json.dumps(before, ensure_ascii=False, indent=2))
    print("\nAfter:")
    print(json.dumps(item, ensure_ascii=False, indent=2))

    if args.yes:
        save_results(args.file, updated)
        print("\nSaved.")
    else:
        print("\nDry-run only. Re-run with --yes to save.")
    return 0


def command_reject(args: argparse.Namespace) -> int:
    if not args.reviewer:
        raise SystemExit("reject requires --reviewer")
    if not args.reason:
        raise SystemExit("reject requires --reason")

    payload = load_results(args.file)
    updated = deepcopy(payload)
    item = find_item(updated, args.candidate_id)
    ensure_can_change(item, args.force)
    before = deepcopy(item)

    item["manualReviewStatus"] = "rejected"
    item["reviewer"] = args.reviewer
    item["reviewedAt"] = args.reviewed_at or datetime.now(timezone.utc).date().isoformat()
    item["decisionReason"] = args.reason
    item["nextAction"] = args.next_action
    recompute_summary(updated)

    print("Before:")
    print(json.dumps(before, ensure_ascii=False, indent=2))
    print("\nAfter:")
    print(json.dumps(item, ensure_ascii=False, indent=2))

    if args.yes:
        save_results(args.file, updated)
        print("\nSaved.")
    else:
        print("\nDry-run only. Re-run with --yes to save.")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Manual confirmation helper for high priority policy candidates.")
    parser.add_argument("--file", type=Path, default=DEFAULT_RESULTS_PATH, help="manual confirmation results JSON path")
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list")
    list_parser.add_argument("--source")
    list_parser.add_argument("--status", choices=sorted(VALID_STATUSES))
    list_parser.set_defaults(func=command_list)

    show_parser = subparsers.add_parser("show")
    show_parser.add_argument("candidate_id")
    show_parser.set_defaults(func=command_show)

    summary_parser = subparsers.add_parser("summary")
    summary_parser.set_defaults(func=command_summary)

    confirm_parser = subparsers.add_parser("confirm")
    confirm_parser.add_argument("candidate_id")
    confirm_parser.add_argument("--page", default="")
    confirm_parser.add_argument("--section", default="")
    confirm_parser.add_argument("--text", default="")
    confirm_parser.add_argument("--reviewer", default="")
    confirm_parser.add_argument("--reviewed-at", default="")
    confirm_parser.add_argument("--reason", default="")
    confirm_parser.add_argument("--next-action", default="create_policy_card")
    confirm_parser.add_argument("--force", action="store_true")
    confirm_parser.add_argument("--yes", action="store_true")
    confirm_parser.set_defaults(func=command_confirm)

    reject_parser = subparsers.add_parser("reject")
    reject_parser.add_argument("candidate_id")
    reject_parser.add_argument("--reviewer", default="")
    reject_parser.add_argument("--reviewed-at", default="")
    reject_parser.add_argument("--reason", default="")
    reject_parser.add_argument("--next-action", default="exclude_from_policy_card")
    reject_parser.add_argument("--force", action="store_true")
    reject_parser.add_argument("--yes", action="store_true")
    reject_parser.set_defaults(func=command_reject)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
