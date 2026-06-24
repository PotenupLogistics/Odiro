# preview.png

Episode 폴더에 선택적으로 둘 수 있는 PNG 이미지 artifact다. 현재 클라이언트 Runner는 이 파일을 자동 생성하지 않는다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/preview.png
```

## 형식

```text
PNG file
```

## 사용 규칙

- 기존 파일이나 외부 도구가 생성한 파일이 있으면 UI가 선택적으로 사용할 수 있다.
- AI review와 사람이 run 결과를 빠르게 훑을 때 사용한다.
- 실패, near-miss, 충돌 등 대표 event 장면은 별도 artifact가 필요할 때 따로 설계한다.
- JSON 원본 근거는 같은 episode의 `scenario.json`, `result.json`, `events.jsonl`이다.
