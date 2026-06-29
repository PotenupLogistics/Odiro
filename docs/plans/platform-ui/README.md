# Common Base Widgets UI Mockups

Common Base Widgets 기반 화면 재구성 검토용 PNG 목업.

## Files
- `mockups.html`: 모든 화면을 렌더링하는 단일 HTML 소스.
- `capture-mockups.ps1`: Edge/Chrome headless로 화면별 PNG 캡처.
- `images/`: 캡처 결과.

## Capture
```powershell
.\capture-mockups.ps1
```

특정 브라우저를 지정해야 하면:

```powershell
.\capture-mockups.ps1 -BrowserPath "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
```

## Screens
- `01-splash-screen.png`
- `03-project-create.png`
- `04-project-startup-guide.png`
- `05-project-scenario-editor.png`
- `06-project-robot-configurator.png`
- `07-project-experiment-config-results.png`
- `08-project-experiment-result-detail.png`
