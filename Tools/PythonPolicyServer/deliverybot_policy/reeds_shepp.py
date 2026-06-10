from __future__ import annotations

from dataclasses import dataclass
import math


SEGMENT_LEFT = "Left"
SEGMENT_RIGHT = "Right"
SEGMENT_STRAIGHT = "Straight"

DIRECTION_FORWARD = "Forward"
DIRECTION_REVERSE = "Reverse"


@dataclass(frozen=True)
class ReedsSheppSegment:
    segment_type: str
    length_cm: float
    direction: str


@dataclass(frozen=True)
class ReedsSheppPose:
    x_cm: float
    y_cm: float
    yaw_radian: float
    direction: str


@dataclass(frozen=True)
class ReedsSheppPath:
    segments: list[ReedsSheppSegment]
    poses: list[ReedsSheppPose]
    length_cm: float
    family: str


def find_reeds_shepp_path(
    start_x_cm: float,
    start_y_cm: float,
    start_yaw_radian: float,
    goal_x_cm: float,
    goal_y_cm: float,
    goal_yaw_radian: float,
    turning_radius_cm: float,
    sample_step_cm: float,
    allow_reverse: bool = True,
) -> ReedsSheppPath | None:
    safe_turning_radius_cm = max(turning_radius_cm, 1.0)
    safe_sample_step_cm = max(sample_step_cm, 1.0)
    candidate_paths: list[ReedsSheppPath] = []

    for direction in build_directions(allow_reverse):
        start_solver_yaw = start_yaw_radian if direction == DIRECTION_FORWARD else start_yaw_radian + math.pi
        goal_solver_yaw = goal_yaw_radian if direction == DIRECTION_FORWARD else goal_yaw_radian + math.pi
        x, y, phi = transform_goal_to_start_frame(
            start_x_cm,
            start_y_cm,
            start_solver_yaw,
            goal_x_cm,
            goal_y_cm,
            goal_solver_yaw,
            safe_turning_radius_cm,
        )
        for family, solver_segments in iter_dubins_csc_segments(x, y, phi):
            segments = build_segments_from_solver(
                solver_segments,
                direction,
                safe_turning_radius_cm,
            )
            path = sample_path(
                start_x_cm,
                start_y_cm,
                start_yaw_radian,
                goal_x_cm,
                goal_y_cm,
                goal_yaw_radian,
                safe_turning_radius_cm,
                safe_sample_step_cm,
                segments,
                f"{direction}:{family}",
            )
            if path is not None:
                candidate_paths.append(path)

    if not candidate_paths:
        return None

    return min(candidate_paths, key=lambda path: path.length_cm)


def build_directions(allow_reverse: bool) -> list[str]:
    directions = [DIRECTION_FORWARD]
    if allow_reverse:
        directions.append(DIRECTION_REVERSE)
    return directions


def transform_goal_to_start_frame(
    start_x_cm: float,
    start_y_cm: float,
    start_yaw_radian: float,
    goal_x_cm: float,
    goal_y_cm: float,
    goal_yaw_radian: float,
    turning_radius_cm: float,
) -> tuple[float, float, float]:
    dx_cm = goal_x_cm - start_x_cm
    dy_cm = goal_y_cm - start_y_cm
    cos_yaw = math.cos(start_yaw_radian)
    sin_yaw = math.sin(start_yaw_radian)
    x = (cos_yaw * dx_cm + sin_yaw * dy_cm) / turning_radius_cm
    y = (-sin_yaw * dx_cm + cos_yaw * dy_cm) / turning_radius_cm
    phi = mod2pi(goal_yaw_radian - start_yaw_radian)
    return x, y, phi


def iter_dubins_csc_segments(x: float, y: float, phi: float) -> list[tuple[str, list[tuple[str, float]]]]:
    d = math.hypot(x, y)
    theta = math.atan2(y, x)
    alpha = mod2pi(-theta)
    beta = mod2pi(phi - theta)
    candidates: list[tuple[str, list[tuple[str, float]]]] = []

    for family, builder in (
        ("LSL", build_lsl),
        ("RSR", build_rsr),
        ("LSR", build_lsr),
        ("RSL", build_rsl),
    ):
        lengths = builder(alpha, beta, d)
        if lengths is None:
            continue

        first, straight, last = lengths
        if first < -1.0e-9 or straight < -1.0e-9 or last < -1.0e-9:
            continue

        segment_types = family_to_segment_types(family)
        candidates.append(
            (
                family,
                [
                    (segment_types[0], first),
                    (segment_types[1], straight),
                    (segment_types[2], last),
                ],
            )
        )

    return candidates


