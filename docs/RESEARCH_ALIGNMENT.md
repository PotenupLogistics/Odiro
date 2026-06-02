# Research Alignment

## 1. 목적

이 문서는 Eureka / DrEureka / Scenic 원 개념과 현재 Proto-AI 프로젝트의 적용 범위를 비교해 정리한다.

중요한 경계:

* 현재 프로젝트는 DrEureka 전체 구현이 아니다.
* 현재 프로젝트의 environmentSampling은 DrEureka-inspired environment sampling으로 표현한다.
* Eureka는 향후 결과 분석/정책 제안 에이전트의 아이디어 근거로 참고한다.
* DrEureka는 현재 environmentSampling 및 향후 환경 변수 범위 추천의 아이디어 근거로 참고한다.
* Scenic은 현재 runtime dependency가 아니라 Scenic-inspired placement and scenario sampling direction의 연구 근거로 참고한다.

## 2. 보관한 원 논문

* `docs/EUREKA.pdf`
  * Eureka: Human-Level Reward Design via Coding Large Language Models
  * arXiv: https://arxiv.org/abs/2310.12931
* `docs/DREUREKA.pdf`
  * DrEureka: Language Model Guided Sim-To-Real Transfer
  * arXiv: https://arxiv.org/abs/2406.01967
* Scenic
  * Probabilistic scenario language / scenario modeling system
  * Website: https://scenic-lang.org/

## 3. Eureka 원 논문 핵심 개념

Eureka는 LLM을 사용해 강화학습 reward function을 자동으로 작성하고 개선하는 연구다.

핵심 흐름:

1. 환경 코드와 task description을 LLM에 제공한다.
2. LLM이 executable reward code를 생성한다.
3. RL 학습 결과와 feedback을 바탕으로 reward를 반복 개선한다.
4. 사람이 직접 reward를 설계하는 부담을 줄이고, 여러 task에서 reward design을 자동화한다.

현재 프로젝트와의 관계:

* 현재 프로젝트는 reward function을 자동 생성하지 않는다.
* 현재 프로젝트는 RL 학습 loop를 실행하지 않는다.
* Eureka는 향후 Result Analysis Agent의 아이디어 근거로 둔다.
* 특히 UE 실행 결과를 분석해 실패 원인, 관련 policy parameter, 다음 실험 제안을 생성하는 구조에 참고할 수 있다.

## 4. DrEureka 원 논문 핵심 개념

DrEureka는 Eureka 계열 아이디어를 sim-to-real transfer로 확장한 연구다.

핵심 흐름:

1. LLM이 task와 환경 정보를 바탕으로 reward function을 생성한다.
2. LLM이 domain randomization configuration을 제안한다.
3. 시뮬레이션에서 학습된 policy가 실제 로봇 환경으로 옮겨갈 수 있도록 환경 변수 범위를 조정한다.
4. reward design과 domain randomization 설계를 함께 다룬다.

현재 프로젝트와의 관계:

* 현재 프로젝트는 DrEureka 전체 구현이 아니다.
* 현재 프로젝트는 sim-to-real policy learning을 수행하지 않는다.
* 현재 프로젝트는 LLM이 reward code나 domain randomization config를 자동 생성하는 구조가 아니다.
* 현재 구현된 것은 DrEureka-inspired environment sampling이다.
* 즉, seed / scenarioType / fixedParameters를 기반으로 보도 폭, 장애물 차단 비율, 제한 시간 같은 numeric environment parameter를 deterministic하게 고정하고 EpisodeSpec handoff까지 반영한다.

## 5. Scenic 원 개념

Scenic은 probabilistic scenario specification language다.

핵심 개념:

* 일부 조건만 명세하고, 나머지 위치/각도/객체 속성은 분포와 constraints를 통해 sampling한다.
* autonomous driving / robotics / CPS testing에서 scenario generation에 사용된다.
* hard/soft constraints를 이용해 유효한 scene을 샘플링한다.
* 여러 simulator와 연결될 수 있는 scenario modeling 방향을 제공한다.

현재 프로젝트와의 관계:

* 현재 프로젝트는 Scenic 전체 구현이 아니다.
* 현재 프로젝트는 Scenic DSL을 사용하고 있지 않다.
* 현재 프로젝트에 Scenic runtime dependency를 추가하지 않았다.
* 현재 프로젝트는 Scenic-inspired placement and scenario sampling direction을 일부 참고한다.
* 현재 구현은 자연어 -> WorldConfig -> EpisodeSpec 변환 구조다.
* 향후 Scenic-like intermediate spec 또는 placement rule layer를 추가할 수 있다.

현재 적용된 Scenic-like 요소:

* 자연어에서 scenario intent 추출
* route midpoint 같은 deterministic placement rule
* environmentSampling으로 수치 파라미터 선택
* EpisodeSpec validation / scenario reflection
* placement validation rules 예정

향후 확장 가능성:

* Scenic-style constraint spec
* scenario distributions
* multiple concrete scene sampling
* invalid placement rejection
* UE EpisodeSpec으로 변환 가능한 Scenic-like intermediate representation

## 6. 현재 프로젝트 적용 범위

현재 Proto-AI의 구현 범위:

* 자연어 prompt 기반 scenario intent extraction
* OpenAI-first WorldConfig generation
* environmentSampling numeric constraints
* deterministic post-processing
* route midpoint placement rule
* WorldConfig -> EpisodeSpec adapter
* EpisodeSpec validation
* EpisodeSpec scenario reflection
* UE handoff response
* diagnostics.generationTrace를 통한 생성 근거 요약

