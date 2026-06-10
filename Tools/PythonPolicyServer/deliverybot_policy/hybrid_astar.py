from __future__ import annotations

from dataclasses import dataclass
import heapq
import math
from typing import Any

from deliverybot_policy.context import get_float_field
from deliverybot_policy.pathfinding import (
    GridIndex,
    get_cell_travel_cost,
    grid_index_to_world_location,
    is_blocked_cell,
    world_to_grid_index,
)
from deliverybot_policy.reeds_shepp import ReedsSheppPath, find_reeds_shepp_path


SearchKey = tuple[int, int, int, str, int]


@dataclass(frozen=True)
class HybridAStarOptions:
    step_distance_cm: float = 75.0
    min_turning_radius_cm: float = 300.0
    heading_bin_count: int = 72
    max_expanded_nodes: int = 15_000
    goal_acceptance_distance_cm: float = 150.0
    goal_acceptance_angle_degree: float = 180.0
    reverse_cost_multiplier: float = 2.2
    gear_switch_cost_penalty: float = 500.0
    max_continuous_reverse_distance_cm: float = 300.0
    reverse_step_distance_scale: float = 0.6
    turn_cost_penalty: float = 15.0
    reverse_turn_cost_penalty: float = 25.0
    turn_switch_cost_penalty: float = 40.0
    clearance_radius_cm: float = 55.0
    collision_sample_step_cm: float = 25.0
    footprint_check_enabled: bool = False
    footprint_half_length_cm: float = 0.0
    footprint_half_width_cm: float = 0.0
    footprint_padding_cm: float = 0.0
    footprint_sample_step_cm: float = 35.0
    analytic_expansion_enabled: bool = True
    analytic_expansion_max_distance_cm: float = 700.0
    analytic_expansion_interval: int = 1
    analytic_expansion_sample_step_cm: float = 25.0
    analytic_expansion_max_length_multiplier: float = 4.0
    allow_reverse: bool = True
    allow_straight: bool = True
    post_process_enabled: bool = True
    shortcut_enabled: bool = True
    resample_enabled: bool = True
    resample_distance_cm: float = 50.0
    dedupe_distance_cm: float = 2.0


@dataclass(frozen=True)
class HybridPose:
    x_cm: float
    y_cm: float
    yaw_degree: float
    direction: str = "Forward"


@dataclass(frozen=True)
class HybridAStarResult:
    poses: list[HybridPose]
    grid_path: list[GridIndex]
    status: str
    expanded_nodes: int = 0
    path_cost: float = 0.0
    raw_pose_count: int = 0
    post_processed: bool = False


@dataclass
class _SearchNode:
    x_cm: float
    y_cm: float
    yaw_radian: float
    direction: str
    turn_sign: float
    reverse_distance_cm: float
    g_score: float
    parent_key: SearchKey | None


