# scenario editor field inventory

이 문서는 `scenario.json.md`를 기준으로 Scenario Editor Detail 패널이 노출해야 할 field, 사용자 표시명, 입력 control, 현재 구현 상태를 정리한다. 목표는 사용자가 JSON path를 직접 편집한다는 느낌 없이 semantic한 editor surface로 `scenario.json`을 작성하게 만드는 것이다.

## Status Legend

| 상태 | 의미 |
| --- | --- |
| `Live` | shared type, JSON read/write, sidebar row가 모두 존재한다. |
| `Partial` | 데이터 경로는 있지만 control, options, visibility, viewport 표현 중 일부가 부족하다. |
| `Missing UI` | schema/runtime 경로는 있지만 sidebar row가 없다. |
| `Doc-only` | `scenario.json.md` 또는 catalog에는 있지만 현재 shared type/read/write가 보존하지 않는다. |
| `Editor-owned` | schema/runtime 경로는 있지만 사용자가 편집하거나 볼 필요가 없어 editor surface에서 숨긴다. |
| `Container` | object/array grouping field다. 직접 입력 row가 아니라 block/list 관리 대상이다. |

## Control Legend

| Control | 용도 |
| --- | --- |
| `ReadOnlyText` | 고정값 또는 요약값 표시. |
| `Text` | 자유 문자열 입력. |
| `MultilineText` | 긴 자연어 설명 입력. |
| `Number` | 단일 숫자 입력. |
| `RangeNumber` | fixed number 또는 `{ "min", "max" }` 입력. |
| `RangeInteger` | fixed integer 또는 `{ "min", "max" }` 입력. |
| `Combo` | 폐쇄형 또는 catalog 기반 선택지. |
| `Checkbox` | boolean. |
| `ListText` | 임시 comma/list text. 이후 multi-select/list editor로 대체할 수 있다. |
| `Block` | 접기/펼치기 가능한 object block. |
| `ArrayBlock` | 추가/삭제 가능한 반복 object block. |
| `hidden` | 사용자 authoring surface에서 제외되는 editor/runtime-owned field. |

## Root / Main

