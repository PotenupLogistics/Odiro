# Natural Language Input Plan

## API Shell Status

Natural-language input can now be accepted through the following API endpoints:

```text
POST /api/v1/generation/world-config/prompt-package
POST /api/v1/generation/world-config
POST /api/v1/scenarios/generate
```

`prompt-package` endpoint는 prompt package만 반환하며 외부 LLM을 호출하지 않는다. `generation/world-config` endpoint는 provider 설정에 따라 WorldConfig generation을 수행할 수 있다. 사용자용 `scenarios/generate` endpoint는 자연어 `prompt`를 필수로 받고 선택적으로 `episode_count`를 허용하며, wrapper 없는 RunQueue JSON을 반환한다.

## Retrieval Connection

Natural-language World Config generation will use the deterministic policy RAG retrieval layer before any embedding or vector DB integration.

The generator can map extracted scenario terms to retrieval filters:

- Pedestrian, sidewalk, or crosswalk terms can retrieve `sidewalk_operation` and `speed_policy` chunks.
- Obstacle or blocked-path terms can retrieve `perception_requirement` chunks.
- Stop, risk, or collision terms can retrieve `emergency_stop` chunks.
- Operator or remote-control terms can retrieve `operator_control` chunks.

Retrieved chunks are context for scenario constraints only. They do not change source review status, manual confirmation status, or policy cards.

## 1. 목적

사용자가 자연어로 배달 로봇 시뮬레이션 환경을 설명하면, AI가 World Config JSON을 생성하고 validation layer로 검증한 뒤 UE5에 전달할 수 있도록 입력 방식을 정의한다.

## 2. 자연어 입력의 MVP 범위

MVP에서는 자연어 입력을 World Config JSON 생성에만 사용한다.

아래 항목은 MVP 범위에서 제외한다.

- 자연어로 Policy Config 직접 생성
- 자연어로 Evaluation Spec 직접 생성
- 자연어로 Decision Response 직접 생성
- 자연어로 UE5를 직접 제어

## 3. 입력 채널

현재 구현:

- API 기반 입력 구현 완료
- Swagger/Postman 또는 간단한 UI에서 prompt 입력 가능
- `/api/v1/scenarios/generate`는 사용자용 natural-language RunQueue 생성 entrypoint
- `episode_count`가 없으면 `SCENARIO_EPISODE_DEFAULT_COUNT`를 사용
- CLI는 주로 JSON 검증과 export tooling 용도로 사용

후속 단계:

- 웹 UI 또는 UE5 에디터 패널에서 자연어 입력 가능

## 4. 입력 예시

예시는 JSON 파일로 만들지 않고 문서 안에 텍스트로만 작성한다.

예: "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."

## 5. 자연어 입력에서 허용할 정보

- 보도 폭
- 보도 길이
- 보행자 수
- 보행자 이동 방향
- 장애물 종류
- 장애물 위치
- 신호등 유무
- 경사/턱 여부
- 날씨/조명은 후순위

## 6. 자연어 입력에서 허용하지 않을 정보

- 정책 근거 문서 임의 해석
- 인증 준수 보장
- 실제 안전 보장 표현
- UE5 actor 코드 직접 생성
- 로봇의 실시간 주행 제어 명령
