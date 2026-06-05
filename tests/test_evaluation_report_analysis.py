from __future__ import annotations

import json
from pathlib import Path

import pytest

from app.models.bot_setup import BotLidarSetup, DeliveryBotSetup
from app.models.evaluation_report import EvaluationReport
from app.models.recommendation import BotSetupRecommendation, EpisodeRecommendationResult
from app.services.bot_setup_generator import generate_bot_setup
from app.services.evaluation_report_parser import (
    EvaluationReportParseError,
    parse_evaluation_report,
)
from app.services.evaluation_report_statistics_extractor import extract_statistics
from app.services.policy_fallback_rules import (
    apply_episode_fallback_rules,
    build_episode_fallback_summary,
)
from app.services.policy_recommendation_orchestrator import analyze_episode_and_recommend


FIXTURES = Path(__file__).parent / "fixtures"
SAMPLE_REPORT = FIXTURES / "sample_evaluation_report.json"


# ── parser ──────────────────────────────────────────────────────────────────


def test_parse_sample_report_success() -> None:
    report = parse_evaluation_report(SAMPLE_REPORT)
    assert isinstance(report, EvaluationReport)
    assert report.summary.outcome == "Failure"
    assert report.summary.terminal_reason == "DeliveryBotSimulationFailed"
    assert report.summary.usable_for_llm_tuning is True
    assert len(report.events) == 3


def test_parse_report_missing_file() -> None:
    with pytest.raises(EvaluationReportParseError):
        parse_evaluation_report("nonexistent.json")


def test_parse_report_empty_file(tmp_path) -> None:
    p = tmp_path / "empty.json"
    p.write_text("", encoding="utf-8")
    with pytest.raises(EvaluationReportParseError):
        parse_evaluation_report(p)


def test_parse_report_invalid_json(tmp_path) -> None:
    p = tmp_path / "bad.json"
    p.write_text("not json", encoding="utf-8")
    with pytest.raises(EvaluationReportParseError):
        parse_evaluation_report(p)


def test_parse_report_usable_false(tmp_path) -> None:
    data = json.loads(SAMPLE_REPORT.read_text(encoding="utf-8"))
    data["summary"]["usable_for_llm_tuning"] = False
    p = tmp_path / "unusable.json"
    p.write_text(json.dumps(data), encoding="utf-8")
    report = parse_evaluation_report(p)
    assert report.summary.usable_for_llm_tuning is False


# ── statistics extractor ────────────────────────────────────────────────────


def test_extract_statistics_from_sample() -> None:
    report = parse_evaluation_report(SAMPLE_REPORT)
    stats = extract_statistics(report)

    assert stats.outcome == "Failure"
    assert stats.terminal_reason == "DeliveryBotSimulationFailed"
    assert stats.duration_s == pytest.approx(38.6)
    assert stats.score == pytest.approx(-7.0)
    assert stats.goal_reached is False
    assert stats.static_obstacle_collision_count == 1
    assert stats.pedestrian_collision_count == 0
    assert stats.near_miss_count == 1
    assert stats.near_miss_min_distance_m == pytest.approx(0.42)
    assert stats.failure_type == "Stuck"
    assert stats.usable_for_llm_tuning is True


def test_extract_statistics_event_counts() -> None:
    report = parse_evaluation_report(SAMPLE_REPORT)
    stats = extract_statistics(report)
    assert stats.event_counts_by_type.get("PedestrianNearMiss") == 1
    assert stats.event_counts_by_type.get("StaticObstacleCollision") == 1


# ── fallback rules ──────────────────────────────────────────────────────────


def _sample_stats(**overrides):
    report = parse_evaluation_report(SAMPLE_REPORT)
    stats = extract_statistics(report)
    data = stats.model_dump()
    data.update(overrides)
    from app.models.recommendation import EvaluationReportStatistics
    return EvaluationReportStatistics(**data)


def test_fallback_near_miss_triggers_stop_distance() -> None:
    stats = _sample_stats(near_miss_min_distance_m=0.3)
    recs = apply_episode_fallback_rules(stats, current_stop_distance_m=1.2)
    params = {r.param for r in recs}
    assert "stop_distance_m" in params


def test_fallback_pedestrian_collision_triggers_stop_distance() -> None:
    stats = _sample_stats(pedestrian_collision_count=1)
    recs = apply_episode_fallback_rules(stats)
    params = {r.param for r in recs}
    assert "stop_distance_m" in params


