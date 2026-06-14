---
id: agent-context
owner: Root
paths:
  - AGENTS.md
  - .agents/index/**
entry:
  - AGENTS.md
  - .agents/index/INDEX.md
keep:
  - AGENTS.md owns repository-wide agent behavior, implementation, comment, and safety rules.
  - Project-local AGENTS.md files may narrow style or tooling rules inside their subtree.
  - .agents/index tracks entry points, ownership, boundaries, and focused verification flow.
verify:
  - git diff --check
related:
  - root-dev-workflow
  - bridge-host
---
