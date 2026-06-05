# Delivery Bot Simulator Prototype

## Overview
- 주행 로봇은 시작 위치로부터 도착 위치까지 최적 경로 이동
- 돌발 이벤트: 장애물, 보행자, 신호 변경 등
- 실제 환경은 센서 관측 데이터 입력 (RGB, Depth, Semantic Segmentation 등)
- 행동 정책: Rule-based. 여러 Rule 동시 발생 가능, 이 때 센서 원본 대신 Aggregator 출력 평가로 우선순위 계산 후 선택 (Goal Vector, Obstacle Sector, Terrain Score, Robot State, Path Progress 등)
- AI-Agent 분석 데이터: 센서 출력, 평가 지표, 실패 케이스, near-miss 태그

> 센서 데이터 목록은 변동 가능

## Project Contracts
- Simulation authority: 최종 Dedicated Server, MVP는 Standalone PIE
- Server responsibilities: Delivery Bot 이동, 충돌 판정, 정책 평가, metric 기록
- Observer boundary: 관찰과 UI 명령 요청만 허용
- Policy input: 센서 원본이 아닌 Observation Aggregator 출력
- Sensor contract: 변동 가능 목록, Aggregator 출력 계약

## Source Mapping
| Area | Description |
| --- | --- |
| DeliveryBot | 주행 로봇 Actor/Component/Subsystem. 이동/경로 추종/회피/정책 판단 |
| Episode | JSON 에피소드 컴파일, 런타임 스폰/조회, 에피소드 Actor/Component, 실행/측정/평가 흐름 |
| Shared | Episode, Simulation, DeliveryBot 사이의 공유 타입, 실행 설정, 리플레이, 측정 로그, 시나리오 스펙 |
| Simulation | `-Simulate=<SimulationSetupFile>` 기반 시뮬레이터 부트스트랩, fixed-step 설정, 맵 로드, 상태/로그/리포트 연결 |

## Scripts
- `RunPreview.bat`: 패키징 프리뷰 (`UnrealEditor.exe <uproject> -game -NoSplash`)
  - `-Simulate=<SimulationSetupFile> -RunId=<RunId>` 로 시뮬레이터 실행