| JSON path | 표시명 | Block | Schema type | 권장 control | Options source | Viewport | 현재 Sidebar | 상태 | 차이/다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$` | 시나리오 | Main > 시나리오 | object | Block | - | none | `RootBlockWidget` | Container | Root metadata grouping. |
| `$.schema` | 문서 형식 | Main > 시나리오 | string, fixed `scenario` | hidden | fixed | none | hidden | Editor-owned | User-facing editor에서 제외하고 JSON read/write 경로가 소유한다. |
| `$.version` | 버전 | Main > 시나리오 | number, fixed `1` | hidden | fixed | none | hidden | Editor-owned | User-facing editor에서 제외하고 JSON read/write 경로가 소유한다. |
| `$.scenario_id` | 시나리오 이름 | Main > 시나리오 | string | Text | user input | none | `ScenarioId`, editable text | Live | 저장 id 성격이라 snake_case helper/validation hint 필요. |
| `$.intent` | 검증 목표 | Main > 시나리오 | string | MultilineText | user input | none | `Intent`, editable multiline | Live | UX copy는 자연어 목표 중심. |
| `$.robot` | 로봇 경로 | Main > 로봇 경로 | object | Block | - | start/goal markers | `RobotBlockWidget` | Container | Summary rows for start/goal are read-only. |
| `$.corridor` | 통로 | Corridor > 통로 | object | Block | - | corridor preview | `CorridorBlockWidget` | Container | Main/Corridor panel boundary. |
| `$.obstacles` | 장애물 | Obstacle > 장애물 | object | Block | - | obstacle previews | `ObstacleBlockWidget` | Container | Palette placement flow also edits this object. |
| `$.pedestrians` | 보행자 | Pedestrian > 보행자 | object | Block | - | pedestrian preview needed | `PedestriansBlockWidget` | Container | Runtime materialization status differs by encounter type. |

## Corridor

| JSON path | 표시명 | Block | Schema type | 권장 control | Options source | Viewport | 현재 Sidebar | 상태 | 차이/다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$.corridor.axis` | 중심 경로 | Corridor > 중심 경로 | object | Block | - | route polyline, vertices | `AxisBlockWidget` | Container | Vertex/segment overlay work depends on this data. |
| `$.corridor.axis.type` | 경로 유형 | Corridor > 중심 경로 | string, fixed `polyline` | ReadOnlyText | fixed | none | `AxisType`, read-only enum text | Live | Combo 불필요. |
| `$.corridor.axis.points_m` | 경로 점 | Corridor > 경로 점 | array of `[x, y]` | ArrayBlock + count | - | route vertices | `AxisPointsCount`, point widgets | Live | Count row add/remove plus per-point rows. |
| `$.corridor.axis.points_m[].x` | X 위치 | Corridor > 경로 점 N | number | Number | - | vertex handle X | `CorridorPointX` | Live | Unit label m would improve clarity. |
| `$.corridor.axis.points_m[].y` | Y 위치 | Corridor > 경로 점 N | number | Number | - | vertex handle Y | `CorridorPointY` | Live | Unit label m would improve clarity. |
| `$.corridor.walkway_width_m` | 보행로 폭 | Corridor > 보행로 폭 | number or range | RangeNumber | - | walkway width band | `WalkwayWidth`, editable range | Live | Viewport range preview needed for min/max. |
| `$.corridor.building_side` | 건물측 영역 | Corridor > 건물측 영역 | array | ArrayBlock + count | - | side lane bands | `BuildingSideCount`, lane widgets | Live | Add/remove count row exists. |
| `$.corridor.building_side[].surface` | 표면 유형 | Corridor > 건물측 영역 N | string | Combo | `environment-catalog.md` Surface, `DA_ScenarioCorridorSurfaceCatalog` | lane material/color | `CorridorLaneSurface`, Combo | Live | Catalog display name mapping still needed. |
| `$.corridor.building_side[].width_m` | 폭 | Corridor > 건물측 영역 N | number or range | RangeNumber | - | lane width band | `CorridorLaneWidth`, range | Live | Viewport range preview needed. |
| `$.corridor.curb_side` | 도로측 영역 | Corridor > 도로측 영역 | array | ArrayBlock + count | - | side lane bands | `CurbSideCount`, lane widgets | Live | Add/remove count row exists. |
| `$.corridor.curb_side[].surface` | 표면 유형 | Corridor > 도로측 영역 N | string | Combo | `environment-catalog.md` Surface, `DA_ScenarioCorridorSurfaceCatalog` | lane material/color | `CorridorLaneSurface`, Combo | Live | Catalog display name mapping still needed. |
| `$.corridor.curb_side[].width_m` | 폭 | Corridor > 도로측 영역 N | number or range | RangeNumber | - | lane width band | `CorridorLaneWidth`, range | Live | Viewport range preview needed. |
| `$.corridor.segments` | 의미 구간 | Corridor > 의미 구간 | array | ArrayBlock + count | - | segment spans along axis | `SegmentsCount`, segment widgets | Live | Segment span overlay is primary viewport need. |
| `$.corridor.segments[].id` | 구간 이름 | Corridor > 의미 구간 N | string | Text | unique segment ids | outliner/labels | `CorridorSegmentId`, text | Live | References should update or validate on rename. |
| `$.corridor.segments[].type` | 구간 유형 | Corridor > 의미 구간 N | string enum | Combo | `straight`, `narrowing`, `crosswalk`, `entrance` | segment color/icon | `CorridorSegmentType`, Combo | Live | Display/value mapping needed. |
| `$.corridor.segments[].along_range_m` | 적용 범위 | Corridor > 의미 구간 N | `[start_m, end_m]` | RangeNumber | - | segment span handles | `CorridorSegmentAlongRange`, range | Live | Viewport min/max span editing is high priority. |
| `$.corridor.segments[].replaced_by` | 대체 표면 | Corridor > 의미 구간 N | string or choices | Combo + choices mode | Surface catalog | surface overlay | `CorridorSegmentReplacedBy`, Combo with unset | Partial | Current Combo chooses one fixed value; full choices authoring is not surfaced. |

## Obstacles

