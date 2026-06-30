# Base UI Components — Form Elements 스펙

Common Base UI Component에 추가할 폼 요소 디자인 스펙. 기존 독립 base WBP 컴포넌트
(`BaseButtonWidget`, `BaseCardWidget` 등)와 동일한 시각 언어를 따른다.

상태: BaseWidget responsive sizing / split token DA / 피드백 반영 중. 계획은 [plan.md](plan.md) 참조.

## 레퍼런스 이미지
- Inputs & selection controls — [SVG](images/inputs-selection-controls.svg) · [PNG](images/inputs-selection-controls.png)
- Data surfaces & overlays — [SVG](images/surfaces-overlays.svg) · [PNG](images/surfaces-overlays.png)

> 이미지의 hex/픽셀값은 토큰 검증용 참고치. 구현은 항상 토큰에서 해석한 값을 쓴다.

## 범위
- 추가: Text Input, Slider, Slider Combo, Switcher, Dropdown, Toggle Button, Checkbox, Thumbnail Card,
  Tree View, Tooltip, Context Menu.
- 비목표: 화면 단위 기능 결합, speculative 옵션/추상화, C++ 주도 visual layout/animation.

## 공통 규칙 (모든 컴포넌트 적용)
- 토큰 출처: color는 `UBaseWidgetColorCatalog` → `/Game/Widgets/Common/DA_BaseColors`,
  size/typography는 `UBaseWidgetSizeCatalog` → `/Game/Widgets/Common/DA_MediumSizes`
  기본값을 사용한다. 필요 시 위젯별 `ColorsOverride` / `SizesOverride`로
  `/Game/Widgets/Common/DA_SmallSizes`, `/Game/Widgets/Common/DA_LargeSizes`, 또는
  프로젝트별 size DA를 지정한다.
- split 이전 combined token catalog와 해당 DataAsset은 제거한다.
  신규/수정 위젯은 split DA 경로만 사용하고, fallback은 WBP-authored 값을 덮어쓰지 않는다.
- Small/Medium/Large authored 값은 각 `DA_*Sizes` asset이 소유해 DPI 보정 로직에 끌려가지 않는다.
- `UBaseWidgetSizeCatalog` CDO는 neutral 값만 가진다. C++ fallback/default size를 두면
  DataAsset delta serialization과 hot reload 캐시가 authored size 값을 가릴 수 있으므로 금지한다.
- Base UI token의 size/font 값은 authored logical UMG unit이다. HiDPI 대응은 DA 값을
  역보정하지 않고 `DisplayDpiScalingRule`이 현재 game/display DPI scale을 UMG custom
  scaling rule로 전달해 Slate / `SGameLayerManager` 경로에서 처리한다.
- Project font display DPI는 `FontDPIPreset=Unreal` / `FontDPI=96`을 사용한다.
  Unreal Details 패널은 `FSlateFontInfo.Size`를 `NativeSize * 96 / FontDPI`로 표시하므로,
  `72 DPI (Standard)`를 쓰면 DA native `16`이 Details에서 `21.33`으로 보인다.
- Widget Blueprint는 layout, style default, hover/focus/animation을 소유한다. C++은 token 해석,
  normalize/helper, API 세터, 런타임 생성/상태 동기화만 담당한다.
- 위젯은 `Tick` / `NativeTick`를 사용하지 않는다. 상태 변화는 setter, delegate, CommonUI/UMG
  이벤트, 제공 library path로 갱신한다.
- 둥근 모서리: `BaseWidgetPrivate::ApplyRoundedSurface(BorderFrame, SurfaceBorder, ...)`
  (SDF UI 머티리얼) 경로 사용. 런타임 위젯에서 `FSlateRoundedBoxBrush` 금지(흐릿함).
  참고: base 위젯의 둥근 처리 방식은 analytic SDF 머티리얼(`fwidth` 기반).
- enum 재사용: `EBaseWidgetVariant / EBaseWidgetSize / EBaseWidgetState / EBaseTextRole`.
  신규 enum은 불가피할 때만(예: 카드 미디어 패딩 모드).
- 폰트: Freesentation (`/Game/Fonts/Freesentation/Freesentation`) + Typeface weight option,
  역할은 `EBaseTextRole` 토큰 사용.
- caret 아이콘: `/Game/Textures/Icon/T_Icon_CaretDown`, `T_Icon_CaretRight`.
  - UE 에셋은 Down/Right 2종만 존재. Up = Down 180° 회전, Left = Right 좌우 반전으로 처리하거나
    `static/assets/icons/svg/caret-up.svg`, `caret-left.svg`에서 신규 임포트.
  - 크기는 셀 높이의 약 절반(시각폭 ≈ 8px)으로, 컨테이너 중앙 정렬.
