"""Internal route labels shared by the result-analysis LangGraph and fallback runner."""

from __future__ import annotations

from typing import Literal


# Analysis route labels decide whether deeper recommendation work is needed.
AnalysisRouteV2 = Literal["insufficient_data", "no_change_needed", "patterns_found"]

# RAG route labels summarize internal retrieval status without exposing evidence.
RagRouteV2 = Literal["skipped", "retrieved", "no_query", "no_result", "store_missing", "jsonl_error", "search_error"]

# Recommendation route labels keep LLM and rule-based outcomes comparable.
RecommendationRouteV2 = Literal["none", "llm_valid", "rule_based_fallback"]

# Validation route labels drive the final conditional edge.
RecommendationValidationRouteV2 = Literal["valid", "fallback"]

ANALYSIS_ROUTE_INSUFFICIENT_DATA: AnalysisRouteV2 = "insufficient_data"
ANALYSIS_ROUTE_NO_CHANGE_NEEDED: AnalysisRouteV2 = "no_change_needed"
ANALYSIS_ROUTE_PATTERNS_FOUND: AnalysisRouteV2 = "patterns_found"

RAG_ROUTE_SKIPPED: RagRouteV2 = "skipped"
RAG_ROUTE_RETRIEVED: RagRouteV2 = "retrieved"
RAG_ROUTE_NO_QUERY: RagRouteV2 = "no_query"
RAG_ROUTE_NO_RESULT: RagRouteV2 = "no_result"
RAG_ROUTE_STORE_MISSING: RagRouteV2 = "store_missing"
RAG_ROUTE_JSONL_ERROR: RagRouteV2 = "jsonl_error"
RAG_ROUTE_SEARCH_ERROR: RagRouteV2 = "search_error"

RECOMMENDATION_ROUTE_NONE: RecommendationRouteV2 = "none"
RECOMMENDATION_ROUTE_LLM_VALID: RecommendationRouteV2 = "llm_valid"
RECOMMENDATION_ROUTE_RULE_BASED_FALLBACK: RecommendationRouteV2 = "rule_based_fallback"

RECOMMENDATION_VALIDATION_VALID: RecommendationValidationRouteV2 = "valid"
RECOMMENDATION_VALIDATION_FALLBACK: RecommendationValidationRouteV2 = "fallback"


def decide_analysis_route(*, episode_count: int, failure_pattern_count: int) -> AnalysisRouteV2:
    """Return the high-level analysis route from parsed episode and pattern counts."""
    if episode_count <= 0:
        return ANALYSIS_ROUTE_INSUFFICIENT_DATA
    if failure_pattern_count > 0:
        return ANALYSIS_ROUTE_PATTERNS_FOUND
    return ANALYSIS_ROUTE_NO_CHANGE_NEEDED


def default_rag_route_for_analysis(analysis_route: AnalysisRouteV2) -> RagRouteV2:
    """Return the default RAG route for analysis paths that do not retrieve context."""
    _ = analysis_route
    return RAG_ROUTE_SKIPPED


def default_recommendation_route_for_analysis(analysis_route: AnalysisRouteV2) -> RecommendationRouteV2:
    """Return the default recommendation route for analysis paths that do not generate recommendations."""
    _ = analysis_route
    return RECOMMENDATION_ROUTE_NONE


def route_for_valid_recommendations(*, analysis_mode: str) -> RecommendationRouteV2:
    """Return the recommendation route for a validation pass."""
    if analysis_mode == "llm_candidate":
        return RECOMMENDATION_ROUTE_LLM_VALID
    return RECOMMENDATION_ROUTE_RULE_BASED_FALLBACK
