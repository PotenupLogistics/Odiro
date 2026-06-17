# OdiroSim Docs

상태: Client 전용 구현 문서.

공유 사용자 project 계약:

- 폴더 구조: `docs/specs/project-structure.md`
- 실행 흐름: `docs/specs/simulation-interface.md`
- 파일 형식: `contracts/specs/user-project-data.md`
- Bridge IPC: `contracts/specs/bridge-ipc.md`

규칙:

- `Client/Docs/Data/**`는 제거됨.
- 새 schema, field, 저장 위치는 `contracts/specs/user-project-data.md`에 기록한다.
- Client 전용 구현 메모만 `Client/Docs/**`에 남긴다.

남은 용도:

- Unreal 구현 메모
- 기존 Client-local JSON 참고 문서
- 개발 중 검증·운영 절차
