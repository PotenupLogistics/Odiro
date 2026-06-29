from __future__ import annotations

from typing import Any

from fastapi import FastAPI, Request
from fastapi.exception_handlers import request_validation_exception_handler
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from app.api.routes import router
from app.models.analysis_v2 import AnalysisRunV2Response


app = FastAPI(
    title="Proto-AI Delivery Robot Policy Backend",
    version="0.1.0",
)
app.include_router(router)


@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    """Return the v2 analysis failed body only for its public endpoint."""
    if request.url.path != "/api/v2/analysis/run":
        return await request_validation_exception_handler(request, exc)
    response = AnalysisRunV2Response(
        status="failed",
        run_id=await _safe_run_id_from_request(request),
        error={
            "code": "INVALID_ANALYSIS_REQUEST",
            "message": "분석 요청 형식이 올바르지 않습니다.",
            "phase": "request_validation",
        },
        warnings=[],
    )
    return JSONResponse(
        status_code=400,
        content=response.model_dump(by_alias=True, exclude_none=True),
    )


async def _safe_run_id_from_request(request: Request) -> str | None:
    """Extract run_id from a malformed request only when the JSON body is safe to inspect."""
    try:
        body: Any = await request.json()
    except Exception:
        return None
    if not isinstance(body, dict):
        return None
    run_id = body.get("run_id")
    return run_id if isinstance(run_id, str) and run_id else None
