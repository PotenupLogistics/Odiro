# Map Generation Data Sources

## 1. 목적

사용자 자연어 기반 맵 생성 시 어떤 데이터를 근거로 JSON을 생성하는지 설명한다.

## 2. 입력 근거

* 사용자 자연어
* Scenario intent extraction
* EnvironmentSampling numeric parameters
* Policy RAG context
* WorldConfig schema / output contract
* UE EpisodeSpec guide
* deterministic placement rules

## 3. Policy RAG의 역할

법령/인증 문서는 좌표 생성 근거가 아니라 안전 정책 근거로 사용한다.

예:

* 보행자 안전
* 장애물 감지
* 감속/정지/회피/관제 요청 기준

## 4. 좌표 생성 근거

좌표는 아래 기준으로 결정된다.

* 사용자가 명시한 좌표
* robot.spawn / robot.goal
* route midpoint rule
* environmentSampling 수치
* placement validation rule

## 5. EpisodeSpec 변환 근거

UE EpisodeSpec JSON Guide를 기준으로 변환한다.

* WorldConfig cm -> EpisodeSpec m
* map -> ground_model.regions
* obstacles -> actors.static_obstacles
* pedestrians -> paths + actors.pedestrians
* robot -> actors.robot

## 6. 주의

LLM이 법령 RAG를 보고 좌표를 계산하는 것이 아니다.
좌표 계산과 배치 검증은 code-level deterministic rule로 보강한다.

## 7. Trace 제공

시나리오 생성 API는 `includeDiagnostics=true`일 때 `diagnostics.generationTrace`로 맵 생성 근거를 제공한다.

Trace는 아래 근거를 summary evidence로 남긴다.

* 사용자 자연어와 scenario intent
* environmentSampling numeric parameters
* route midpoint 같은 deterministic placement rule
* policy RAG safety context
* post-processing patch
* EpisodeSpec adapter의 cm -> m 변환
* validation / scenario reflection 결과

Trace에는 API key, rawContent, full WorldConfig, full EpisodeSpec을 저장하지 않는다.

## 8. 연구 정렬

`RESEARCH_ALIGNMENT.md`는 Eureka / DrEureka / Scenic의 원 개념과 현재 적용 범위를 분리해 설명한다.

* DrEureka는 현재 `environmentSampling`과 향후 환경 변수 범위 추천의 연구적 근거다.
* Scenic은 현재 runtime dependency가 아니라 Scenic-inspired placement and scenario sampling direction의 연구적 근거다.
* 현재 프로젝트는 Scenic DSL이 아니라 WorldConfig/EpisodeSpec 기반이다.
