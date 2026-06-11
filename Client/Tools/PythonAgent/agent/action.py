from dataclasses import dataclass



# Unreal이 실제로 실행할 이동 명령
@dataclass
class BotAction:
    steering: float             # 조향 값. -1.0 ~ 1.0
    targetSpeedKmh: float       # 목표 속도. km/h
    brake: float                # 브레이크 값. 0.0 ~ 1.0
    direction: str = "Forward"  # 이동 방향. Forward 또는 Reverse

    # JSON 응답에 넣기 위해 dict로 변환
    def to_dict(self) -> dict:
        return {
            "steering": self.steering,
            "targetSpeedKmh": self.targetSpeedKmh,
            "brake": self.brake,
            "direction": self.direction,
        }


# 숫자 범위를 안전하게 제한
def clamp(value: float, min_value: float, max_value: float) -> float:
    return max(min_value, min(max_value, value))



# 즉시 정지 Action 생성
def stop_action() -> BotAction:
    return BotAction(
        steering=0.0,
        targetSpeedKmh=0.0,
        brake=1.0,
        direction="Forward",
    )


# 전진 주행 Action 생성
def drive_action(steering: float, speed_kmh: float) -> BotAction:
    return BotAction(
        steering=clamp(steering, -1.0, 1.0),
        targetSpeedKmh=max(0.0, speed_kmh),
        brake=0.0,
        direction="Forward",
    )
    
    
# 후진 주행 Action 생성
def reverse_action(steering: float, speed_kmh: float) -> BotAction:
    return BotAction(
        steering=clamp(steering, -1.0, 1.0),
        targetSpeedKmh=max(0.0, speed_kmh),
        brake=0.0,
        direction="Reverse",
    )
    
    
    
# 실패 시 안전하게 멈추기 위한 Action 생성
def fail_safe_action() -> BotAction:
    return stop_action()
    