def build_hybrid_astar_options(source: dict[str, Any] | None = None) -> HybridAStarOptions:
    safe_source = source if isinstance(source, dict) else {}

    return HybridAStarOptions(
        step_distance_cm=max(get_float_setting(safe_source, ("stepDistanceCm", "step_distance_cm"), 75.0), 10.0),
        min_turning_radius_cm=max(
            get_float_setting(safe_source, ("minTurningRadiusCm", "min_turning_radius_cm"), 300.0),
            50.0,
        ),
        heading_bin_count=max(get_int_setting(safe_source, ("headingBinCount", "heading_bin_count"), 72), 8),
        max_expanded_nodes=max(get_int_setting(safe_source, ("maxExpandedNodes", "max_expanded_nodes"), 15_000), 1),
        goal_acceptance_distance_cm=max(
            get_float_setting(
                safe_source,
                ("goalAcceptanceDistanceCm", "goal_acceptance_distance_cm"),
                150.0,
            ),
            1.0,
        ),
        goal_acceptance_angle_degree=max(
            get_float_setting(
                safe_source,
                ("goalAcceptanceAngleDegree", "goal_acceptance_angle_degree"),
                180.0,
            ),
            0.0,
        ),
        reverse_cost_multiplier=max(
            get_float_setting(safe_source, ("reverseCostMultiplier", "reverse_cost_multiplier"), 2.2),
            1.0,
        ),
        gear_switch_cost_penalty=max(
            get_float_setting(safe_source, ("gearSwitchCostPenalty", "gear_switch_cost_penalty"), 500.0),
            0.0,
        ),
        max_continuous_reverse_distance_cm=max(
            get_float_setting(
                safe_source,
                ("maxContinuousReverseDistanceCm", "max_continuous_reverse_distance_cm"),
                300.0,
            ),
            0.0,
        ),
        reverse_step_distance_scale=max(
            get_float_setting(safe_source, ("reverseStepDistanceScale", "reverse_step_distance_scale"), 0.6),
            0.1,
        ),
        turn_cost_penalty=max(
            get_float_setting(safe_source, ("turnCostPenalty", "turn_cost_penalty"), 15.0),
            0.0,
        ),
        reverse_turn_cost_penalty=max(
            get_float_setting(safe_source, ("reverseTurnCostPenalty", "reverse_turn_cost_penalty"), 25.0),
            0.0,
        ),
        turn_switch_cost_penalty=max(
            get_float_setting(safe_source, ("turnSwitchCostPenalty", "turn_switch_cost_penalty"), 40.0),
            0.0,
        ),
        clearance_radius_cm=max(
            get_float_setting(safe_source, ("clearanceRadiusCm", "clearance_radius_cm"), 55.0),
            0.0,
        ),
        collision_sample_step_cm=max(
            get_float_setting(safe_source, ("collisionSampleStepCm", "collision_sample_step_cm"), 25.0),
            5.0,
        ),
        footprint_check_enabled=get_bool_setting(
            safe_source,
            ("footprintCheckEnabled", "footprint_check_enabled"),
            False,
        ),
        footprint_half_length_cm=max(
            get_float_setting(safe_source, ("footprintHalfLengthCm", "footprint_half_length_cm"), 0.0),
            0.0,
        ),
        footprint_half_width_cm=max(
            get_float_setting(safe_source, ("footprintHalfWidthCm", "footprint_half_width_cm"), 0.0),
            0.0,
        ),
        footprint_padding_cm=max(
            get_float_setting(safe_source, ("footprintPaddingCm", "footprint_padding_cm"), 0.0),
            0.0,
        ),
        footprint_sample_step_cm=max(
            get_float_setting(safe_source, ("footprintSampleStepCm", "footprint_sample_step_cm"), 35.0),
            5.0,
        ),
        analytic_expansion_enabled=get_bool_setting(
            safe_source,
            (
                "analyticExpansionEnabled",
                "analytic_expansion_enabled",
                "reedsSheppEnabled",
                "reeds_shepp_enabled",
            ),
            True,
        ),
        analytic_expansion_max_distance_cm=max(
            get_float_setting(
                safe_source,
                (
                    "analyticExpansionMaxDistanceCm",
                    "analytic_expansion_max_distance_cm",
                    "reedsSheppMaxDistanceCm",
                    "reeds_shepp_max_distance_cm",
                ),
                700.0,
            ),
            0.0,
        ),
        analytic_expansion_interval=max(
            get_int_setting(
                safe_source,
                (
                    "analyticExpansionInterval",
                    "analytic_expansion_interval",
                    "reedsSheppInterval",
                    "reeds_shepp_interval",
                ),
                1,
            ),
            1,
        ),
        analytic_expansion_sample_step_cm=max(
            get_float_setting(
                safe_source,
                (
                    "analyticExpansionSampleStepCm",
                    "analytic_expansion_sample_step_cm",
                    "reedsSheppSampleStepCm",
                    "reeds_shepp_sample_step_cm",
                ),
                25.0,
            ),
            5.0,
        ),
        analytic_expansion_max_length_multiplier=max(
            get_float_setting(
                safe_source,
                (
                    "analyticExpansionMaxLengthMultiplier",
                    "analytic_expansion_max_length_multiplier",
                    "reedsSheppMaxLengthMultiplier",
                    "reeds_shepp_max_length_multiplier",
                ),
                4.0,
            ),
            1.0,
        ),
        allow_reverse=get_bool_setting(safe_source, ("allowReverse", "allow_reverse"), True),
        allow_straight=get_bool_setting(safe_source, ("allowStraight", "allow_straight"), True),
        post_process_enabled=get_bool_setting(
            safe_source,
            ("postProcessEnabled", "post_process_enabled"),
            True,
        ),
        shortcut_enabled=get_bool_setting(
            safe_source,
            ("shortcutEnabled", "shortcut_enabled"),
            True,
        ),
        resample_enabled=get_bool_setting(
            safe_source,
            ("resampleEnabled", "resample_enabled"),
            True,
        ),
        resample_distance_cm=max(
            get_float_setting(safe_source, ("resampleDistanceCm", "resample_distance_cm"), 50.0),
            5.0,
        ),
        dedupe_distance_cm=max(
            get_float_setting(safe_source, ("dedupeDistanceCm", "dedupe_distance_cm"), 2.0),
            0.0,
        ),
    )


