# Base UI Components — Form Elements 구현 계획

대상: `Client/Source/OdiroSim` (UE5, CommonUI + MVVM). 디자인은 [spec.md](spec.md).
Codex ↔ Claude를 오가며 진행하므로, 이 문서가 단일 기준점이다.

## 목표
spec.md의 폼 요소를 기존 독립 base WBP 컴포넌트 패턴으로 구현한다.
신규 토큰/추상화 없이 `DA_BaseTokens` 위에서 재사용 가능한 위젯을 만든다.

## 접근
- 컴포넌트 = C++ `UBaseXxxWidget` + WBP, 서브위젯 `BindWidget`.
- 둥근 모서리는 `BaseWidgetPrivate::ApplyRoundedSurface`(SDF 머티리얼) 재사용.
- 색/치수/폰트/caret은 토큰·프로젝트 에셋에서 해석. spec.md 공통 규칙 준수.
- base 위젯은 presentational(세터 + 바인딩). 화면 결합은 앱 단 위젯에서.

## 배치 (PR 단위, 한 번에 하나)
배치마다 사이클: 구현 → 컴파일 → 에디터 비주얼 확인 → (가능하면) 스모크 테스트 → PR.

### Batch A — 입력
- `BaseTextInputWidget`: single / number(stepper) / number-range(단일 필드, `–`) / multiline.
  상태 default·hover·focused·error·disabled.
- `BaseSliderWidget`: 얇은 핸들, value+number field 동기화, range(dual handle)+paired
  number-range field(Text Input range와 동일 사양), disabled(필드 없음).
- `BaseSwitcherWidget`: segmented, 단일 active, 2/3분할·아이콘·disabled.
- `BaseDropdownWidget`: text-only / icon+text / open(hover+selected ✓) / disabled, caret 아이콘.

### Batch B — 선택
- `BaseToggleButtonWidget`: 버튼형(Off/On/Indeterminate/Disabled) + 스위치형(Off/On/Disabled).
- `BaseCheckBoxWidget`: unchecked/checked/disabled + 부모-자식 indeterminate, 그룹 컨테이너.

### Batch C — 표면 / 오버레이
- `BaseThumbnailCardWidget`: 4:3 media + 단일 content NamedSlot, 균일 10px 패딩,
  media full-bleed(기본)/inset(옵션), selected=accent border.
- `BaseTreeRowWidget` (+ 제네릭 tree view): 1줄 row, 슬롯 expander/icon/label+inline sublabel/right label.
  base는 row/슬롯까지; hierarchy·선택 상태는 Scenario Editor 도메인에서 소유.
- Tooltip: 흰 배경 + dark 텍스트, 커서 좌하단 앵커(우상단 전개). 공용 tooltip 위젯/스타일.
- `BaseContextMenuWidget`: 포인터 위치 top-left 시작, 임의 child NamedSlot 컨테이너,
  danger 항목·단축키·서브메뉴 caret.

## Codex 핸드오프 규칙 (필수)
시작 전 읽기: `AGENTS.md`, `.agents/index/cards/client-*.yaml`,
`.agents/skills/ue5-dev/SKILL.md`, 레퍼런스 위젯 `BaseButtonWidget.{h,cpp}`,
토큰/타입/헬퍼 헤더 (spec.md "참고 구현/검증 자산").

- 둥근 모서리는 `ApplyRoundedSurface`(SDF) 사용, 런타임에서 `FSlateRoundedBoxBrush` 금지.
- 색/폰트/간격/반경/높이는 `DA_BaseTokens`에서 해석, 하드코딩 hex 금지.
- `EBaseWidgetVariant/Size/State`, `EBaseTextRole` 재사용. 신규 enum은 불가피할 때만.
- 네이밍/구조는 기존 `UBaseXxxWidget`(+WBP, BindWidget) 답습. caret = `T_Icon_CaretDown/Right`.
- 주석: 모든 non-local 함수/멤버에 의도 주석(동작 나열 금지). 경계에서만 입력 검증.
- 커밋/푸시/제출 금지(명시 요청 시에만). 바이너리 에셋은 LFS lock-only, `filter=lfs` 추가 금지.
- 요청된 use case만 구현, speculative 옵션/추상화 금지.

## 검증 (배치별)
- `OdiroSim` 모듈 컴파일 통과.
- 에디터 기동 / PIE에서 위젯 렌더 확인(가능하면 캡처로 spec 이미지 육안 대조).
- 기존 `CaptureBaseFormElementsMockup`을 실제 위젯 캡처로 전환 시 시각 회귀 대조 가능.
- 가능하면 `PlatformUi*` 자동화 테스트 패턴 따라 위젯 스모크 테스트 추가.
- 변경 파일 / 검증 결과 / 잔여 리스크 보고.

## 미해결 / 결정 필요
- Thumbnail Card media 패딩 기본값: **full-bleed 권장** (확정 필요).
- caret Up/Left: 에셋 회전·반전 처리 vs `static` SVG에서 신규 임포트 (구현 시 택1).
- Tooltip/Context Menu의 포인터 앵커: UMG 기본 동작 활용 가능 범위 확인 후 커스텀 최소화.
