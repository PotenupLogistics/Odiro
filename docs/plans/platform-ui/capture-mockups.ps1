param(
    [string]$BrowserPath = ""
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Html = Join-Path $Root "mockups.html"
$OutDir = Join-Path $Root "images"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Screens = @(
    @{ Id = "splash-screen"; File = "01-splash-screen.png" },
    @{ Id = "project-create"; File = "03-project-create.png" },
    @{ Id = "project-startup-guide"; File = "04-project-startup-guide.png" },
    @{ Id = "project-scenario-editor"; File = "05-project-scenario-editor.png" },
    @{ Id = "project-robot-configurator"; File = "06-project-robot-configurator.png" },
    @{ Id = "project-experiment-config-results"; File = "07-project-experiment-config-results.png" },
    @{ Id = "project-experiment-result-detail"; File = "08-project-experiment-result-detail.png" }
)

function Resolve-Browser {
    param([string]$Preferred)

    if ($Preferred -and (Test-Path $Preferred)) {
        return (Resolve-Path $Preferred).Path
    }

    $Candidates = @(
        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
        "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe"
    )

    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path $Candidate)) {
            return (Resolve-Path $Candidate).Path
        }
    }

    $Command = Get-Command msedge.exe -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }

    $Command = Get-Command chrome.exe -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }

    throw "No Edge or Chrome executable found. Pass -BrowserPath with a Chromium-compatible browser."
}

$Browser = Resolve-Browser -Preferred $BrowserPath
$HtmlUri = [System.Uri]::new((Resolve-Path $Html).Path).AbsoluteUri

foreach ($Screen in $Screens) {
    $Output = Join-Path $OutDir $Screen.File
    $Url = "${HtmlUri}?screen=$($Screen.Id)"
    & $Browser `
        "--headless=new" `
        "--disable-gpu" `
        "--hide-scrollbars" `
        "--force-device-scale-factor=1" `
        "--window-size=1600,900" `
        "--screenshot=$Output" `
        $Url | Out-Null

    if (!(Test-Path $Output)) {
        throw "Capture failed: $($Screen.File)"
    }
}

Write-Host "Captured $($Screens.Count) mockup images to $OutDir"
