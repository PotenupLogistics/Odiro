"""CLI wrapper for building PDF Vector Hybrid RAG corpus artifacts."""

from __future__ import annotations

import sys
from pathlib import Path


# Agents root must be importable when this file runs as a script.
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.services.pdf_rag_corpus import main


if __name__ == "__main__":
    raise SystemExit(main())
