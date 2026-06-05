from __future__ import annotations

from app.models.bot_setup import BotDriveSetup, BotLidarSetup, BotPathFollowSetup, DeliveryBotSetup
from app.models.recommendation import BotSetupRecommendation


def generate_bot_setup(
    current: DeliveryBotSetup,
    recommendations: list[BotSetupRecommendation],
) -> DeliveryBotSetup:
    """추천값을 현재 setup에 적용해 다음 실험용 DeliveryBotSetup 반환."""
    drive_data = current.drive.model_dump()
    path_follow_data = current.path_follow.model_dump()
    lidar_data = current.lidar.model_dump()

    lidar_fields = {
        "stop_distance_m", "slow_down_distance_m",
        "scan_range_m", "angle_step_degree",
    }
    drive_fields = {"max_speed_kmh"}
    path_follow_fields = {
        "target_speed_kmh", "obstacle_slow_speed_kmh", "look_ahead_distance_m",
    }

    for rec in recommendations:
        if rec.param in lidar_fields:
            lidar_data[rec.param] = rec.suggested
        elif rec.param in drive_fields:
            drive_data[rec.param] = rec.suggested
        elif rec.param in path_follow_fields:
            path_follow_data[rec.param] = rec.suggested

    return DeliveryBotSetup(
        drive=BotDriveSetup(**drive_data),
        path_follow=BotPathFollowSetup(**path_follow_data),
        lidar=BotLidarSetup(**lidar_data),
    )
