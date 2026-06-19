# trace.jsonl

Robot이 직접 관측하거나 결정한 정보가 아닌 runtime world state를 replay, debugging, post-run analysis용으로 기록하는 JSON Lines 파일이다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/trace.jsonl
```

## line schema

```json
"episode_trace"
```

## Line Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `episode_trace`. |
| `version` | number | 예 | 고정값 `1`. |
| `sample_index` | number | 예 | 0-based trace sample index. |
| `run_time_seconds` | number | 예 | Episode 실행 timestamp. |
| `delta_seconds` | number | 예 | Previous sample과의 frame delta. |
| `robot` | object | 예 | Robot ground-truth state. |
| `actors` | array | 예 | Robot 외 actor ground-truth state 목록. |

## robot

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Robot id. |
| `position_cm` | array | 예 | World position `[x,y,z]` in centimeters. |
| `rotation_quat_xyzw` | array | 예 | World rotation quaternion `[x,y,z,w]`. |
| `velocity_cm_per_s` | array | 예 | World velocity `[x,y,z]` in centimeters per second. |

## actors[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `actor_index` | number | 예 | Trace line 안의 actor index. |
| `position_cm` | array | 예 | World position `[x,y,z]` in centimeters. |
| `rotation_quat_xyzw` | array | 예 | World rotation quaternion `[x,y,z,w]`. |
| `velocity_cm_per_s` | array | 예 | World velocity `[x,y,z]` in centimeters per second. |

## Join Rules

- `actions.jsonl`과 `events.jsonl`은 `run_time_seconds`로 trace와 조인한다.
- `sequence`는 action/event 조인 전용이며 trace join key가 아니다.
- Robot observation 원본은 `actions.jsonl`에 있다.
