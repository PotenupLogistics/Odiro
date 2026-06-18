# 시뮬레이션 인터페이스

제품 개념과 실행 데이터 흐름을 정의한다.

## 핵심 용어

### 데이터 단위

| 용어              | 형식   | 정의                                    | 구성                                              |
| ----------------- | ------ | --------------------------------------- | ------------------------------------------------- |
| `Scenario`        | JSON   | 로봇이 수행할 작업과 환경 구성          | 시작/도착 위치, 도로 구성, 장애물 배치, 랜덤 요소 |
| `EpisodeScenario` | JSON   | `Scenario` + seed로 확정한 episode 입력 | 확정된 시작/도착 위치, 랜덤 요소 제거된 시나리오  |
| `Profile`         | JSON   | 로봇의 물리적 특성                      | 최대 속도, 토크, 무게, 센서 범위                  |
| `Setting`         | JSON   | 시뮬레이션 설정                         | 반복 횟수, base seed, FPS 등                      |
| `Policy`          | Python | 행동을 결정하는 알고리즘 또는 모델      | decision tree, custom policy                      |

### 실행 단위

| 용어                | 정의                                                                                      |
| ------------------- | ----------------------------------------------------------------------------------------- |
| `Project`           | 단일 `Scenario`, `Profile`, `Setting`, `Policy`와 결과 로그를 소유하는 사용자 데이터 단위 |
| `Run`(`Simulation`) | 한 `Project`를 실행한 단위. policy 변경 후 같은 project에서 새 run을 만들 수 있음         |
| `Episode`           | `Run` 내에서 episode scenario 하나가 시작부터 종료까지 실행되는 단위                      |

식별자:

- `RunId`는 Bridge가 할당하는 6자리 decimal string이다. 예: `000001`.
- `EpisodeId`는 `Run` 안에서 1-based로 증가하는 6자리 decimal string이다. 예: `000001`.
- 둘 다 path segment로 사용하므로 `..`, 경로 구분자, 임의 접두사/접미사를 허용하지 않는다.

### 로봇 구성

| 용어      | 정의                                               | 모방 대상               |
| --------- | -------------------------------------------------- | ----------------------- |
| 로봇      | 로봇의 물리적 특성과 동작을 모방한 액터            | 실제 로봇의 물리적 특성 |
| 지도 정보 | 로봇 주행 환경의 지리적 레이아웃                   | GPS 지도                |
| 인식 정보 | 로봇의 주변 환경 인식 데이터                       | 카메라, LIDAR, IMU 센서 |
| 행동      | 로봇이 수행하는 제어 동작                          | 실제 로봇 제어 방식     |
| 행동 정책 | 주어진 상황에서 행동을 결정하는 알고리즘 또는 모델 | 실제 제어 알고리즘      |

### 시뮬레이션 기록

| 용어      | 형식  | 정의                                   | 구성 요소                                 |
| --------- | ----- | -------------------------------------- | ----------------------------------------- |
| `actions` | JSONL | 로봇의 입력·출력 데이터                | 기록 시간, 입력 데이터, 출력 행동         |
| `events`  | JSONL | 발생한 사건 또는 변화                  | 발생 시간, 이벤트 유형, 관련 객체         |
| `trace`   | JSONL | 로봇과 무관한 환경 정보                | 기록 시간, 환경 변경, 로봇이 못 본 데이터 |
| `status`  | JSON  | 한 `Run`의 process 생명주기 상태       | process id, 상태, 시작/갱신/종료 시각     |
| `result`  | JSON  | 한 `Episode`의 결과                    | 총 실행 시간, 성공/실패, 충돌 횟수 등     |
| `summary` | JSON  | 한 `Run` 내의 모든 `Episode` 결과 요약 | 총 실행 시간, 성공률, 주요 실패 원인 등   |

### 평가

| 용어          | 정의                            | 예시                              |
| ------------- | ------------------------------- | --------------------------------- |
| 실패 상황     | 시나리오 실패 조건              | 보행자 충돌, 도로 이탈, 시간 초과 |
| 행동 평가     | 한 순간의 행동 적절성 평가      | 이상 행동 탐지                    |
| 정책 평가     | 행동 정책의 안정성 평가         | 취약 시나리오 탐지, 개선점 제안   |
| 시나리오 평가 | 시나리오의 난이도와 적합성 평가 | 정책 평가용 시나리오 식별         |

## 실행 흐름

1. Project 생성
2. Scenario 작성
   - ScenarioEditor 또는 ScenarioAgent가 project의 단일 `scenario.json` 생성/수정
   - 랜덤성이 필요한 값은 고정값 대신 range 또는 choices로 기록
3. Setting, Profile 작성
   - FPS, episode_count 등 시뮬레이션 설정은 설정 페이지에서 `setting.json` 생성/수정
   - 로봇 특성은 `profile.json`로 기록, 별도의 에디터로 프리뷰 제공할 수 있음
4. Policy 작성
   - `policy/` 디렉토리에 Python 파일로 작성, `PolicyRuntime` 인터페이스 준수
5. Simulation 실행
   1. Bridge가 6자리 decimal `RunId`를 할당하고 `runs/<RunId>/` 생성
   2. Run 시작 시 `setting.json`, `profile.json`, `scenario.json`, `policy/`를 snapshot
   3. Simulator는 실행 중 project root의 원본 입력을 다시 읽지 않고 snapshot만 사용
   4. Simulator 프로세스가 snapshot scenario와 고정 seed로 episode 수만큼 episode scenario 생성
   5. Simulator 프로세스가 snapshot `policy/__init__.py:create_policy`로 `PolicyRuntime`을 준비
   6. `PolicyRuntime` 준비가 끝난 뒤 episode scenario마다 Episode 생성
      1. 로봇은 입력 데이터를 `PolicyRuntime`에 전달, 행동 판단 수신
      2. 입출력, 이벤트, 환경 데이터 `Run` 로그에 기록
   7. 각 Episode 종료 시 결과 기록
   8. Run 종료 시 summary 기록
6. 시뮬레이션 결과 AI 분석
   1. Run summary를 분석하여 Run 간 차이 분석
   2. Episode result 분석하여 Episode 간 차이 분석
   3. 분석 결과를 바탕으로 Policy 개선 · Scenario 개선 방향 도출