현재 구현된 environmentSampling 예:

* sidewalkWidthCm=120
* obstacleBlockingRatio=0.6
* timeLimitSec=60
* scenarioType=obstacle_ahead
* seed=1001

이 값들은 WorldConfig와 EpisodeSpec에 반영된다.

* `sidewalkWidthCm=120` -> `EpisodeSpec.ground_model.regions[].shape.size_m[1]=1.2`
* `obstacleBlockingRatio=0.6` -> `actors.static_obstacles[].properties.blocking_ratio=0.6`
* `timeLimitSec=60` -> `run.time_limit_s=60.0`

## 7. 비교표

| 항목 | Eureka | DrEureka | 현재 Proto-AI |
| --- | --- | --- | --- |
| 주요 목적 | LLM 기반 reward design | LLM 기반 reward + domain randomization for sim-to-real | 자연어 기반 UE EpisodeSpec scenario generation |
| LLM 역할 | reward code 작성/개선 | reward 및 domain randomization 설정 제안 | WorldConfig 생성, prompt 기반 scenario generation |
| RL 학습 loop | 있음 | 있음 | 없음 |
| reward function 생성 | 있음 | 있음 | 없음 |
| domain randomization 자동 설계 | 없음 또는 중심 아님 | 있음 | 아직 없음 |
| environment parameter sampling | 중심 아님 | domain randomization과 관련 | DrEureka-inspired deterministic environment sampling |
| sim-to-real transfer | 직접 목적 아님 | 핵심 목적 | 아직 UE simulation handoff 단계 |
| 현재 적용 표현 | 향후 Result Analysis Agent 아이디어 근거 | environmentSampling / 향후 환경 변수 범위 추천 근거 | 구현 대상 |

## 8. Scenic mapping table

| Research concept | Original role | Proto-AI current status | Notes |
| --- | --- | --- | --- |
| Scenic scenario language | Probabilistic scenario specification and scene sampling | Not directly implemented | Used as inspiration for placement rules and future scenario sampling |
| Scenic constraints | Hard/soft constraints over scene validity | Partially planned | Related to placement validation rules |
| Scenic sampling | Generate multiple concrete scenes from distributions | Not implemented | Future batch/scenario matrix candidate |

## 9. 향후 적용 가능성

Eureka-inspired Result Analysis Agent:

* UE Run Result를 입력으로 받는다.
* collisionCount, nearMissCount, minDistanceCm, deliveryTimeSec, stopCount, rerouteCount 등을 분석한다.
* 실패 원인과 관련 policy parameter를 추출한다.
* 다음 정책 조정 또는 다음 scenario 제안을 생성한다.

DrEureka-inspired environment sampling 확장:

* 현재는 fixed candidate 기반 deterministic sampling이다.
* 향후 UE 실행 결과를 기반으로 환경 변수 범위를 추천할 수 있다.
* 예: obstacleBlockingRatio 범위 확대, sidewalkWidthCm 난이도 조정, pedestrianCount 증가, timeLimitSec 조정.
* 단, 이것은 DrEureka 전체 구현이 아니라 DrEureka-inspired 환경 변수 추천/샘플링 확장이다.

Scenic-inspired placement and scenario sampling direction:

* 현재는 deterministic route midpoint와 environmentSampling 중심이다.
* 향후 placement constraints와 distributions를 분리한 intermediate representation을 정의할 수 있다.
* invalid scene rejection과 multiple concrete scene sampling을 UE 단일 케이스 검증 이후 검토할 수 있다.
* 단, 이것은 Scenic DSL runtime 도입이 아니라 Scenic-inspired 설계 방향이다.

## 10. 현재 명시적으로 하지 않는 것

현재 프로젝트는 아래를 수행하지 않는다.

* DrEureka 전체 구현
* Eureka 전체 구현
* RL reward function 자동 생성
* RL policy training
* sim-to-real transfer 실험
* domain randomization policy 자동 생성
* Scenic DSL runtime 사용
* Scenic 패키지 dependency 도입
* Scenic 전체 구현
* DOE matrix generation
* batch scenario generation

## 11. 설명 문장 가이드

사용하기 좋은 표현:

* 현재 프로젝트는 DrEureka-inspired environment sampling을 사용한다.
* Eureka는 향후 Result Analysis Agent와 policy recommendation 구조의 아이디어 근거다.
* DrEureka는 environmentSampling과 향후 환경 변수 범위 추천의 연구적 근거다.
* Scenic은 향후 Scenic-inspired placement and scenario sampling direction의 연구적 근거다.
* 현재는 Scenic DSL이 아니라 WorldConfig/EpisodeSpec 기반이다.
* 현재 구현은 UE EpisodeSpec scenario generation과 handoff까지다.

피해야 할 표현:

* 현재 프로젝트를 DrEureka 전체 구현으로 단정하는 표현
* 현재 프로젝트를 Eureka 전체 구현으로 단정하는 표현
* 현재 프로젝트가 Scenic을 사용 중이라고 단정하는 표현
* 현재 프로젝트가 Scenic DSL runtime을 도입했다고 말하는 표현
* 현재 프로젝트가 sim-to-real transfer를 완료했다는 표현
* 현재 프로젝트가 reward design 자동화를 구현했다는 표현
