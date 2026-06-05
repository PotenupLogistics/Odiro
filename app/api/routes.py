from __future__ import annotations

from fastapi import APIRouter

from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services.scenario_generation_service import generate_scenario_run_queue


router = APIRouter()


@router.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok", "service": "proto-ai", "version": "0.1.0"}


@router.post(
    "/api/v1/scenarios/generate",
    response_model=EpisodeRunQueue,
)
def scenario_generate_endpoint(
    request: ScenarioGenerateRequest,
) -> EpisodeRunQueue:
    return generate_scenario_run_queue(request)
