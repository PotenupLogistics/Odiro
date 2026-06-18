# Scenario JSON Body

이 문서는 LLM이 응답해야 하는 `scenario.json` 본문 형식을 정의한다. 응답 envelope, HTTP status, 파일 경로, 저장 위치는 AI 서비스/UE 연결부의 책임이며, 이 문서의 JSON은 UE가 꺼내서 `<UserProject>/scenario.json`으로 저장할 수 있는 순수 scenario object만 다룬다.

## 응답 원칙

LLM은 아래 형태의 JSON object 하나를 생성한다.

- Markdown 코드블록, 설명 문장, 주석을 섞지 않는다.
- `schema`는 항상 `"scenario"`이다.
- `version`은 항상 `1`이다.
- `template_id`를 쓰지 않고 `scenario_id`를 쓴다.
- `scenario_path`, `project_path`, `run_id`, `episode_count`, `seed`, 분석 결과는 넣지 않는다.
- `scenario_sample` 또는 `episode_scenario`를 직접 생성하지 않는다.
- 고정값과 random range/choices는 모두 이 `scenario.json` 본문 안에 둔다.

## Root

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "pinch_oncoming_low_coop",
  "intent": "Validate whether the robot can safely pass an oncoming pedestrian in a narrow corridor.",
  "corridor": {},
  "obstacles": {},
  "pedestrians": {},
  "robot": {}
}
```

| Field | Required | Description |
| --- | --- | --- |
| `schema` | required | Fixed value `"scenario"`. |
| `version` | required | Fixed value `1`. |
| `scenario_id` | required | Human-readable snake_case scenario id. |
| `intent` | required | Natural-language scenario intent or hypothesis. |
| `corridor` | required | Spatial skeleton, lanes, surfaces, and semantic segments. |
| `obstacles` | recommended | Static obstacle placement rules. Use an empty rule object when no obstacles are needed. |
| `pedestrians` | recommended | Background pedestrian and designed encounter rules. |
| `robot` | required | Robot start and goal anchors. |

## Value Shapes

Numeric and integer fields can be fixed values or sampled ranges.

```json
"walkway_width_m": 3.0
```

```json
"walkway_width_m": { "min": 2.5, "max": 4.0 }
```

String fields that explicitly support choices can be fixed values or sampled choices.

```json
"replaced_by": "grass"
```

```json
"replaced_by": { "choices": ["grass", "road"] }
```

Ranges and choices are resolved later when the project scenario is materialized for an episode. The LLM should keep uncertainty as ranges/choices only when variation is intentional.

## corridor

`corridor` defines the scenario-local path and semantic areas. Coordinates are local meters, not Unreal world coordinates.

```json
"corridor": {
  "axis": {
    "type": "polyline",
    "points_m": [[0, 0], [12, 0], [24, 0]]
  },
  "walkway_width_m": { "min": 2.4, "max": 3.2 },
  "building_side": [
    { "surface": "building", "width_m": 1.0 }
  ],
  "curb_side": [
    { "surface": "road", "width_m": 1.0 }
  ],
  "segments": [
    { "id": "entry", "type": "straight", "along_range_m": [0, 8] },
    { "id": "pinch", "type": "narrowing", "along_range_m": [8, 16], "replaced_by": { "choices": ["sidewalk", "grass"] } },
    { "id": "exit", "type": "straight", "along_range_m": [16, 24] }
  ]
}
```

| Field | Description |
| --- | --- |
| `axis.type` | Fixed value `"polyline"` in v1. |
| `axis.points_m` | At least two `[x, y]` points in scenario-local meters. |
| `walkway_width_m` | Main walkway width in meters. Fixed number or `{ "min", "max" }`. |
| `building_side` | Lane/surface rules on the building side of the walkway. |
| `curb_side` | Lane/surface rules on the curb side of the walkway. |
| `building_side[].surface`, `curb_side[].surface` | Surface catalog id such as `sidewalk`, `grass`, `road`, `wall`, or `building`. |
| `building_side[].width_m`, `curb_side[].width_m` | Lane width in meters. Fixed number or range. |
| `segments[].id` | Unique segment id inside this scenario. |
| `segments[].type` | One of `straight`, `narrowing`, `crosswalk`, `entrance`. |
| `segments[].along_range_m` | `[start_m, end_m]` distance range along the corridor axis. |
| `segments[].replaced_by` | Optional fixed surface id or `{ "choices": [...] }`. |

## obstacles

`obstacles` describes placement rules relative to corridor segments and lanes.

```json
"obstacles": {
  "min_clear_width_m": 1.2,
  "placements": [
    {
      "id": "crate_at_pinch",
      "kind": "fixed",
      "prop": "crate",
      "at": {
        "segment": "pinch",
        "along_m": 11.0,
        "offset_m": 0.35,
        "lane": "walkway"
      },
      "yaw_deg": { "min": -10.0, "max": 10.0 },
      "allow_blocking": false
    }
  ]
}
```

| Field | Description |
| --- | --- |
| `min_clear_width_m` | Minimum clear walkway width the generator should preserve unless explicitly overridden. |
| `placements[].id` | Unique placement id inside `obstacles.placements`. |
| `placements[].kind` | One of `fixed`, `pattern`, `scatter`. |
| `placements[].allow_blocking` | `true` only when the placement intentionally violates the clear-width rule. |

### fixed placement

Use `fixed` for a specific obstacle anchored to one corridor-local pose.

```json
{
  "id": "crate_1",
  "kind": "fixed",
  "prop": "crate",
  "at": { "segment": "pinch", "along_m": 10.5, "offset_m": 0.2, "lane": "walkway" },
  "yaw_deg": 0.0,
  "allow_blocking": false
}
```

Required for `fixed`: `prop`, `at.segment`, `at.along_m`, `at.offset_m`.

### pattern placement

Use `pattern` for a gate, line, cluster, or other structured obstacle group.

```json
{
  "id": "gate_1",
  "kind": "pattern",
  "pattern": "gate",
  "prop": "bollard",
  "at": { "segment": "entry", "along_m": 6.0, "offset_m": 0.0, "lane": "across" },
  "count": 2,
  "gap_width_m": 1.4,
  "allow_blocking": false
}
```

Useful optional fields: `count`, `spacing_m`, `gap_width_m`, `yaw_deg`.

### scatter placement

Use `scatter` for density-based placement in segment/lane zones.

```json
{
  "id": "light_clutter",
  "kind": "scatter",
  "zone": {
    "segments": ["entry", "exit"],
    "lanes": ["building_edge", "curb_edge"]
  },
  "density_per_10m": { "min": 0.5, "max": 1.5 },
  "palette": {
    "categories": ["small_static"],
    "classes": ["cone", "crate"]
  },
  "allow_blocking": false
}
```

Required for `scatter`: `density_per_10m`. Segment ids in `zone.segments` must exist in `corridor.segments`.

Common lane hints:

| Value | Meaning |
| --- | --- |
| `walkway` | Main traversable walkway. |
| `building_edge` | Edge near the building side. |
| `center` | Center of the walkway. |
| `curb_edge` | Edge near the curb side. |
| `across` | Across the walkway, mainly for pattern placement. |

## pedestrians

`pedestrians` describes background pedestrians and designed encounters. It does not list final paths; paths are generated from corridor semantics later.

```json
"pedestrians": {
  "background": {
    "count": { "min": 1, "max": 3 },
    "speed_mps": { "min": 0.8, "max": 1.4 },
    "spawn_zone": { "segments": ["entry", "exit"] }
  },
  "encounters": [
    {
      "id": "main_oncoming",
      "type": "oncoming_pass",
      "at": "pinch",
      "persona": "assertive",
      "meet_offset_m": 0.0,
      "overrides": {
        "cooperation": { "min": 0.15, "max": 0.4 },
        "personal_space_m": 0.7
      }
    }
  ]
}
```

| Field | Description |
| --- | --- |
| `background.count` | Number of background pedestrians. Integer or integer range. |
| `background.speed_mps` | Background pedestrian walking speed in meters per second. |
| `background.spawn_zone.segments` | Optional segment filter for background pedestrian spawning. |
| `encounters[].id` | Unique encounter id inside `pedestrians.encounters`. |
| `encounters[].type` | One of `oncoming_pass`, `overtake`, `cross_path`, `standing_group`. |
| `encounters[].at` | Segment id where the encounter should occur. |
| `encounters[].persona` | Persona id such as `passive`, `normal`, `assertive`, or `vulnerable`. |
| `encounters[].meet_offset_m` | Offset from the intended meeting point in meters. |
| `encounters[].overrides` | Optional behavior values overriding persona defaults. |

Supported override fields:

| Field | Description |
| --- | --- |
| `cooperation` | Willingness to yield to the robot. |
| `evasiveness` | Willingness to sidestep or avoid the robot. |
| `personal_space_m` | Desired personal space in meters. |
| `awareness_horizon_s` | Prediction horizon in seconds. |
| `max_yield_wait_s` | Maximum yield waiting time in seconds. |
| `sidestep_distance_m` | Sidestep distance in meters. |

## robot

`robot` stores start and goal as abstract or corridor-local anchors. It does not store policy, planner, sensor, or robot setup values.

```json
"robot": {
  "start": { "type": "entry" },
  "goal": {
    "type": "corridor_pose",
    "segment": "exit",
    "along_m": 22.0,
    "offset_m": 0.0,
    "lane": "walkway",
    "heading": "forward"
  }
}
```

| Field | Description |
| --- | --- |
| `start`, `goal` | Robot anchor objects. |
| `type` | One of `entry`, `exit`, `corridor_pose`. |
| `segment` | Required when `type` is `corridor_pose`. Must reference a corridor segment id. |
| `along_m` | Required when `type` is `corridor_pose`. Distance along the corridor axis. |
| `offset_m` | Required when `type` is `corridor_pose`. Lateral offset from the corridor axis. |
| `lane` | Optional editor/generation hint. |
| `heading` | Optional. One of `forward`, `backward`, `auto`. Omit when `auto` is acceptable. |

## Minimal Valid Example

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "simple_sidewalk_pass",
  "intent": "Validate that the robot can traverse a straight sidewalk with no designed conflicts.",
  "corridor": {
    "axis": { "type": "polyline", "points_m": [[0, 0], [12, 0]] },
    "walkway_width_m": 2.5,
    "building_side": [{ "surface": "building", "width_m": 1.0 }],
    "curb_side": [{ "surface": "road", "width_m": 1.0 }],
    "segments": [{ "id": "main", "type": "straight", "along_range_m": [0, 12] }]
  },
  "obstacles": {
    "min_clear_width_m": 1.2,
    "placements": []
  },
  "pedestrians": {
    "background": { "count": 0, "speed_mps": 1.0 },
    "encounters": []
  },
  "robot": {
    "start": { "type": "entry" },
    "goal": { "type": "exit" }
  }
}
```

## Validation Rules

UE validates the JSON before saving or running it.

- `schema` must be `"scenario"`.
- `version` must be `1`.
- `scenario_id` and `intent` must not be empty.
- `corridor.axis.points_m` must contain at least two points.
- `corridor.walkway_width_m` is required.
- `corridor.segments[].id` must be unique and non-empty.
- `corridor.segments[].along_range_m` must be valid `[start, end]` ranges.
- All referenced segment ids in obstacles, pedestrians, and robot anchors must exist.
- `fixed` and `pattern` obstacle placements require `prop`, `at.segment`, `at.along_m`, and `at.offset_m`.
- `scatter` obstacle placements require `density_per_10m`.
- `pedestrians.encounters[].id` must be unique and non-empty.
- `pedestrians.encounters[].persona` must not be empty.
- `corridor_pose` robot anchors require `segment`, `along_m`, and `offset_m`; fixed `along_m` must fall within the referenced segment range.
- For any `{ "min": a, "max": b }` range, `min` must be less than or equal to `max`.
- For any `{ "choices": [...] }` value, `choices` must not be empty.
