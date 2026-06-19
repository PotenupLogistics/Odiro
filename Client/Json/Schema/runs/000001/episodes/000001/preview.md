# preview.png

Episode를 대표하는 PNG 이미지 artifact다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/preview.png
```

## 형식

```text
PNG file
```

## 사용 규칙

- 실패, near-miss, 충돌 등 대표 event 장면을 우선한다.
- AI review와 사람이 run 결과를 빠르게 훑을 때 사용한다.
- 해상도, event 참조 위치, fallback frame 규칙은 아직 별도 schema로 확정하지 않는다.
- JSON 원본 근거는 같은 episode의 `scenario.json`, `result.json`, `events.jsonl`이다.