def find_hybrid_astar_path(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start_pose: dict[str, Any],
    goal_pose: dict[str, Any],
    options: HybridAStarOptions | dict[str, Any] | None = None,
) -> HybridAStarResult:
    safe_options = options if isinstance(options, HybridAStarOptions) else build_hybrid_astar_options(options)
    start_index = world_to_grid_index(
        grid_info,
        get_float_field(start_pose, "x"),
        get_float_field(start_pose, "y"),
    )
    goal_index = world_to_grid_index(
        grid_info,
        get_float_field(goal_pose, "x"),
        get_float_field(goal_pose, "y"),
    )

    if start_index is None or goal_index is None:
        return HybridAStarResult([], [], "outside_grid")

    if is_blocked_cell(cell_lookup.get(start_index)) or is_blocked_cell(cell_lookup.get(goal_index)):
        return HybridAStarResult([], [], "start_or_goal_blocked")

    start_yaw_radian = math.radians(get_float_field(start_pose, "yawDegree", 0.0))
    start_heading_index = get_heading_index(start_yaw_radian, safe_options)
    start_key = (start_index[0], start_index[1], start_heading_index, "Forward", 0)
    start_node = _SearchNode(
        get_float_field(start_pose, "x"),
        get_float_field(start_pose, "y"),
        normalize_radian(start_yaw_radian),
        "Forward",
        0.0,
        0.0,
        0.0,
        None,
    )

    open_heap: list[tuple[float, int, SearchKey]] = []
    nodes: dict[SearchKey, _SearchNode] = {start_key: start_node}
    g_scores: dict[SearchKey, float] = {start_key: 0.0}
    closed: set[SearchKey] = set()
    heap_counter = 0
    heapq.heappush(
        open_heap,
        (
            estimate_cost_to_goal(start_node.x_cm, start_node.y_cm, goal_pose),
            heap_counter,
            start_key,
        ),
    )
    expanded_nodes = 0
    protected_indexes = {start_index, goal_index}

    while open_heap:
        _, _, current_key = heapq.heappop(open_heap)
        if current_key in closed:
            continue

        current_node = nodes[current_key]
        current_index = (current_key[0], current_key[1])
        if is_goal_reached(current_node, goal_pose, safe_options):
            raw_poses = reconstruct_pose_path(nodes, current_key)
            poses = post_process_hybrid_poses(
                grid_info,
                cell_lookup,
                raw_poses,
                safe_options,
                protected_indexes,
            )
            return HybridAStarResult(
                poses,
                [world_to_grid_index_or_current(grid_info, pose, current_index) for pose in poses],
                "ok",
                expanded_nodes,
                current_node.g_score,
                len(raw_poses),
                not are_pose_paths_equivalent(raw_poses, poses),
            )

        analytic_path = try_reeds_shepp_analytic_expansion(
            grid_info,
            cell_lookup,
            current_node,
            goal_pose,
            safe_options,
            protected_indexes,
            expanded_nodes,
        )
        if analytic_path is not None:
            raw_poses = reconstruct_pose_path(nodes, current_key)
            analytic_poses = build_hybrid_poses_from_reeds_shepp_path(analytic_path)
            combined_raw_poses = [*raw_poses, *analytic_poses[1:]]
            poses = post_process_hybrid_poses(
                grid_info,
                cell_lookup,
                combined_raw_poses,
                safe_options,
                protected_indexes,
            )
            return HybridAStarResult(
                poses,
                [world_to_grid_index_or_current(grid_info, pose, current_index) for pose in poses],
                "ok",
                expanded_nodes,
                current_node.g_score + get_reeds_shepp_path_cost(analytic_path, safe_options),
                len(combined_raw_poses),
                not are_pose_paths_equivalent(combined_raw_poses, poses),
            )

        closed.add(current_key)
        expanded_nodes += 1
        if expanded_nodes >= safe_options.max_expanded_nodes:
            return HybridAStarResult([], [], "max_expanded_nodes_reached", expanded_nodes)

        for next_node in iter_motion_primitives(current_node, safe_options):
            next_index = world_to_grid_index(grid_info, next_node.x_cm, next_node.y_cm)
            if next_index is None:
                continue

            next_heading_index = get_heading_index(next_node.yaw_radian, safe_options)
            next_key = (
                next_index[0],
                next_index[1],
                next_heading_index,
                next_node.direction,
                get_reverse_distance_bin(next_node, safe_options),
            )
            if next_key in closed:
                continue

            if not is_motion_collision_free(
                grid_info,
                cell_lookup,
                current_node,
                next_node,
                safe_options,
                protected_indexes,
            ):
                continue

            next_cell = cell_lookup.get(next_index)
            if not isinstance(next_cell, dict):
                continue

            step_cost = get_step_cost(current_node, next_node, next_cell, safe_options)
            tentative_g_score = current_node.g_score + step_cost
            if tentative_g_score >= g_scores.get(next_key, math.inf):
                continue

            next_node.parent_key = current_key
            next_node.g_score = tentative_g_score
            nodes[next_key] = next_node
            g_scores[next_key] = tentative_g_score
            heap_counter += 1
            priority = tentative_g_score + estimate_cost_to_goal(next_node.x_cm, next_node.y_cm, goal_pose)
            heapq.heappush(open_heap, (priority, heap_counter, next_key))

    return HybridAStarResult([], [], "not_found", expanded_nodes)