def build_lsl(alpha: float, beta: float, d: float) -> tuple[float, float, float] | None:
    sin_alpha = math.sin(alpha)
    sin_beta = math.sin(beta)
    cos_alpha = math.cos(alpha)
    cos_beta = math.cos(beta)
    cos_alpha_beta = math.cos(alpha - beta)
    p_squared = 2.0 + d * d - 2.0 * cos_alpha_beta + 2.0 * d * (sin_alpha - sin_beta)
    if p_squared < 0.0:
        return None

    tmp = math.atan2(cos_beta - cos_alpha, d + sin_alpha - sin_beta)
    t = mod2pi(-alpha + tmp)
    p = math.sqrt(max(p_squared, 0.0))
    q = mod2pi(beta - tmp)
    return t, p, q


def build_rsr(alpha: float, beta: float, d: float) -> tuple[float, float, float] | None:
    sin_alpha = math.sin(alpha)
    sin_beta = math.sin(beta)
    cos_alpha = math.cos(alpha)
    cos_beta = math.cos(beta)
    cos_alpha_beta = math.cos(alpha - beta)
    p_squared = 2.0 + d * d - 2.0 * cos_alpha_beta + 2.0 * d * (-sin_alpha + sin_beta)
    if p_squared < 0.0:
        return None

    tmp = math.atan2(cos_alpha - cos_beta, d - sin_alpha + sin_beta)
    t = mod2pi(alpha - tmp)
    p = math.sqrt(max(p_squared, 0.0))
    q = mod2pi(-beta + tmp)
    return t, p, q


def build_lsr(alpha: float, beta: float, d: float) -> tuple[float, float, float] | None:
    sin_alpha = math.sin(alpha)
    sin_beta = math.sin(beta)
    cos_alpha = math.cos(alpha)
    cos_beta = math.cos(beta)
    cos_alpha_beta = math.cos(alpha - beta)
    p_squared = -2.0 + d * d + 2.0 * cos_alpha_beta + 2.0 * d * (sin_alpha + sin_beta)
    if p_squared < 0.0:
        return None

    p = math.sqrt(max(p_squared, 0.0))
    tmp = math.atan2(-cos_alpha - cos_beta, d + sin_alpha + sin_beta) - math.atan2(-2.0, p)
    t = mod2pi(-alpha + tmp)
    q = mod2pi(-mod2pi(beta) + tmp)
    return t, p, q


def build_rsl(alpha: float, beta: float, d: float) -> tuple[float, float, float] | None:
    sin_alpha = math.sin(alpha)
    sin_beta = math.sin(beta)
    cos_alpha = math.cos(alpha)
    cos_beta = math.cos(beta)
    cos_alpha_beta = math.cos(alpha - beta)
    p_squared = d * d - 2.0 + 2.0 * cos_alpha_beta - 2.0 * d * (sin_alpha + sin_beta)
    if p_squared < 0.0:
        return None

    p = math.sqrt(max(p_squared, 0.0))
    tmp = math.atan2(cos_alpha + cos_beta, d - sin_alpha - sin_beta) - math.atan2(2.0, p)
    t = mod2pi(alpha - tmp)
    q = mod2pi(beta - tmp)
    return t, p, q


def family_to_segment_types(family: str) -> tuple[str, str, str]:
    return (
        SEGMENT_LEFT if family[0] == "L" else SEGMENT_RIGHT,
        SEGMENT_STRAIGHT,
        SEGMENT_LEFT if family[2] == "L" else SEGMENT_RIGHT,
    )