| JSON path | 표시명 | Block | Schema type | 권장 control | Options source | Viewport | 현재 Sidebar | 상태 | 차이/다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$.obstacles.min_clear_width_m` | 최소 통행 폭 | Obstacle > 최소 통행 폭 | number or range | RangeNumber | - | corridor clearance band | `MinClearWidth`, range | Live | Viewport clearance envelope needed. |
| `$.obstacles.placements` | 배치 규칙 | Obstacle > 배치 규칙 | array | ArrayBlock + count | - | obstacle previews | `PlacementsCount`, placement widgets | Live | Asset palette placement already reuses this list. |
| `$.obstacles.placements[].id` | 규칙 이름 | Obstacle > 배치 규칙 N | string | Text | unique placement ids | outliner/labels | `PlacementId`, text | Live | Rename should keep outliner and actor link coherent. |
| `$.obstacles.placements[].kind` | 배치 방식 | Obstacle > 배치 규칙 N | string enum | Combo | `fixed`, `pattern`, `scatter` | controls visible fields | `PlacementKind`, Combo | Live | Display/value mapping can be improved later. |
| `$.obstacles.placements[].prop` | 장애물 종류 | Obstacle > 배치 규칙 N | string | Combo | `environment-catalog.md` Props, static obstacle palette | obstacle mesh preview | `PlacementProp`, text | Partial | Should use palette/catalog picker, not free text. |
| `$.obstacles.placements[].pattern` | 배치 패턴 | Obstacle > 배치 규칙 N | string | Combo | pattern catalog: `gate`, `line`, `cluster` | pattern preview | `PlacementPattern`, Combo | Partial | Pattern id list is inline options, not a first-class catalog yet. |
| `$.obstacles.placements[].at` | 배치 위치 | Obstacle > 배치 규칙 N | object | Block | - | anchor/handle | implicit rows | Container | Visible for fixed/pattern only. |
| `$.obstacles.placements[].at.segment` | 구간 | Obstacle > 배치 규칙 N | string | Combo | corridor segment ids | anchor on segment | `PlacementSegment`, Combo | Live | Segment id options come from current corridor. |
| `$.obstacles.placements[].at.along_m` | 진행 거리 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | along-position handle/range | `PlacementAlong`, range | Live | Viewport range visualization needed. |
| `$.obstacles.placements[].at.offset_m` | 좌우 위치 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | lateral handle/range | `PlacementOffset`, range | Live | Viewport range visualization needed. |
| `$.obstacles.placements[].at.lane` | 배치 영역 | Obstacle > 배치 규칙 N | string | Combo | lane hints: `walkway`, `building_edge`, `center`, `curb_edge`, `across` | anchor lane guide | `PlacementLane`, Combo | Live | Uses lane hint options with explicit unset. |
| `$.obstacles.placements[].zone` | 허용 구역 | Obstacle > 배치 규칙 N | object | Block | - | scatter zone overlay | implicit rows | Container | Visible for scatter only. |
| `$.obstacles.placements[].zone.segments` | 허용 구간 | Obstacle > 배치 규칙 N | array of string | MultiSelect or ListText | corridor segment ids | scatter segment spans | `PlacementZoneSegments`, text | Partial | Replace comma/list text with multi-select. |
| `$.obstacles.placements[].zone.lanes` | 허용 영역 | Obstacle > 배치 규칙 N | array of string | MultiSelect or ListText | lane hints | scatter lane bands | `PlacementZoneLanes`, text | Partial | Replace comma/list text with multi-select. |
| `$.obstacles.placements[].palette` | 후보 팔레트 | Obstacle > 배치 규칙 N | object | Block | - | none | implicit rows | Container | Visible for scatter only. |
| `$.obstacles.placements[].palette.categories` | 후보 카테고리 | Obstacle > 배치 규칙 N | array of string | MultiSelect or ListText | prop categories | none | `PlacementPaletteCategories`, text | Partial | Needs category options from catalog. |
| `$.obstacles.placements[].palette.classes` | 후보 클래스 | Obstacle > 배치 규칙 N | array of string | MultiSelect or ListText | prop classes | none | `PlacementPaletteClasses`, text | Partial | Needs class options from catalog. |
| `$.obstacles.placements[].count` | 개수 | Obstacle > 배치 규칙 N | integer or range | RangeInteger | - | repeated preview count | `PlacementCount`, range | Live | Integer range UI currently shares generic range row. |
| `$.obstacles.placements[].spacing_m` | 간격 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | pattern spacing preview | `PlacementSpacing`, range | Live | Viewport pattern preview needed. |
| `$.obstacles.placements[].gap_width_m` | 통과 간격 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | gap preview | `PlacementGapWidth`, range | Live | Viewport gap preview needed. |
| `$.obstacles.placements[].density_per_10m` | 생성 밀도 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | scatter density preview | `PlacementDensity`, range | Live | Density preview can remain abstract initially. |
| `$.obstacles.placements[].yaw_deg` | 회전 | Obstacle > 배치 규칙 N | number or range | RangeNumber | - | obstacle orientation | `PlacementYaw`, range | Live | Degree unit label needed. |
| `$.obstacles.placements[].allow_blocking` | 통로 차단 허용 | Obstacle > 배치 규칙 N | boolean | Checkbox | - | clearance violation state | `PlacementAllowBlocking`, true/false Combo | Partial | C++ row now uses Combo; WBP checkbox asset/control can replace it later. |

## Pedestrians

| JSON path | 표시명 | Block | Schema type | 권장 control | Options source | Viewport | 현재 Sidebar | 상태 | 차이/다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$.pedestrians.background` | 배경 보행자 | Pedestrian > 배경 보행자 | object | Block | - | spawn previews | `BackgroundBlockWidget` | Container | Current edits are structure-only warning in panel. |
| `$.pedestrians.background.count` | 배경 보행자 수 | Pedestrian > 배경 보행자 | integer or range | RangeInteger | - | count preview | `BackgroundCount`, range | Partial | Row exists, but commit path is not implemented in panel yet. |
| `$.pedestrians.background.speed_mps` | 보행 속도 | Pedestrian > 배경 보행자 | number or range | RangeNumber | - | none | `BackgroundSpeed`, range | Partial | Row exists, but commit path is not implemented in panel yet. |
| `$.pedestrians.background.spawn_zone` | 스폰 구역 | Pedestrian > 스폰 구역 | object | Block | - | spawn zone overlay | `SpawnZoneBlockWidget` | Container | Segment restriction only. |
| `$.pedestrians.background.spawn_zone.segments` | 스폰 구간 | Pedestrian > 스폰 구역 | array of string | MultiSelect or ListText | corridor segment ids | spawn segment spans | `SpawnSegments`, text | Partial | Row exists, but commit path is not implemented; should become multi-select. |
| `$.pedestrians.encounters` | 상호작용 상황 | Pedestrian > 상호작용 상황 | array | ArrayBlock + count | - | encounter previews | `EncountersCount`, encounter widgets | Partial | Add/remove exists visually; commit support is incomplete for pedestrian panel. |
| `$.pedestrians.encounters[].id` | 상황 이름 | Pedestrian > 상호작용 N | string | Text | unique encounter ids | outliner/labels | `EncounterId`, text | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].type` | 상황 유형 | Pedestrian > 상호작용 N | string enum | Combo | `oncoming_pass`, `overtake`, `cross_path`, `standing_group` | controls visible fields | `EncounterType`, Combo | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].at` | 발생 구간 | Pedestrian > 상호작용 N | string | Combo | corridor segment ids | encounter position/span | `EncounterAtSegment`, Combo | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].persona` | 보행자 성향 | Pedestrian > 상호작용 N | string | Combo | `environment-catalog.md` Persona | behavior preview | `EncounterPersona`, Combo | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].meet_offset_m` | 만남 위치 보정 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | encounter meeting point | `EncounterMeetOffset`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].speed_mps` | 보행자 속도 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | none | none | Doc-only | Schema doc/catalog mention this for `overtake`; shared type/read/write do not preserve it. |
| `$.pedestrians.encounters[].trigger_distance_m` | 반응 시작 거리 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | trigger radius/line | none | Doc-only | Schema doc/catalog mention this for `cross_path`; shared type/read/write do not preserve it. |
| `$.pedestrians.encounters[].from` | 진입 방향 | Pedestrian > 상호작용 N | string | Combo | lane/side direction options TBD | approach arrow | none | Doc-only | Schema doc/catalog mention this for `cross_path`; shared type/read/write do not preserve it. |
| `$.pedestrians.encounters[].size` | 군집 크기 | Pedestrian > 상호작용 N | integer or range | RangeInteger | - | group footprint | none | Doc-only | Schema doc/catalog mention this for `standing_group`; shared type/read/write do not preserve it. |
| `$.pedestrians.encounters[].blocked_width_ratio` | 점유 폭 비율 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | blocked width overlay | none | Doc-only | Schema doc/catalog mention this for `standing_group`; shared type/read/write do not preserve it. |
| `$.pedestrians.encounters[].overrides` | 행동 보정 | Pedestrian > 상호작용 N | object | Block | - | none | implicit rows | Container | Override rows are flattened under encounter. |
| `$.pedestrians.encounters[].overrides.cooperation` | 양보 성향 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | none | `EncounterCooperation`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].overrides.evasiveness` | 회피 성향 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | none | `EncounterEvasiveness`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].overrides.personal_space_m` | 개인 공간 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | personal-space radius | `EncounterPersonalSpace`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].overrides.awareness_horizon_s` | 예측 시간 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | none | `EncounterAwarenessHorizon`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].overrides.max_yield_wait_s` | 대기 한계 시간 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | none | `EncounterMaxYieldWait`, range | Partial | Row exists; commit path not implemented. |
| `$.pedestrians.encounters[].overrides.sidestep_distance_m` | 회피 이동 거리 | Pedestrian > 상호작용 N | number or range | RangeNumber | - | sidestep distance guide | `EncounterSidestepDistance`, range | Partial | Row exists; commit path not implemented. |

## Robot

| JSON path | 표시명 | Block | Schema type | 권장 control | Options source | Viewport | 현재 Sidebar | 상태 | 차이/다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$.robot.start` | 시작 위치 | Main > 시작 위치 | object | Block | - | start marker | `RobotStartBlockWidget`, summary row | Live | C++ proxy/overlay marker follow-up can reuse this. |
| `$.robot.goal` | 목표 위치 | Main > 목표 위치 | object | Block | - | goal marker | `RobotGoalBlockWidget`, summary row | Live | C++ proxy/overlay marker follow-up can reuse this. |
| `$.robot.start.type`, `$.robot.goal.type` | 위치 기준 | Main > 시작/목표 위치 | string enum | Combo | `entry`, `exit`, `corridor_pose` | marker placement mode | `RobotStartType`, `RobotGoalType`, Combo | Live | Display/value mapping needed. |
| `$.robot.start.segment`, `$.robot.goal.segment` | 구간 | Main > 시작/목표 위치 | string | Combo | corridor segment ids | marker snapped segment | `RobotStartSegment`, `RobotGoalSegment`, Combo | Live | Visible only for `corridor_pose`; options come from current corridor. |
| `$.robot.start.along_m`, `$.robot.goal.along_m` | 진행 거리 | Main > 시작/목표 위치 | number | Number or RangeNumber | - | marker along handle | `RobotStartAlong`, `RobotGoalAlong`, range | Partial | Schema says number, implementation supports template number/range. Decide whether range should be allowed. |
| `$.robot.start.offset_m`, `$.robot.goal.offset_m` | 좌우 위치 | Main > 시작/목표 위치 | number | Number or RangeNumber | - | marker lateral handle | `RobotStartOffset`, `RobotGoalOffset`, range | Partial | Schema says number, implementation supports template number/range. Decide whether range should be allowed. |
| `$.robot.start.lane`, `$.robot.goal.lane` | 이동 영역 | Main > 시작/목표 위치 | string | Combo | lane hints | marker lane guide | `RobotStartLane`, `RobotGoalLane`, Combo | Live | Uses lane hint options with explicit unset. |
| `$.robot.start.heading`, `$.robot.goal.heading` | 진행 방향 | Main > 시작/목표 위치 | string enum | Combo | `forward`, `backward`, `auto` | marker orientation | `RobotStartHeading`, `RobotGoalHeading`, Combo | Live | Display/value mapping needed. |

