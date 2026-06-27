# ADR-260628 Unified UE MCP Bridge

상태: Accepted

날짜: 2026-06-28

## Context

Unreal 작업에는 `UE_MCP_Bridge`와 `UmgMcp` 두 MCP surface가 병행 존재했다. `UE_MCP_Bridge`는 editor/runtime/build/log/capture 등 넓은 도구군을 제공했고, `UmgMcp`는 UMG active-target 편집, layout 검증, UMG animation, material/HLSL 편집 도구를 제공했다.

두 surface는 도구 이름 일부가 충돌했고, 55557 TCP endpoint와 Bridge endpoint를 함께 운용해야 해서 tool discovery, capture 선택, UMG 수정 검증 흐름이 반복적으로 헷갈렸다.

## Decision

외부 진입점은 `UE_MCP_Bridge` 하나로 통합한다.

- `Client/Plugins/UE_MCP_Bridge`가 survivor plugin이다.
- `UmgMcp` C++ 코드는 삭제하지 않고 survivor plugin 내부 `UmgMcp` module로 이동한다.
- `UmgMcp` module은 standalone TCP MCP server를 기본 시작하지 않는다.
- 기존 `UmgMcp` 도구명은 `UEMCP::RegisterExternalHandler`를 통해 `UE_MCP_Bridge` surface에 등록한다.
- `Client/OdiroSim.uproject`에는 `UE_MCP_Bridge` plugin entry만 남긴다.

## Collision Policy

충돌 이름은 compatibility dispatch를 사용한다.

- explicit `path`/`assetPath` 기반 호출은 기존 Bridge handler가 처리한다.
- old `UmgMcp` active-target 호출 형태는 migrated `UmgMcp` handler가 처리한다.
- 대표 충돌: `add_variable`, `compile_blueprint`, `delete_node`, `delete_variable`, `list_assets`, `save_asset`.

## Consequences

장점:

- MCP discovery와 사용 지침이 단일 surface로 단순해진다.
- Widget capture, runtime geometry, logs, build status, UMG writes를 같은 endpoint에서 조합할 수 있다.
- `UmgMcp` reflected symbols와 subsystem 이름을 유지해 asset/reflection migration risk를 줄인다.

Trade-offs:

- survivor plugin이 UMG tooling까지 소유하므로 `UE_MCP_Bridge` plugin blast radius가 커진다.
- old Python-only wrapper 동작은 C++ handler 변환으로 유지해야 한다.
- standalone `UmgMcpServer.py` 문서는 archive 참고자료로만 남고 active setup 문서에서는 제외된다.

## Verification

- `task-build.bat client`
- Unreal Editor log에 Bridge endpoint 하나만 표시되고, legacy 55557 listener startup이 없어야 한다.
- `UE_MCP_Bridge` surface에서 widget tools, capture/log tools, migrated UMG active-target tools를 모두 호출할 수 있어야 한다.
