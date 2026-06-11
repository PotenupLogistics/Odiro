from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from app.models.run_result import RunMetrics


# ── 기존 MeasurementLog 기반 모델 (하위호환 유지) ──────────────────────────

PolicyParamName = Literal[
    "stopDistanceM",
    "slowDownDistanceM",
    "maxSpeedKmh",
    "canRepath",
]


class AnalysisStatistics(BaseModel):
    model_config = ConfigDict(extra="forbid")

    totalTicks: int = Field(ge=0)
    deliveryTimeSec: float = Field(ge=0)
    closeReason: str
    diagnosticsWarningCount: int = Field(ge=0)

    avgFrontDistanceM: float = Field(ge=0)
    minFrontDistanceM: float = Field(ge=0)
    medianFrontDistanceM: float = Field(ge=0)
    frontObjectDetectionRate: float = Field(ge=0, le=1)

    actionReasonFreq: dict[str, int]
    brakeAppliedCount: int = Field(ge=0)
    brakeAppliedRatio: float = Field(ge=0, le=1)

    avgTargetSpeedKmh: float = Field(ge=0)
    targetSpeedVariance: float = Field(ge=0)
    avgActualSpeedKmh: float = Field(ge=0)

    stopActionRatio: float = Field(ge=0, le=1)
    slowdownActionRatio: float = Field(ge=0, le=1)
    repathActionRatio: float = Field(ge=0, le=1)


class ParamRecommendation(BaseModel):
    model_config = ConfigDict(extra="forbid")

    param: PolicyParamName
    current: float | bool
    suggested: float | bool
    reason: str
    citations: list[str] = Field(default_factory=list)


GenerationMethod = Literal["llm_rag", "fallback_rules", "fallback_after_llm_failure"]


class PolicyRecommendationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str = "1.0.0"
    analysisId: str
    logPath: str
    generatedAt: str

    runMetrics: RunMetrics
    statistics: AnalysisStatistics
    recommendations: list[ParamRecommendation]
    summary: str

    generationMethod: GenerationMethod
    llmWarnings: list[str] = Field(default_factory=list)


# ── EpisodeEvaluationReport 기반 신규 모델 ────────────────────────────────

BotSetupParamName = Literal[
    "stop_distance_m",
    "slow_down_distance_m",
    "max_speed_kmh",
    "target_speed_kmh",
    "obstacle_slow_speed_kmh",
    "look_ahead_distance_m",
    "scan_range_m",
    "angle_step_degree",
]


class EvaluationReportStatistics(BaseModel):
    """EpisodeEvaluationReport에서 추출한 추천 엔진 입력용 통계."""

    outcome: str
    terminal_reason: str
    duration_s: float = Field(ge=0)
    score: float
    goal_reached: bool

    static_obstacle_collision_count: int = Field(ge=0, default=0)
    blocked_region_collision_count: int = Field(ge=0, default=0)
    pedestrian_collision_count: int = Field(ge=0, default=0)
    near_miss_count: int = Field(ge=0, default=0)
    near_miss_min_distance_m: float | None = None
    robot_tip_over_count: int = Field(ge=0, default=0)

    failure_type: str | None = None
    failure_speed_kmh: float | None = None

    event_counts_by_type: dict[str, int] = Field(default_factory=dict)
    diagnostics: list[str] = Field(default_factory=list)

    usable_for_llm_tuning: bool = True


class BotSetupRecommendation(BaseModel):
    param: BotSetupParamName
    current: float
    suggested: float
    reason: str
    citations: list[str] = Field(default_factory=list)


class EpisodeRecommendationResult(BaseModel):
    schemaVersion: str = "1.0.0"
    analysisId: str
    reportPath: str
    generatedAt: str

    statistics: EvaluationReportStatistics
    recommendations: list[BotSetupRecommendation]
    summary: str

    generationMethod: GenerationMethod
    llmWarnings: list[str] = Field(default_factory=list)

    nextBotSetup: dict[str, Any] | None = None


# ── 통합 (5-입력) 추천 모델 ────────────────────────────────────────────────

EpisodeSetupParamName = Literal[
    "run.time_limit_s",
    "evaluation.goal_acceptance_radius_m",
    "evaluation.near_miss.distance_m",
    "evaluation.tip_over_angle_deg",
    "actors.pedestrians[*].movement.speed_mps",
    "actors.pedestrians[*].spawn_time_s",
    "actors.static_obstacles[*].xy_m",
]


PolicyServerParamName = Literal[
    "stop_distance_m_threshold",
    "slow_down_distance_m_threshold",
    "repath_grace_time_s",
    "force_action_override",
]


class EpisodeSetupRecommendation(BaseModel):
    param: EpisodeSetupParamName
    current: float | str | None = None
    suggested: float | str | None = None
    reason: str
    citations: list[str] = Field(default_factory=list)


class PolicyServerRecommendation(BaseModel):
    param: PolicyServerParamName
    current: float | str | None = None
    suggested: float | str | None = None
    reason: str
    citations: list[str] = Field(default_factory=list)


class IntegratedRecommendationResult(BaseModel):
    schemaVersion: str = "1.0.0"
    analysisId: str
    inputs: dict[str, str]
    generatedAt: str

    episodeStatistics: EvaluationReportStatistics
    measurementStatistics: AnalysisStatistics
    setupSnapshot: dict[str, Any]

    botSetupRecommendations: list[BotSetupRecommendation] = Field(default_factory=list)
    episodeSetupRecommendations: list[EpisodeSetupRecommendation] = Field(default_factory=list)
    policyServerRecommendations: list[PolicyServerRecommendation] = Field(default_factory=list)

    summary: str
    generationMethod: GenerationMethod
    llmWarnings: list[str] = Field(default_factory=list)

    nextBotSetup: dict[str, Any] | None = None
    nextEpisodeSetup: dict[str, Any] | None = None

    # policy_server.py 정적 분석 결과 (FORCED_ACTION 감지 등)
    policySourceAnalysis: dict[str, Any] | None = None
    # BotSetup ↔ PolicyServer 거리값 정합성 검사 결과
    paramConsistencyCheck: dict[str, Any] | None = None


class EpisodeAnalysisRequest(BaseModel):
    """언리얼 → 분석 API 요청. 서버 로컬 파일 경로 4개를 받는다.

    policy_server.py는 서버 내부 코드이므로 요청으로 받지 않고
    서버가 고정 기본경로를 사용한다.
    """

    model_config = ConfigDict(extra="forbid")

    evaluation_report_path: str = Field(min_length=1)
    measurement_log_path: str = Field(min_length=1)
    episode_setup_path: str = Field(min_length=1)
    bot_setup_path: str = Field(min_length=1)
    fallback_only: bool = False
