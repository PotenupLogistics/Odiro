# 프로젝트 규칙

구체적인 폴더 구조는 [프로젝트 구조](project-structure.md)를 따른다.

## 이름 규칙

### File

- Default: kebab-case
- 각 프로젝트는 자체 관례를 우선한다.
  - `Client`: Unreal 관례 우선. PascalCase, Unreal prefix 사용
  - `Agents`: Python 관례 우선. snake_case 사용
  - `Bridge`: Go 관례 우선. flatcase/snake_case 사용

예:

```text
docs/specs/project-rules.md
docs/plans/bridge-bootstrap.md
Client/Source/OdiroSim/ADeliveryBot.h
Client/Content/UI/WBP_MainMenu.uasset
```

### Module

서브 프로젝트는 PascalCase, 그 외는 kebab-case 기본 사용.

### Executable

Release executable 이름은 PascalCase 사용.

## Ownership 관리

- 프로젝트 전용 파일은 각 프로젝트 내부에 두고, 공유 및 통합 파일은 루트에 둔다.
- 둘 이상의 프로젝트에 같은 내용이 중복되어 있고 프로젝트 전체에 적용되는 개념이면 루트에 새 canonical 문서를 만들고 기존 파일은 사용자가 삭제하도록 유도한다.

### Client

Unreal 프로젝트, 전용 `Tools`, `Docs` 소유 가능.

- Controller, Simulator, Scenario Editor 제공
- Simulator는 Python Agent Server를 실행하고 IPC로 통신

Client 내부의 Python Runtime은 사용자의 Python Environment를 사용하고, `Agents`와 공유하지 않는다.

### Agents

Python Agent Server 프로젝트.

- 자연어 기반 시나리오 생성
- 정책 RAG 검색
- 시나리오 생성과 검증
- 시뮬레이션 결과 분석
- 행동 정책 개선안 제공 또는 자동 생성

### Bridge

Go 프로젝트. 백그라운드 host process를 제공한다.

- Client Controller용 local portless IPC 제공
- Client, Agents, Simulator workflow orchestration
- Simulator 프로세스 실행 및 상태 추적
- Agents 프로세스 실행 및 통신
- User Directory 읽기/쓰기 API 제공

Release에서는 host 단일 바이너리로 배포됨.

## Static Files

각 서브 프로젝트는 Release에서도 사용하는 정적 파일을 지정된 서브폴더에 둔다.

```text
Client/Static/
Agents/static/
```

> `Bridge`에 release static asset이 생기면 `Bridge/static/`에 두고 host binary에 embed한다.

## contracts

모든 프로젝트가 공유하는 인터페이스를 보관한다.

### 계약 대상

- 둘 이상의 프로젝트가 runtime, test, API에서 참조하는 규약
- 외부 사용자가 맞춰야 하는 파일 형식이나 API 규약

### 포함 대상

- JSON Schema
- 사람이 읽는 공유 contract spec
- IPC message schema
- shared example payload
- 컴포넌트 간 호환성 테스트 fixture

### 포함하지 않는 대상

- 사람이 읽는 제품 요구사항
- 프로젝트 별 내부 설계 문서 및 구현 계획
- 프로젝트 내부적으로만 사용하는 private schema

### Spec vs. Contract

- `contracts/specs`: payload, file format, API 등 공유 인터페이스의 사람이 읽는 기준
- `contracts/schemas`: 같은 계약의 machine-readable validation
- `contracts/examples`: 같은 계약의 example payload
- `docs/specs`: 프로젝트 구조, 규칙, 요구사항 등 repository/product 명세

## 문서

### Top-Level

- `docs/specs`: 현재 구조, 요구사항, 데이터 모델
- `docs/plans`: 전체 프로젝트 관련 변경 계획
- `docs/decisions`: 장기 의사결정
- `docs/guides`: 개발, 실행, 배포 가이드

### Agent Context

- `.agents/index`: agent-only source index for area entrypoints, ownership, boundaries, and verification

## 경로 처리

개발 환경과 배포 환경에서 파일 경로가 달라질 수 있으므로, 파일 입출력 시 실행 인자, 환경 변수, 설정 파일, 개발용 fallback 등으로 유연하게 처리해야 한다.

금지:
- 절대 경로 하드코딩
- `../..` 같은 상대 경로 사용
