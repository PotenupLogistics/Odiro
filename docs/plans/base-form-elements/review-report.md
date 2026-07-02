# Base Widgets Review Report

작성일: 2026-06-30

## 완료 검증
- C++ automation: `OdiroSim.UI.BaseFormElements` commandlet 9/9 성공.
  - 통과: `CheckBoxGroup`, `DpiScale`, `Selection`, `SizeConstraints`, `Slider`,
    `SurfaceSmoke`, `TextInput`, `TextPreservation`, `TokenScale`.
- WBP compile/open: Base/Gallery WBP 37개 compile 성공, warning 0, editor open 성공, missing 0.
- Tick scan: 대상 WBP EventGraph `Tick` node 0개, UI C++ `NativeTick` / `Tick(` 검색 결과 없음.
- Root sizing: 대상 WBP 37개 readback, SizeBox root 16개 모두
  `bOverride_WidthOverride=False`, `bOverride_HeightOverride=False`.
- Capture smoke:
  - `WBP_Gallery_{Small,Medium,Large}_FormElements` wide/narrow capture 생성.
  - 대표 위젯 capture 생성: button, icon, text, text input, dropdown, switch,
    slider combo, tooltip, context menu.
- PIE smoke: `pie_control start` / `stop` 성공.
- 2026-06-30 visual regression pass:
  - `WBP_BaseButton`, `WBP_BaseToggleButton`, `WBP_BaseCheckBox`, `WBP_BaseSwitch`,
    `WBP_BaseTab`, `WBP_BaseDropdown`, `WBP_BaseDropdownOption`, `WBP_BaseTextInput`
    DA-driven state sync 회복.
  - `WBP_BaseSliderCombo` native property / WBP child variable name 충돌 제거,
    compile/save/capture 성공.
  - `WBP_BaseText`, `WBP_BaseToggleButton`, `WBP_BaseSliderCombo` 단독 capture 성공.

## 남은 리스크
- Hover/focus/right-click popup의 UMG Animation 동작은 compile/open/capture와 PIE smoke까지 확인했다.
  자동 hover/right-click 상호작용 재현은 MCP 입력 경로가 안정적이지 않아 이번 검증에서 제외했다.
- Gallery wide capture에서 `Selection Controls` sample frame 사이 간격이 매우 좁다.
  실제 clipping/overlap은 확인되지 않았지만, 시각 테스트 페이지 가독성 개선 여지가 있다.
- 기존 read-only asset은 compile은 성공했지만 save가 skip됐다:
  `WBP_BaseCard`, `WBP_BaseEmptyState`, `WBP_BaseListItem`, `WBP_BaseMetricCard`,
  `WBP_BaseNotificationRow`, `WBP_BaseProgressCard`, `WBP_BaseToolbar`.
- Base 범위 밖 Platform WBP에서 동일 계열 BindWidget 충돌 로그 존재:
  `WBP_ProjectCreateScreen.ProjectNameInput/ProjectParentFolderInput`,
  `WBP_RunListScreen.BaseSeedInput/EpisodeCountInput/FixedFpsInput/MapIdInput`.
  Base Widgets compile/capture에는 영향 없음.

## 도구 이슈
- In-editor `Automation RunTests`는 `FWaitForInteractiveFrameRate`에서 600초 대기했고
  이후 결과가 늦게 나왔다. Base UI automation은 commandlet 경로를 우선 사용한다.
- Reload MCP rebuild/restart 중 `UE_MCP_Bridge` WebSocket은 살아 있는데
  `Saved/UE_MCP_Bridge/port.json`이 사라지는 상태가 발생했다. 현재 작업에서는 listen 중인
  editor PID/port를 확인해 runtime lockfile만 복구한 뒤 검증을 계속했다.