- 구조: C++ `UBaseXxxWidget` + WBP, 서브위젯은 `UPROPERTY(meta=(BindWidget))`.
  CommonUI 기반 유지, MVVM은 기존 base 위젯처럼 presentational(세터 + 바인딩 가능) 수준.

## Responsive sizing contract
- Base Widget은 부모 child로 배치될 때 부모 슬롯의 `Padding`, `HAlign/VAlign`, Fill/Auto 규칙을 따른다.
- 부모 슬롯 layout 값은 부모 WBP/화면이 소유한다. Base Widget C++는 부모 슬롯 padding/align을 설정하지 않는다.
- `FBaseWidgetSizeConstraints`는 `RootSize` / `RootSizeBox`에 min/max desired-size 제약을 적용하는
  선택 API다. C++은 responsive child layout을 위해 root `WidthOverride` / `HeightOverride`를 항상 지운다.
- `MinWidth`, `MinHeight`, `MaxWidth`, `MaxHeight` 값 `0`은 해당 desired-size constraint를 clear한다.
  음수는 `0`으로 정규화하고, min > max는 순서를 교정한다.
- 기본 크기가 필요한 컴포넌트는 outer root가 아니라 내부 surface/control wrapper의 desired size나
  의미상 고정 부품(아이콘, checkbox mark, switch track/thumb)에만 크기를 둔다.
- 모든 BaseWidget WBP는 Designer에서 Desired 모드로 보일 수 있도록 outer root가 fixed size에
  의존하지 않는 구조를 유지한다. Draw Call을 늘리는 불필요한 wrapper/중첩 card 구조는 피한다.

## 핵심 토큰값

### Color (`DA_BaseColors`)
| 항목 | 값 |
|---|---|
| Accent | `#0070E0` (hover `#2589F5`, active `#0059C2`) |
| Surface well(입력면) | `#0F0F0F` (`SurfaceWellColor`) |
| Surface panel / chrome | `#242424` / `#151515` |
| Field border / hover / inset | `#3A3A3A` / `#5E5E5E` / `#4A4A4A` |
| Text primary / secondary / label / faint | `#CFCFCF` / `#A8A8A8` / `#8C8C8C` / `#5E5E5E` |
| Status success / warn / danger / info | `#4CAF50` / `#E0A030` / `#E5534B` / `#4A9FF5` |
| Disabled surface / border | `#151515` / `#262626`, text faint |

### Size Presets
| 항목 | Small | Medium | Large |
|---|---:|---:|---:|
| Title / Label / Body / Caption / Value font | 14 / 12 / 12 / 11 / 22 | 16 / 14 / 14 / 12 / 28 | 18 / 16 / 16 / 14 / 36 |
| Space1 / 2 / 3 / 4 / 5 / 6 | 2 / 4 / 6 / 8 / 10 / 12 | 2 / 4 / 6 / 8 / 10 / 12 | 3 / 6 / 8 / 10 / 12 / 16 |
| Space8 / 10 / 12 / 16 / 20 | 16 / 20 / 24 / 36 / 40 | 16 / 20 / 24 / 36 / 40 | 20 / 24 / 30 / 40 / 48 |
| Spacing small / medium / large | 4 / 8 / 12 | 4 / 8 / 12 | 6 / 10 / 16 |
| Control / small control / field | 28 / 26 / 22 | 30 / 28 / 24 | 36 / 32 / 30 |
| Row / property row / title bar / tab bar / panel header | 22 / 24 / 30 / 34 / 28 | 24 / 26 / 32 / 36 / 30 | 30 / 32 / 38 / 42 / 36 |
| Icon | 18 | 20 | 24 |
| Radius / Border / Elevation | 기존 값 유지 | 기존 값 유지 | 기존 값 유지 |

## 컴포넌트 스펙

### Text Input
- 변형: text / number / number range.
- 상태: default(border `#3A3A3A`) · hover(`#5E5E5E`) · focused(accent border + 텍스트 caret)
  · error(danger border + 하단 메시지) · disabled.
- Text Wrap off: single-line field + scrollbar path. Text Wrap on: multiline field + automatic wrapping,
  입력 내용은 위쪽 정렬.
- Number: 우측 stepper 컬럼(세로 구분선 + 가로 구분선), up/down caret 아이콘 상하 정렬.
- Number range: **단일 테두리 input** 한 개로 표시, 두 값 사이 구분자 `–`(en dash).
  Slider의 paired range field와 **동일 사양**(폭/내부 정렬 일치).

### Slider
- 핸들은 얇은 바(≈4px).
- 변형: single value · range(dual handle) · disabled.
- 값 입력 필드는 포함하지 않는다. label/slider/input 조합은 `BaseSliderComboWidget`이 소유한다.