def iter_motion_primitives(
    current: _SearchNode,
    options: HybridAStarOptions,
) -> list[_SearchNode]:
    directions = ["Forward"]
    if options.allow_reverse:
        directions.append("Reverse")

    turn_signs = [-1.0, 1.0]
    if options.allow_straight:
        turn_signs.insert(1, 0.0)

    primitives: list[_SearchNode] = []
    for direction in directions:
        direction_sign = 1.0 if direction == "Forward" else -1.0
        step_cm = options.step_distance_cm
        if direction == "Reverse":
            step_cm *= options.reverse_step_distance_scale

        reverse_distance_cm = 0.0
        if direction == "Reverse":
            reverse_distance_cm = current.reverse_distance_cm + step_cm if current.direction == "Reverse" else step_cm
            if (
                options.max_continuous_reverse_distance_cm > 0.0
                and reverse_distance_cm > options.max_continuous_reverse_distance_cm
            ):
                continue

        for turn_sign in turn_signs:
            next_x, next_y, next_yaw = integrate_motion(
                current.x_cm,
                current.y_cm,
                current.yaw_radian,
                direction_sign,
                turn_sign,
                step_cm,
                options.min_turning_radius_cm,
            )
            primitives.append(
                _SearchNode(
                    next_x,
                    next_y,
                    next_yaw,
                    direction,
                    turn_sign,
                    reverse_distance_cm,
                    math.inf,
                    None,
                )
            )

    return primitives


def integrate_motion(
    x_cm: float,
    y_cm: float,
    yaw_radian: float,
    direction_sign: float,
    turn_sign: float,
    step_cm: float,
    turning_radius_cm: float,
) -> tuple[float, float, float]:
    if abs(turn_sign) <= 1.0e-6:
        return (
            x_cm + math.cos(yaw_radian) * direction_sign * step_cm,
            y_cm + math.sin(yaw_radian) * direction_sign * step_cm,
            normalize_radian(yaw_radian),
        )

    curvature = turn_sign / max(turning_radius_cm, 1.0)
    next_yaw = normalize_radian(yaw_radian + direction_sign * curvature * step_cm)
    next_x = x_cm + (math.sin(next_yaw) - math.sin(yaw_radian)) / curvature
    next_y = y_cm + (-math.cos(next_yaw) + math.cos(yaw_radian)) / curvature
    return next_x, next_y, next_yaw


