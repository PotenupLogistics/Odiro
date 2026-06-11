from __future__ import annotations

import re
from pathlib import Path

from pypdf import PdfReader


def normalize_for_match(value: str) -> str:
    return re.sub(r"\s+", "", value).lower()


def short_snippet(text: str, start: int, end: int, radius: int = 90) -> str:
    left = max(0, start - radius)
    right = min(len(text), end + radius)
    return re.sub(r"\s+", " ", text[left:right]).strip()[:260]


def extract_pages(pdf_path: Path) -> list[dict]:
    reader = PdfReader(str(pdf_path))
    pages: list[dict] = []
    for index, page in enumerate(reader.pages, start=1):
        try:
            text = page.extract_text() or ""
        except Exception:
            text = ""
        pages.append(
            {
                "pageNumber": index,
                "text": text,
                "normalizedText": normalize_for_match(text),
            }
        )
    return pages


def find_page_hints_in_pages(pages: list[dict], extracted_text: str, category: str) -> dict:
    needle = extracted_text.strip()
    normalized_needle = normalize_for_match(needle)
    hints: list[dict] = []

    if needle:
        for page in pages:
            idx = page["text"].find(needle)
            if idx != -1:
                hints.append(
                    {
                        "pageNumber": page["pageNumber"],
                        "matchType": "exact",
                        "matchedSnippet": short_snippet(page["text"], idx, idx + len(needle)),
                        "confidence": "high",
                    }
                )
                return {
                    "hintStatus": "found",
                    "pageHints": hints,
                    "note": "Exact text match found in extracted PDF text. Page number is a review hint, not confirmed evidence.",
                }

    if normalized_needle:
        partial = normalized_needle[: min(80, len(normalized_needle))]
        if len(partial) >= 24:
            for page in pages:
                idx = page["normalizedText"].find(partial)
                if idx != -1:
                    hints.append(
                        {
                            "pageNumber": page["pageNumber"],
                            "matchType": "normalized_partial",
                            "matchedSnippet": short_snippet(page["text"], 0, min(len(page["text"]), 160)),
                            "confidence": "medium",
                        }
                    )
                    return {
                        "hintStatus": "partial",
                        "pageHints": hints,
                        "note": "Normalized partial text match found. Manual PDF verification is required.",
                    }

    keyword_terms = [term for term in re.split(r"[_\W]+", category) if len(term) >= 5]
    for page in pages:
        lower_text = page["text"].lower()
        for term in keyword_terms:
            idx = lower_text.find(term.lower())
            if idx != -1:
                hints.append(
                    {
                        "pageNumber": page["pageNumber"],
                        "matchType": "keyword_nearby",
                        "matchedSnippet": short_snippet(page["text"], idx, idx + len(term)),
                        "confidence": "low",
                    }
                )
                return {
                    "hintStatus": "needs_manual_page_search",
                    "pageHints": hints,
                    "note": "Only a weak keyword nearby hint was found. Manual page search is required.",
                }

    return {
        "hintStatus": "not_found",
        "pageHints": [],
        "note": "No text match found. This may be due to table/image/OCR extraction limits.",
    }


def find_page_hints(pdf_path: Path, extracted_text: str, category: str) -> dict:
    return find_page_hints_in_pages(extract_pages(pdf_path), extracted_text, category)
