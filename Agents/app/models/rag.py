from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field


class RagSearchQuery(BaseModel):
    model_config = ConfigDict(extra="forbid")

    query: str = ""
    topK: int = Field(default=5, ge=1)
    categoryFilter: list[str] | None = None
    actionFilter: list[str] | None = None
    policyParamFilter: list[str] | None = None
    sourceFilter: list[str] | None = None


class RagChunkMetadata(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sourceIds: list[str]
    category: str
    relatedPolicyParams: list[str]
    relatedRequestFields: list[str]
    relatedActions: list[str]
    relatedMetrics: list[str]
    evidenceLocation: str
    createdFromCandidateId: str
    status: str


class RagRetrievedChunk(BaseModel):
    model_config = ConfigDict(extra="forbid")

    chunkId: str
    cardId: str
    chunkText: str
    metadata: RagChunkMetadata
    score: float
    matchedFields: list[str]


class RagSearchResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    query: RagSearchQuery
    resultCount: int
    results: list[RagRetrievedChunk]
