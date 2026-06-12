from dataclasses import dataclass


@dataclass
class BotAction:
    steering: float
    targetSpeedKmh: float
    brake: float
    direction: str = "Forward"

    def to_dict(self) -> dict:
        return {
            "steering": self.steering,
            "targetSpeedKmh": self.targetSpeedKmh,
            "brake": self.brake,
            "direction": self.direction,
        }


def clamp(value: float, min_value: float, max_value: float) -> float:
    return max(min_value, min(max_value, value))


def stop_action() -> BotAction:
    return BotAction(
        steering=0.0,
        targetSpeedKmh=0.0,
        brake=1.0,
        direction="Forward",
    )


def soft_stop_action(brake: float = 0.2, steering: float = 0.0) -> BotAction:
    return BotAction(
        steering=clamp(steering, -1.0, 1.0),
        targetSpeedKmh=0.0,
        brake=clamp(brake, 0.0, 1.0),
        direction="Forward",
    )


def drive_action(steering: float, speed_kmh: float) -> BotAction:
    return BotAction(
        steering=clamp(steering, -1.0, 1.0),
        targetSpeedKmh=max(0.0, speed_kmh),
        brake=0.0,
        direction="Forward",
    )


def reverse_action(steering: float, speed_kmh: float) -> BotAction:
    return BotAction(
        steering=clamp(steering, -1.0, 1.0),
        targetSpeedKmh=max(0.0, speed_kmh),
        brake=0.0,
        direction="Reverse",
    )


def fail_safe_action() -> BotAction:
    return stop_action()
