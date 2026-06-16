# Sensor Captures

경로:

```text
<UserProject>/runs/<RunId>/episodes/<EpisodeId>/captures/
```

형식:

```text
image/data artifacts
```

상태: artifact 위치만 확정. capture index/manifest schema는 논의중이다.

## 합의

- episode 중 저장한 센서 데이터 파일을 둔다.
- `actions.jsonl`, `trace.jsonl`, `events.jsonl`에서 필요한 capture를 참조할 수 있어야 한다.
- 파일 수가 많을 수 있으므로 index/manifest 필요 여부를 이후 확정한다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| location | `episodes/<EpisodeId>/captures/` |
| content | sensor image 또는 sensor data artifact |
| reference | `run_time_seconds`, frame index, sensor id 등으로 조인 가능해야 함 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| directory layout | sensor별/시간별 폴더 구조 |
| capture manifest | 별도 manifest 파일 필요 여부 |
| file naming | sensor id, frame, timestamp 명명 규칙 |