## Cross-Cutting Authoring Types

| Schema shape | 표시명 | 적용 field | 권장 UX | 현재 구현 | 다음 작업 |
| --- | --- | --- | --- | --- | --- |
| `number` | 고정 숫자 | all `number or range` fields | single value input with unit suffix | `Range` row can disable min/max | Unit suffix and mode affordance needed. |
| `{ "min": n, "max": n }` | 범위 숫자 | `walkway_width_m`, widths, along/offset, behavior values | fixed/range segmented control + min/max fields | `Range` row exposes min/max when enabled | Add viewport min/max visualization. |
| `integer` / integer range | 고정/범위 정수 | counts, group size | integer stepper or range integer | generic `Range` row | Prevent fractional input and show count semantics. |
| `string` catalog id | 카탈로그 선택 | surface, prop, persona, segment refs | Combo/search picker with display name and raw id tooltip | single-select refs mostly Combo; prop/palette still Text | Catalog display labels and prop picker remain. |
| `{ "choices": [...] }` | 후보 선택 | `segments[].replaced_by` | multi-choice editor or "sampled from choices" mode | not fully surfaced | Needed before random-choice authoring feels complete. |
| `array<string>` | 다중 선택 | zones, palette filters, spawn segments | multi-select chips/list | free text | Replace with reusable list/multi-select row. |
| `boolean` | 켜기/끄기 | `allow_blocking` | Checkbox/toggle | true/false Combo | Replace with checkbox when WBP row asset supports it. |

## Priority Notes

1. **Closed-set controls first**: single-select enum/reference fields now use Combo where possible; remaining multi-select/list and checkbox-specific asset work is separate.
2. **Catalog-backed selectors next**: prop, palette filters, and display-name mapping should stop being raw id text.
3. **Random-value UX after that**: fixed/range and fixed/choices need an explicit mode affordance.
4. **Viewport range visualization**: corridor segment spans, walkway/lane width ranges, obstacle along/offset ranges, and clearance width are the highest-value visualizations.
5. **Pedestrian gap is structural**: schema docs mention encounter-specific fields that current shared types and JSON round-trip do not preserve. Do not add sidebar rows for those until the data model/read/write path is extended.
