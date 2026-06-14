from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class NormalizedScenarioRequest:
    raw_prompt: str
    normalized_prompt: str


class RequestNormalizer:
    def normalize(self, prompt: str) -> NormalizedScenarioRequest:
        normalized = " ".join(prompt.strip().split())
        return NormalizedScenarioRequest(raw_prompt=prompt, normalized_prompt=normalized)

