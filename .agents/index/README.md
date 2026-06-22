# Agent Source Index

Agent-only YAML source index for routing code reading, ownership boundaries, and focused verification.

## Purpose
- Route source reading to the smallest useful area before broad navigation.
- Keep ownership, entry points, guardrails, and focused checks close to affected paths.
- Treat source code, contracts, specs, tests, and build files as authoritative.
- Treat cards as descriptive current-state metadata, not implementation policy.
- If a card conflicts with source, canonical docs, higher-priority instructions, or a direct user request, follow the authoritative source and update the stale card when the change affects indexed facts.

## Reading Algorithm
- Start here, then scan `cards/*.yaml`; open only cards whose `paths` or `workflows` match the task.
- Read each matching card's `entry` groups in order.
- Follow `links` only when canonical docs/specs are needed.
- Use `related` only when the change crosses area boundaries.

## Active Card Format
- Root: `.agents/index/README.md`
- Cards: `.agents/index/cards/<area>.yaml`
- Field order: `id`, `owner`, `description`, `paths`, optional `workflows`, `entry`, `guardrails`, `verify`, `links`, optional `related`.
- `entry` contains ordered read groups with `id`, `description`, optional `needs`, and `read`.
- `verify` contains grouped checks with `when` and `run`.

## Maintenance
- Keep cards concise navigation data; move long explanations to canonical docs/specs and link them.
- Prefer directory globs and stable read groups over exhaustive file/widget/test lists.
- Keep `guardrails` to ownership, boundary, migration, and non-obvious lifecycle constraints; target 10 or fewer per card.
- Keep each guardrail owned by one card. Use `related` for cross-card awareness instead of duplicating text.
- Group verification by behavior or subsystem, not every test name, unless one named test is the canonical check.
- Do not maintain a card manifest in this README; `cards/*.yaml` is the manifest to reduce concurrent-edit conflicts.
- Split a card when unrelated owners or repeated merge conflicts make edits contend on the same file.
