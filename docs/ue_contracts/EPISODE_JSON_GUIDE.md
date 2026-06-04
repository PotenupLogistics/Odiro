# 에피소드 JSON 양식 가이드

# 개요

이 페이지는 에피소드 작성 및 실행에 사용되는 JSON Input과 실행 후 생성되는 JSON Output 양식을 팀 공용으로 설명한다. LLM이 JSON을 생성하거나 평가 결과를 해석할 때도 이 구조를 기준으로 삼는다.

# 디렉토리 구조

| 구분 | 경로 | 포함 파일 | 목적 |
| --- | --- | --- | --- |
| Input | `Json/Input` | EpisodeSetup JSON, DeliveryBotSetup JSON, RunQueue JSON | 에피소드 배치와 로봇 튜닝값을 정의한다. |
| Output | `Json/Output` | EvaluationReport JSON | 실행 결과, 평가 지표, 주요 이벤트를 저장한다. |

# 실행 단위

EpisodeSetup JSON과 DeliveryBotSetup JSON은 항상 pair로 다룬다. 한 pair가 언리얼 시뮬레이션 레이어로 입력되면 언리얼에서 에피소드를 배치 및 실행하고 그 결과값을 EvaluationReport JSON으로 저장한다. (혹은 저장하지 않고 LLM에게 전달)

| JSON | 책임 |
| --- | --- |
| EpisodeSetup JSON | 에피소드 실행 정보, 지면 영역, 경로, 장애물, 보행자, 로봇 배치, 로봇 목적지 |
| DeliveryBotSetup JSON | DeliveryBot의 주행, 경로 추종, 라이다 반응, 정책 튜닝값 (추후 로봇 정책 파라미터들이 이 양식에 추가될 예정) |
| RunQueue JSON | 여러 pair를 순서대로 실행하기 위한 큐 (단일 실행일 경우 사용 안해도 됨) |
| EvaluationReport JSON | 한 번의 pair 실행 결과와 LLM이 해석할 주요 사건 목록 |

# 핵심 원칙

- LLM이 입력 JSON을 만들 때는 valid JSON object만 출력한다.
- EpisodeSetup은 meter와 degree를 사용한다. 컴파일러가 Unreal centimeter로 변환한다.
- EpisodeSetup의 actor 배치는 `xy_m`, `yaw_deg`만 사용한다. scale, pitch, roll, transform object는 입력하지 않는다.
- DeliveryBotSetup은 로봇 배치, instance ID, 목적지, run 정보를 갖지 않는다.
- RunQueue의 각 항목은 `episode_setup`과 `delivery_bot_setup`을 모두 가져야 한다.
- EvaluationReport는 LLM이 조정할 수 있는 episode behavior와 관련된 정보만 담는다. 정책 서버 통신 실패 같은 인프라 오류는 핵심 평가 데이터에서 제외한다.

# 하위 페이지

[Input - EpisodeSetup JSON](https://app.notion.com/p/Input-EpisodeSetup-JSON-3f2262ac9c88835caf0f014151a7c1be?pvs=21)

[Input - DeliveryBotSetup JSON](https://app.notion.com/p/Input-DeliveryBotSetup-JSON-2a3262ac9c88825ebe1f811d4e525538?pvs=21)

[Input - RunQueue JSON](https://app.notion.com/p/Input-RunQueue-JSON-ef0262ac9c88839a8616014d108054d2?pvs=21)

[Output - EvaluationReport JSON](https://app.notion.com/p/Output-EvaluationReport-JSON-9de262ac9c8882a4b8cb0184217ba645?pvs=21)