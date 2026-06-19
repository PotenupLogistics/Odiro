# captures/

Episode 중 저장한 sensor image 또는 sensor data artifact를 담는 directory다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/captures/
```

## 형식

```text
image/data files
```

## 사용 규칙

- Capture file은 episode 원본 JSON이 아니라 보조 artifact다.
- `actions.jsonl`, `trace.jsonl`, `events.jsonl`에서 필요한 capture를 참조할 수 있다.
- Capture manifest, file naming, sensor별 directory layout은 아직 v1 schema로 확정하지 않는다.
- LLM은 capture 파일만으로 판단하지 말고 `summary.json`, `result.json`, `events.jsonl`, `scenario.json`을 함께 근거로 사용한다.
