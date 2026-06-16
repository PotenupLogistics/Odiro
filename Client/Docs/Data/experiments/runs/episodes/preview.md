# Episode Preview

경로:

```text
<UserProject>/runs/<RunId>/episodes/<EpisodeId>/preview.png
```

형식:

```text
PNG artifact
```

상태: artifact 위치와 형식만 확정. event reference와 fallback rule은 논의중이다.

## 합의

- episode를 대표하는 이미지다.
- 실패, near-miss, 충돌 등 대표 event가 있으면 그 장면을 우선한다.
- AI review와 사람이 빠르게 결과를 훑는 용도로 사용한다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| file name | `preview.png` |
| format | PNG |
| source | episode runtime capture 또는 replay capture |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| resolution | 기본 해상도 |
| event reference | 어떤 event에서 생성했는지 기록 위치 |
| fallback rule | 대표 event가 없을 때 선택할 frame |