def build_segments_from_solver(
    solver_segments: list[tuple[str, float]],
    direction: str,
    turning_radius_cm: float,
) -> list[ReedsSheppSegment]:
    segments: list[ReedsSheppSegment] = []
    for segment_type, normalized_length in solver_segments:
        actual_segment_type = segment_type
        if direction == DIRECTION_REVERSE:
            actual_segment_type = reverse_turn_segment_type(segment_type)

        length_cm = normalized_length * turning_radius_cm
        if length_cm <= 1.0e-6:
            continue

        segments.append(ReedsSheppSegment(actual_segment_type, length_cm, direction))

    return segments


def reverse_turn_segment_type(segment_type: str) -> str:
    if segment_type == SEGMENT_LEFT:
        return SEGMENT_RIGHT
    if segment_type == SEGMENT_RIGHT:
        return SEGMENT_LEFT
    return segment_type


def sample_path(
    start_x_cm: float,
    start_y_cm: float,
    start_yaw_radian: float,
    goal_x_cm: float,
    goal_y_cm: float,
    goal_yaw_radian: float,
    turning_radius_cm: float,
    sample_step_cm: float,
    segments: list[ReedsSheppSegment],
    family: str,
) -> ReedsSheppPath | None:
    if not segments:
        return None

    x_cm = start_x_cm
    y_cm = start_y_cm
    yaw_radian = normalize_radian(start_yaw_radian)
    poses = [ReedsSheppPose(x_cm, y_cm, yaw_radian, segments[0].direction)]

    for segment in segments:
        remaining_cm = segment.length_cm
        while remaining_cm > 1.0e-6:
            step_cm = min(sample_step_cm, remaining_cm)
            x_cm, y_cm, yaw_radian = integrate_segment(
                x_cm,
                y_cm,
                yaw_radian,
                segment,
                step_cm,
                turning_radius_cm,
            )
            poses.append(ReedsSheppPose(x_cm, y_cm, yaw_radian, segment.direction))
            remaining_cm -= step_cm

    if not is_endpoint_close(x_cm, y_cm, yaw_radian, goal_x_cm, goal_y_cm, goal_yaw_radian, sample_step_cm):
        return None

    poses[-1] = ReedsSheppPose(
        goal_x_cm,
        goal_y_cm,
        normalize_radian(goal_yaw_radian),
        poses[-1].direction,
    )
    return ReedsSheppPath(segments, poses, sum(segment.length_cm for segment in segments), family)


def integrate_segment(
    x_cm: float,
    y_cm: float,
    yaw_radian: float,
    segment: ReedsSheppSegment,
    step_cm: float,
    turning_radius_cm: float,
) -> tuple[float, float, float]:
    direction_sign = 1.0 if segment.direction == DIRECTION_FORWARD else -1.0
    if segment.segment_type == SEGMENT_STRAIGHT:
        return (
            x_cm + math.cos(yaw_radian) * direction_sign * step_cm,
            y_cm + math.sin(yaw_radian) * direction_sign * step_cm,
            normalize_radian(yaw_radian),
        )

    turn_sign = 1.0 if segment.segment_type == SEGMENT_LEFT else -1.0
    curvature = turn_sign / max(turning_radius_cm, 1.0)
    next_yaw = normalize_radian(yaw_radian + direction_sign * curvature * step_cm)
    next_x = x_cm + (math.sin(next_yaw) - math.sin(yaw_radian)) / curvature
    next_y = y_cm + (-math.cos(next_yaw) + math.cos(yaw_radian)) / curvature
    return next_x, next_y, next_yaw


def is_endpoint_close(
    x_cm: float,
    y_cm: float,
    yaw_radian: float,
    goal_x_cm: float,
    goal_y_cm: float,
    goal_yaw_radian: float,
    sample_step_cm: float,
) -> bool:
    distance_error_cm = math.hypot(goal_x_cm - x_cm, goal_y_cm - y_cm)
    yaw_error = abs(normalize_radian(goal_yaw_radian - yaw_radian))
    return distance_error_cm <= max(sample_step_cm, 1.0) + 1.0e-6 and yaw_error <= math.radians(2.0)


def mod2pi(angle_radian: float) -> float:
    return angle_radian % math.tau


def normalize_radian(angle_radian: float) -> float:
    return (angle_radian + math.pi) % math.tau - math.pi

