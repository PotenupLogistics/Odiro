from __future__ import annotations

import warnings

from app.agents.scenario_generation_v2.agent import ScenarioGenerationV2Agent
from app.core.settings import Settings

try:
    from langgraph.graph import StateGraph
except ImportError:  # pragma: no cover - depends on optional local dependency
    StateGraph = None


class ScenarioGenerationGraphRunnerV2:
    def __init__(self, *, settings: Settings | None = None, fallback_agent: ScenarioGenerationV2Agent | None = None) -> None:
        self.settings = settings or Settings()
        self.fallback_agent = fallback_agent

    def run(self, request):
        if StateGraph is None:
            warnings.warn(
                "LangGraph is not installed; falling back to ScenarioGenerationV2Agent.",
                RuntimeWarning,
                stacklevel=2,
            )
            return (self.fallback_agent or ScenarioGenerationV2Agent(settings=self.settings)).generate(request)
        warnings.warn(
            "ScenarioGenerationGraphRunnerV2 is a skeleton; falling back to ScenarioGenerationV2Agent.",
            RuntimeWarning,
            stacklevel=2,
        )
        return (self.fallback_agent or ScenarioGenerationV2Agent(settings=self.settings)).generate(request)