def test_fallback_static_collision_triggers_slow_down() -> None:
    stats = _sample_stats(static_obstacle_collision_count=1)
    recs = apply_episode_fallback_rules(stats)
    params = {r.param for r in recs}
    assert "slow_down_distance_m" in params


def test_fallback_stuck_triggers_obstacle_slow_speed() -> None:
    stats = _sample_stats(terminal_reason="Stuck", failure_type="Stuck")
    recs = apply_episode_fallback_rules(stats)
    params = {r.param for r in recs}
    assert "obstacle_slow_speed_kmh" in params


def test_fallback_timeout_triggers_target_speed() -> None:
    stats = _sample_stats(terminal_reason="Timeout", failure_type=None)
    recs = apply_episode_fallback_rules(stats)
    params = {r.param for r in recs}
    assert "target_speed_kmh" in params


def test_fallback_tip_over_triggers_max_speed() -> None:
    stats = _sample_stats(robot_tip_over_count=1, terminal_reason="RobotTipOver")
    recs = apply_episode_fallback_rules(stats)
    params = {r.param for r in recs}
    assert "max_speed_kmh" in params


def test_fallback_summary_empty() -> None:
    summary = build_episode_fallback_summary([])
    assert "없음" in summary


def test_fallback_summary_nonempty() -> None:
    recs = [
        BotSetupRecommendation(param="stop_distance_m", current=1.2, suggested=1.5, reason="test"),
    ]
    summary = build_episode_fallback_summary(recs)
    assert "stop_distance_m" in summary


# ── bot_setup_generator ──────────────────────────────────────────────────────


def test_generate_bot_setup_applies_recommendations() -> None:
    current = DeliveryBotSetup()
    recs = [
        BotSetupRecommendation(param="stop_distance_m", current=1.2, suggested=1.5, reason="test"),
        BotSetupRecommendation(param="max_speed_kmh", current=10.0, suggested=8.0, reason="test"),
    ]
    next_setup = generate_bot_setup(current, recs)
    assert next_setup.lidar.stop_distance_m == pytest.approx(1.5)
    assert next_setup.drive.max_speed_kmh == pytest.approx(8.0)


def test_generate_bot_setup_enforces_slow_down_constraint() -> None:
    current = DeliveryBotSetup()
    recs = [
        BotSetupRecommendation(param="stop_distance_m", current=1.2, suggested=3.4, reason="test"),
    ]
    next_setup = generate_bot_setup(current, recs)
    # slow_down_distance_m은 stop_distance_m + 0.1 이상 보장
    assert next_setup.lidar.slow_down_distance_m >= next_setup.lidar.stop_distance_m + 0.09


def test_generate_bot_setup_json_dict_format() -> None:
    current = DeliveryBotSetup()
    next_setup = generate_bot_setup(current, [])
    d = next_setup.to_json_dict()
    assert d["schema"] == "delivery_bot_setup"
    assert "robot" in d
    assert "lidar" in d["robot"]
    assert "drive" in d["robot"]
    assert "path_follow" in d["robot"]


# ── orchestrator E2E ─────────────────────────────────────────────────────────


def test_orchestrator_episode_fallback_only() -> None:
    result = analyze_episode_and_recommend(
        report_path=SAMPLE_REPORT,
        fallback_only=True,
    )
    assert isinstance(result, EpisodeRecommendationResult)
    assert result.generationMethod == "fallback_rules"
    assert result.statistics.outcome == "Failure"
    assert result.nextBotSetup is not None
    assert result.nextBotSetup["schema"] == "delivery_bot_setup"


def test_orchestrator_episode_unusable_report(tmp_path) -> None:
    data = json.loads(SAMPLE_REPORT.read_text(encoding="utf-8"))
    data["summary"]["usable_for_llm_tuning"] = False
    p = tmp_path / "unusable.json"
    p.write_text(json.dumps(data), encoding="utf-8")

    result = analyze_episode_and_recommend(report_path=p, fallback_only=True)
    assert result.recommendations == []
    assert result.nextBotSetup is None
    assert any("usable_for_llm_tuning" in w for w in result.llmWarnings)


def test_orchestrator_episode_result_json_serializable() -> None:
    result = analyze_episode_and_recommend(
        report_path=SAMPLE_REPORT,
        fallback_only=True,
    )
    payload = result.model_dump(mode="json")
    text = json.dumps(payload, ensure_ascii=False)
    reloaded = json.loads(text)
    assert reloaded["analysisId"] == result.analysisId
    assert reloaded["generationMethod"] == "fallback_rules"
    assert "nextBotSetup" in reloaded
