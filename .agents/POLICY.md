# Policy

## Style
- Tone: concise, factual, command-oriented; paths, commands, errors, verification, next actions
- Preserve: identifiers, API names, errors, commands, paths, quoted text
- Report: 결론, 확인한 파일, 추천 또는 변경 내용, 검증 상태, 남은 리스크
- Markdown: no single-clause bullet periods; inline code for paths/symbols/commands; tables for mappings; fenced code with language
- Korean endings: avoid `~함`, `~됨`, `~한다`, `~이다`; prefer `Label: value`
- Uncertainty: 추측 표시

## Code Boundary
- Default response: code-based recommendation
- Code edits: explicit implementation request only

## Architecture Guidance
- Structure: 기존 구조 우선, 대규모 리팩토링은 마지막 선택지
- Boundaries: Actor/Component/Subsystem 책임 분리, Component/Interface 기반 이동 정책
- Runtime: Tick 최소화, 이벤트 기반 우선, 런타임 객체 소유권과 생명주기 설명
- Blueprint: tunable 값은 `UPROPERTY(EditAnywhere)`, `BlueprintReadOnly` 등으로 노출
- Code: 의도 기반 접근제한자, all-public 금지, Null 체크, 한국어 주석 예시

## Naming
- Class/member/function: 대문자 시작, `class Apple`, `void SetDead()`, `float Hp{ 0.f }`
- Parameter/local: 소문자 시작, camel case, `float hp`, `float damageValue`
- Bool: variable `bDead`, function `IsDead()`

## Artifacts
- Plan files: `Docs/plans/PLAN-NNNN-<type>-<title>.md`
- Decision records: `Docs/decisions/ADR-NNNN-<title>.md`
