from __future__ import annotations

from typing import Final

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


# Canonical ids map directly to bundled static/templates/scenario preset files.
CANONICAL_PRESET_IDS: Final[frozenset[str]] = frozenset({"blank", "line", "curved", "barricade", "s-curve"})

# Legacy ids are resolved before any filesystem lookup reaches the loader.
PRESET_ALIASES: Final[dict[str, str]] = {
    "curved-road": "curved",
    "demo": "line",
}


class ScenarioPresetRegistry:
    """Selects and resolves semantic scenario preset ids without reading files."""

    def resolve(self, preset_id: str | None) -> str | None:
        """Return the canonical preset id for a user or legacy preset id."""
        if not isinstance(preset_id, str):
            return None
        normalized = preset_id.strip().lower()
        if not normalized:
            return None
        resolved = PRESET_ALIASES.get(normalized, normalized)
        return resolved if resolved in CANONICAL_PRESET_IDS else None

    def select(self, intent: ScenarioIntent) -> str | None:
        """Choose the canonical preset that best matches the parsed user intent."""
        if intent.robot_anchor_only:
            return None
        resolved = self.resolve(intent.preset_profile)
        if resolved is not None:
            return resolved
        if intent.corridor_profile == "curved-road":
            return self.resolve("curved-road")
        if intent.requested_length_m is not None:
            return self.resolve("line")
        return None