### Slider Combo
- `BaseSliderWidget` + `BaseTextInputWidget` composition.
- 옵션: Show Label, Show Value Field, Range Mode.
- Visual Style: Compact = Label-Slider-Input 한 줄. Modern = 상단 Label/Input + 하단 Slider.
- input ↔ slider 값은 delegate 기반으로 동기화한다.

### Switcher
- segmented control, 단일 active(accent inset). 2/3분할, 아이콘 세그먼트, disabled.
- 그룹으로 묶인 토글의 집합 표현(switch element 묶음).

### Dropdown
- 변형: text-only · icon + text · open(hover 행 + selected 행 ✓) · disabled.
- 닫힘 caret = down, 열림 caret = up. 선택 항목은 accent 텍스트 + check.

### Toggle Button
- 버튼형: Off · On(accent) · Indeterminate(neutral + accent border) · Disabled.
- 스위치형: Off · On · Disabled (boolean 설정용 대안 표현).

### Checkbox
- 단독 상태: unchecked · checked(accent + check) · disabled.
- indeterminate는 **자식 체크박스가 있을 때만** 의미. 부모-자식 규칙:
  - 일부 자식 checked → 부모 indeterminate(accent + dash)
  - 모든 자식 checked → 부모 checked
  - 자식 없음 → 부모 unchecked
- 그룹 컨테이너 제공.

### Thumbnail Card (container)
- 4:3 media + **단일 content slot**(NamedSlot). title/subtitle/status/time 등 모두 슬롯 내부.
- 패딩 규칙: 상수 1개(10px) 균일 적용. content slot은 남는 세로 공간을 채우고
  하단 패딩은 항상 동일.
- media 패딩 모드: **full-bleed(기본, 카드 가장자리까지)** / inset(옵션, 사방 균일 패딩).
- selected 상태 = accent border.
- 공용 골격(media + slot)만 base로 제공. 구체 카드(Recent/Preset/Episode Result)는
  슬롯을 채워 앱 단에서 구성.

### Tree View
- 1줄 row. 슬롯: expander(caret, 자식 있을 때 자동) · icon(texture/glyph + tint)
  · label + **inline sublabel**(같은 줄, muted) · right-aligned label.
- 각 슬롯 optional. 제네릭 item view model 기반(데이터 구동).
- 선택 행 = panel 배경 + accent 좌측 바.
- base로는 row 비주얼/슬롯까지. hierarchy/선택 상태는 Scenario Editor 도메인 위젯에서 소유.

### Tooltip
- **흰 배경 + `#1A1A1A` 텍스트**.
- 일반 사용법: 대상 위젯의 Details > Behavior > Tool Tip Widget에 `WBP_BaseTooltip`을 지정하고
  `Message`를 설정한다. 이 경로가 Unreal UMG tooltip customization의 기본 방식이다.
- `BaseTooltipAnchor`는 별도 runtime hover/delay spawn 예제용 wrapper이며, 표준 툴팁 설정을 대체하지 않는다.
- Gallery는 항상 보이는 static `WBP_BaseTooltip` 예제와 tooltip widget 설정 예제를 함께 둔다.

### Context Menu
- 우클릭 지점에서 메뉴 top-left 시작(포인터와 겹쳐도 됨, 다음 클릭 전까지 유지).
- **임의 child를 담는 NamedSlot 컨테이너 WBP**(아이템/구분선/중첩 서브메뉴 호스팅).
- danger 항목(Delete 등) = danger 텍스트. 단축키 우정렬, 서브메뉴 caret-right.
- 옵션이 없으면 placeholder text를 표시해 Designer에서 빈 위젯 용도를 드러낸다.

## 검증 메모
- Designer/Preview hover·right-click popup은 신뢰하지 않는다. Gallery에는 항상 보이는 static
  Tooltip/Context Menu sample을 두고, 실제 popup은 PIE/runtime 이벤트로 별도 확인한다.
- runtime 검증에서 synthetic hover가 `UUserWidget` hover native path를 재현하지 못하면
  anchor의 class/message/delay readback과 C++ spawn path를 함께 확인해 preview/tool 한계와
  구현 문제를 구분한다.

## 참고 구현/검증 자산
- 레퍼런스 위젯: `Client/Source/OdiroSim/Public/UI/BaseButtonWidget.h` (+ `.cpp`).
- 토큰: `Client/Source/OdiroSim/Public/UI/BaseWidgetTokens.h`,
  `Private/UI/BaseWidgetTokens.cpp`, `Public/UI/BaseWidgetTypes.h`.
- 헬퍼: `Client/Source/OdiroSim/Private/UI/BaseWidgetPrivate.h`.
- 기존 mockup 렌더러: `UmgBaseFormMockupRenderer.cpp` /
  `UUmgGetSubsystem::CaptureBaseFormElementsMockup` — **mockup 전용**(`FSlateRoundedBoxBrush`).
  실제 위젯 캡처로 전환하면 시각 회귀 대조에 활용 가능.