def is_motion_collision_free(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    current: _SearchNode,
    next_node: _SearchNode,
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> bool:
    distance_cm = math.hypot(next_node.x_cm - current.x_cm, next_node.y_cm - current.y_cm)
    sample_count = max(math.ceil(distance_cm / options.collision_sample_step_cm), 1)

    for sample_index in range(sample_count + 1):
        alpha = sample_index / sample_count
        sample_x = current.x_cm + (next_node.x_cm - current.x_cm) * alpha
        sample_y = current.y_cm + (next_node.y_cm - current.y_cm) * alpha
        sample_yaw_radian = interpolate_radian(current.yaw_radian, next_node.yaw_radian, alpha)
        if not is_pose_clear(
            grid_info,
            cell_lookup,
            sample_x,
            sample_y,
            sample_yaw_radian,
            options,
            protected_indexes,
        ):
            return False

    return True


def try_reeds_shepp_analytic_expansion(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    current_node: _SearchNode,
    goal_pose: dict[str, Any],
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
    expanded_nodes: int,
) -> ReedsSheppPath | None:
    if not options.analytic_expansion_enabled:
        return None
    if expanded_nodes % options.analytic_expansion_interval != 0:
        return None

    goal_x_cm = get_float_field(goal_pose, "x")
    goal_y_cm = get_float_field(goal_pose, "y")
    distance_to_goal_cm = math.hypot(goal_x_cm - current_node.x_cm, goal_y_cm - current_node.y_cm)
    if distance_to_goal_cm > options.analytic_expansion_max_distance_cm:
        return None

    goal_yaw_radian = get_goal_yaw_radian_for_analytic_expansion(current_node, goal_pose)
    path = find_reeds_shepp_path(
        current_node.x_cm,
        current_node.y_cm,
        current_node.yaw_radian,
        goal_x_cm,
        goal_y_cm,
        goal_yaw_radian,
        options.min_turning_radius_cm,
        options.analytic_expansion_sample_step_cm,
        allow_reverse=options.allow_reverse,
    )
    if path is None:
        return None
    if path.length_cm > max(distance_to_goal_cm, 1.0) * options.analytic_expansion_max_length_multiplier:
        return None
    if not is_reeds_shepp_reverse_distance_allowed(path, current_node, options):
        return None
    if not is_reeds_shepp_path_collision_free(grid_info, cell_lookup, path, options, protected_indexes):
        return None

    return path


def get_goal_yaw_radian_for_analytic_expansion(
    current_node: _SearchNode,
    goal_pose: dict[str, Any],
) -> float:
    if "yawDegree" in goal_pose:
        return math.radians(get_float_field(goal_pose, "yawDegree", 0.0))

    delta_x_cm = get_float_field(goal_pose, "x") - current_node.x_cm
    delta_y_cm = get_float_field(goal_pose, "y") - current_node.y_cm
    if math.isclose(delta_x_cm, 0.0) and math.isclose(delta_y_cm, 0.0):
        return current_node.yaw_radian

    return math.atan2(delta_y_cm, delta_x_cm)


def is_reeds_shepp_reverse_distance_allowed(
    path: ReedsSheppPath,
    current_node: _SearchNode,
    options: HybridAStarOptions,
) -> bool:
    if options.max_continuous_reverse_distance_cm <= 0.0:
        return True

    reverse_distance_cm = current_node.reverse_distance_cm if current_node.direction == "Reverse" else 0.0
    previous_direction = current_node.direction
    for segment in path.segments:
        if segment.direction == "Reverse":
            reverse_distance_cm = (
                reverse_distance_cm + segment.length_cm
                if previous_direction == "Reverse"
                else segment.length_cm
            )
            if reverse_distance_cm > options.max_continuous_reverse_distance_cm:
                return False
        else:
            reverse_distance_cm = 0.0

        previous_direction = segment.direction

    return True


def is_reeds_shepp_path_collision_free(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    path: ReedsSheppPath,
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> bool:
    return all(
        is_pose_clear(
            grid_info,
            cell_lookup,
            pose.x_cm,
            pose.y_cm,
            pose.yaw_radian,
            options,
            protected_indexes,
        )
        for pose in path.poses
    )


def build_hybrid_poses_from_reeds_shepp_path(path: ReedsSheppPath) -> list[HybridPose]:
    return [
        HybridPose(
            pose.x_cm,
            pose.y_cm,
            math.degrees(pose.yaw_radian),
            pose.direction,
        )
        for pose in path.poses
    ]


def get_reeds_shepp_path_cost(path: ReedsSheppPath, options: HybridAStarOptions) -> float:
    cost = 0.0
    previous_direction = path.segments[0].direction if path.segments else "Forward"
    for segment in path.segments:
        segment_cost = segment.length_cm
        if segment.direction == "Reverse":
            segment_cost *= options.reverse_cost_multiplier
        if segment.direction != previous_direction:
            segment_cost += options.gear_switch_cost_penalty
        if segment.segment_type != "Straight":
            segment_cost += (
                options.reverse_turn_cost_penalty
                if segment.direction == "Reverse"
                else options.turn_cost_penalty
            )

        cost += segment_cost
        previous_direction = segment.direction

    return cost


def is_pose_clear(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    x_cm: float,
    y_cm: float,
    yaw_radian: float,
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> bool:
    center_index = world_to_grid_index(grid_info, x_cm, y_cm)
    if center_index is None:
        return False

    if has_usable_footprint(options):
        return is_footprint_clear(
            grid_info,
            cell_lookup,
            x_cm,
            y_cm,
            yaw_radian,
            options,
            protected_indexes,
        )

    return is_index_clear_with_radius(
        grid_info,
        cell_lookup,
        center_index,
        options.clearance_radius_cm,
        protected_indexes,
    )


def has_usable_footprint(options: HybridAStarOptions) -> bool:
    return (
        options.footprint_check_enabled
        and options.footprint_half_length_cm > 0.0
        and options.footprint_half_width_cm > 0.0
    )


def is_footprint_clear(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    x_cm: float,
    y_cm: float,
    yaw_radian: float,
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> bool:
    cos_yaw = math.cos(yaw_radian)
    sin_yaw = math.sin(yaw_radian)

    for local_x_cm, local_y_cm in iter_footprint_sample_offsets(options):
        sample_x_cm = x_cm + cos_yaw * local_x_cm - sin_yaw * local_y_cm
        sample_y_cm = y_cm + sin_yaw * local_x_cm + cos_yaw * local_y_cm
        sample_index = world_to_grid_index(grid_info, sample_x_cm, sample_y_cm)
        if sample_index is None:
            return False
        if not is_index_clear_with_radius(grid_info, cell_lookup, sample_index, 0.0, protected_indexes):
            return False

    return True


def iter_footprint_sample_offsets(options: HybridAStarOptions) -> list[tuple[float, float]]:
    half_length_cm = options.footprint_half_length_cm + options.footprint_padding_cm
    half_width_cm = options.footprint_half_width_cm + options.footprint_padding_cm
    local_x_values = build_axis_sample_values(half_length_cm, options.footprint_sample_step_cm)
    local_y_values = build_axis_sample_values(half_width_cm, options.footprint_sample_step_cm)
    return [(local_x_cm, local_y_cm) for local_x_cm in local_x_values for local_y_cm in local_y_values]


def build_axis_sample_values(half_extent_cm: float, sample_step_cm: float) -> list[float]:
    safe_half_extent_cm = max(half_extent_cm, 0.0)
    if safe_half_extent_cm <= 0.0:
        return [0.0]

    safe_sample_step_cm = max(sample_step_cm, 1.0)
    segment_count = max(math.ceil((safe_half_extent_cm * 2.0) / safe_sample_step_cm), 1)
    values = [
        -safe_half_extent_cm + (2.0 * safe_half_extent_cm * index / segment_count)
        for index in range(segment_count + 1)
    ]
    if 0.0 not in values:
        values.append(0.0)
        values.sort()

    return values


def is_index_clear_with_radius(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    center_index: GridIndex,
    clearance_radius_cm: float,
    protected_indexes: set[GridIndex],
) -> bool:
    center_cell = cell_lookup.get(center_index)
    if is_blocked_cell(center_cell) and center_index not in protected_indexes:
        return False

    if clearance_radius_cm <= 0.0:
        return True

    cell_size_cm = max(get_float_field(grid_info, "cellSizeCm", 100.0), 1.0)
    radius_cell_count = max(math.ceil(clearance_radius_cm / cell_size_cm), 0)
    center_world = grid_index_to_world_location(grid_info, center_index)

    for offset_x in range(-radius_cell_count, radius_cell_count + 1):
        for offset_y in range(-radius_cell_count, radius_cell_count + 1):
            neighbor_index = (center_index[0] + offset_x, center_index[1] + offset_y)
            if neighbor_index == center_index or neighbor_index in protected_indexes:
                continue

            neighbor_world = grid_index_to_world_location(grid_info, neighbor_index)
            if math.hypot(neighbor_world["x"] - center_world["x"], neighbor_world["y"] - center_world["y"]) > (
                clearance_radius_cm + cell_size_cm * 0.5
            ):
                continue

            if is_blocked_cell(cell_lookup.get(neighbor_index)):
                return False

    return True


def get_step_cost(
    current: _SearchNode,
    next_node: _SearchNode,
    next_cell: dict[str, Any],
    options: HybridAStarOptions,
) -> float:
    distance_cm = math.hypot(next_node.x_cm - current.x_cm, next_node.y_cm - current.y_cm)
    cost = distance_cm * get_cell_travel_cost(next_cell)

    if next_node.direction == "Reverse":
        cost *= options.reverse_cost_multiplier

    if current.direction != next_node.direction:
        cost += options.gear_switch_cost_penalty

    if abs(next_node.turn_sign) > 1.0e-6:
        cost += options.reverse_turn_cost_penalty if next_node.direction == "Reverse" else options.turn_cost_penalty

    if abs(current.turn_sign) > 1.0e-6 and abs(next_node.turn_sign) > 1.0e-6:
        if math.copysign(1.0, current.turn_sign) != math.copysign(1.0, next_node.turn_sign):
            cost += options.turn_switch_cost_penalty

    return cost


def is_goal_reached(
    node: _SearchNode,
    goal_pose: dict[str, Any],
    options: HybridAStarOptions,
) -> bool:
    distance_cm = estimate_cost_to_goal(node.x_cm, node.y_cm, goal_pose)
    if distance_cm > options.goal_acceptance_distance_cm:
        return False

    if "yawDegree" not in goal_pose:
        return True

    yaw_error_degree = abs(normalize_angle_degree(math.degrees(node.yaw_radian) - get_float_field(goal_pose, "yawDegree")))
    return yaw_error_degree <= options.goal_acceptance_angle_degree


def estimate_cost_to_goal(x_cm: float, y_cm: float, goal_pose: dict[str, Any]) -> float:
    return math.hypot(get_float_field(goal_pose, "x") - x_cm, get_float_field(goal_pose, "y") - y_cm)


def reconstruct_pose_path(
    nodes: dict[SearchKey, _SearchNode],
    current_key: SearchKey,
) -> list[HybridPose]:
    poses: list[HybridPose] = []
    key: SearchKey | None = current_key
    while key is not None:
        node = nodes[key]
        poses.append(
            HybridPose(
                node.x_cm,
                node.y_cm,
                math.degrees(node.yaw_radian),
                node.direction,
            )
        )
        key = node.parent_key

    poses.reverse()
    return poses


def post_process_hybrid_poses(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    poses: list[HybridPose],
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> list[HybridPose]:
    if not options.post_process_enabled or len(poses) <= 2:
        return poses

    processed = remove_duplicate_poses(poses, options.dedupe_distance_cm)
    if options.shortcut_enabled:
        processed = shortcut_hybrid_poses(
            grid_info,
            cell_lookup,
            processed,
            options,
            protected_indexes,
        )

    if options.resample_enabled:
        processed = resample_hybrid_poses(processed, options.resample_distance_cm)

    return recompute_pose_yaws(processed)


def remove_duplicate_poses(poses: list[HybridPose], dedupe_distance_cm: float) -> list[HybridPose]:
    if len(poses) <= 2 or dedupe_distance_cm <= 0.0:
        return poses

    deduped = [poses[0]]
    for index, pose in enumerate(poses[1:], start=1):
        previous = deduped[-1]
        is_last_pose = index == len(poses) - 1
        distance_cm = math.hypot(pose.x_cm - previous.x_cm, pose.y_cm - previous.y_cm)
        if not is_last_pose and pose.direction == previous.direction and distance_cm <= dedupe_distance_cm:
            continue

        deduped.append(pose)

    return deduped


def shortcut_hybrid_poses(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    poses: list[HybridPose],
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> list[HybridPose]:
    if len(poses) <= 2:
        return poses

    shortened = [poses[0]]
    anchor_index = 0
    last_index = len(poses) - 1

    while anchor_index < last_index:
        best_index = anchor_index + 1
        for candidate_index in range(last_index, anchor_index, -1):
            if not has_uniform_segment_direction(poses, anchor_index + 1, candidate_index):
                continue

            if is_shortcut_collision_free(
                grid_info,
                cell_lookup,
                poses[anchor_index],
                poses[candidate_index],
                options,
                protected_indexes,
            ):
                best_index = candidate_index
                break

        shortened.append(poses[best_index])
        anchor_index = best_index

    return shortened


def has_uniform_segment_direction(poses: list[HybridPose], start_index: int, end_index: int) -> bool:
    if start_index > end_index:
        return False

    direction = poses[start_index].direction
    return all(pose.direction == direction for pose in poses[start_index : end_index + 1])


def is_shortcut_collision_free(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start: HybridPose,
    end: HybridPose,
    options: HybridAStarOptions,
    protected_indexes: set[GridIndex],
) -> bool:
    distance_cm = math.hypot(end.x_cm - start.x_cm, end.y_cm - start.y_cm)
    sample_count = max(math.ceil(distance_cm / options.collision_sample_step_cm), 1)

    for sample_index in range(sample_count + 1):
        alpha = sample_index / sample_count
        sample_x = start.x_cm + (end.x_cm - start.x_cm) * alpha
        sample_y = start.y_cm + (end.y_cm - start.y_cm) * alpha
        sample_yaw_radian = interpolate_radian(
            math.radians(start.yaw_degree),
            math.radians(end.yaw_degree),
            alpha,
        )
        if not is_pose_clear(
            grid_info,
            cell_lookup,
            sample_x,
            sample_y,
            sample_yaw_radian,
            options,
            protected_indexes,
        ):
            return False

    return True


def resample_hybrid_poses(poses: list[HybridPose], resample_distance_cm: float) -> list[HybridPose]:
    if len(poses) <= 1:
        return poses

    safe_resample_distance_cm = max(resample_distance_cm, 1.0)
    resampled = [poses[0]]

    for pose in poses[1:]:
        previous = resampled[-1]
        distance_cm = math.hypot(pose.x_cm - previous.x_cm, pose.y_cm - previous.y_cm)
        sample_count = max(math.ceil(distance_cm / safe_resample_distance_cm), 1)

        for sample_index in range(1, sample_count + 1):
            alpha = sample_index / sample_count
            x_cm = previous.x_cm + (pose.x_cm - previous.x_cm) * alpha
            y_cm = previous.y_cm + (pose.y_cm - previous.y_cm) * alpha
            yaw_degree = build_segment_yaw_degree(previous.x_cm, previous.y_cm, x_cm, y_cm, pose.direction)
            resampled.append(HybridPose(x_cm, y_cm, yaw_degree, pose.direction))

    return resampled


def recompute_pose_yaws(poses: list[HybridPose]) -> list[HybridPose]:
    if len(poses) <= 1:
        return poses

    recomputed = [poses[0]]
    for pose in poses[1:]:
        previous = recomputed[-1]
        yaw_degree = build_segment_yaw_degree(previous.x_cm, previous.y_cm, pose.x_cm, pose.y_cm, pose.direction)
        recomputed.append(HybridPose(pose.x_cm, pose.y_cm, yaw_degree, pose.direction))

    return recomputed


def build_segment_yaw_degree(
    start_x_cm: float,
    start_y_cm: float,
    end_x_cm: float,
    end_y_cm: float,
    direction: str,
) -> float:
    if math.isclose(start_x_cm, end_x_cm) and math.isclose(start_y_cm, end_y_cm):
        return 0.0

    segment_yaw_degree = math.degrees(math.atan2(end_y_cm - start_y_cm, end_x_cm - start_x_cm))
    if direction == "Reverse":
        segment_yaw_degree += 180.0

    return normalize_angle_degree(segment_yaw_degree)


def are_pose_paths_equivalent(first: list[HybridPose], second: list[HybridPose]) -> bool:
    if len(first) != len(second):
        return False

    return all(
        math.isclose(first_pose.x_cm, second_pose.x_cm)
        and math.isclose(first_pose.y_cm, second_pose.y_cm)
        and math.isclose(first_pose.yaw_degree, second_pose.yaw_degree)
        and first_pose.direction == second_pose.direction
        for first_pose, second_pose in zip(first, second)
    )


def world_to_grid_index_or_current(
    grid_info: dict[str, Any],
    pose: HybridPose,
    fallback: GridIndex,
) -> GridIndex:
    return world_to_grid_index(grid_info, pose.x_cm, pose.y_cm) or fallback


def get_heading_index(yaw_radian: float, options: HybridAStarOptions) -> int:
    normalized = normalize_radian(yaw_radian) % (math.tau)
    heading_step = math.tau / max(options.heading_bin_count, 1)
    return int(round(normalized / heading_step)) % options.heading_bin_count


def get_reverse_distance_bin(node: _SearchNode, options: HybridAStarOptions) -> int:
    if node.direction != "Reverse":
        return 0

    reverse_step_cm = max(options.step_distance_cm * options.reverse_step_distance_scale, 1.0)
    return int(round(node.reverse_distance_cm / reverse_step_cm))


def normalize_radian(angle_radian: float) -> float:
    return (angle_radian + math.pi) % math.tau - math.pi


def interpolate_radian(start_radian: float, end_radian: float, alpha: float) -> float:
    return normalize_radian(start_radian + shortest_angle_delta_radian(start_radian, end_radian) * alpha)


def shortest_angle_delta_radian(start_radian: float, end_radian: float) -> float:
    return normalize_radian(end_radian - start_radian)


def normalize_angle_degree(angle_degree: float) -> float:
    return (angle_degree + 180.0) % 360.0 - 180.0


def get_bool_setting(source: dict[str, Any], names: tuple[str, ...], default: bool) -> bool:
    for name in names:
        if name not in source:
            continue

        value = source[name]
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            return value.strip().lower() in {"1", "true", "yes", "y", "on"}

        return bool(value)

    return default


def get_float_setting(source: dict[str, Any], names: tuple[str, ...], default: float) -> float:
    for name in names:
        if name not in source:
            continue

        try:
            return float(source[name])
        except (TypeError, ValueError):
            return default

    return default


def get_int_setting(source: dict[str, Any], names: tuple[str, ...], default: int) -> int:
    for name in names:
        if name not in source:
            continue

        try:
            return int(source[name])
        except (TypeError, ValueError):
            return default

    return default
