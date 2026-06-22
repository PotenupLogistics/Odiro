# Shared helper functions for Agents development scripts.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:AgentsRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$script:ToolPrefix = "agents"

function Get-AgentsRoot {
    return $script:AgentsRoot
}

function Write-ToolMessage {
    param(
        [string] $Message,
        [string] $Prefix = "",
        [ConsoleColor] $Color = [ConsoleColor]::White
    )

    if ([string]::IsNullOrWhiteSpace($Prefix)) {
        $Prefix = $script:ToolPrefix
    }

    Write-Host "[$Prefix] $Message" -ForegroundColor $Color
}

function Set-ToolPrefix {
    param([string] $Prefix)
    $script:ToolPrefix = $Prefix
}

function Write-Step {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Cyan
}

function Write-Success {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Green
}

function Write-WarningMessage {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Yellow
}

function Write-ErrorMessage {
    param([string] $Message)
    Write-ToolMessage -Message "ERROR: $Message" -Color Red
}

function New-PrerequisiteIssue {
    param(
        [string] $Name,
        [string] $Detail,
        [string] $Reason = "",
        [string[]] $MissingItems = @(),
        [string] $Install,
        [string] $Verify = "",
        [string[]] $Docs = @()
    )

    return [pscustomobject]@{
        Name = $Name
        Detail = $Detail
        Reason = $Reason
        MissingItems = $MissingItems
        Install = $Install
        Verify = $Verify
        Docs = $Docs
    }
}

function Get-IssueValue {
    param(
        [object] $Issue,
        [string] $PropertyName,
        [object] $DefaultValue = ""
    )

    $property = $Issue.PSObject.Properties[$PropertyName]
    if ($property) {
        return $property.Value
    }
    return $DefaultValue
}

function Complete-Prerequisites {
    param(
        [object[]] $Issues,
        [switch] $AllowMissing,
        [string] $SuccessMessage = "Setup prerequisites OK.",
        [string] $ErrorMessage = "Setup prerequisites are missing. Install the missing tools, or rerun setup with -AllowMissingPrerequisites to continue anyway."
    )

    if ($Issues.Count -eq 0) {
        Write-Success $SuccessMessage
        return
    }

    Write-WarningMessage "Missing setup prerequisites:"
    foreach ($issue in $Issues) {
        Write-WarningMessage "- $(Get-IssueValue -Issue $issue -PropertyName "Name")"

        $detail = Get-IssueValue -Issue $issue -PropertyName "Detail"
        if (-not [string]::IsNullOrWhiteSpace($detail)) {
            Write-WarningMessage "  Status: $detail"
        }

        $reason = Get-IssueValue -Issue $issue -PropertyName "Reason"
        if (-not [string]::IsNullOrWhiteSpace($reason)) {
            Write-WarningMessage "  Required for: $reason"
        }

        $missingItems = @(Get-IssueValue -Issue $issue -PropertyName "MissingItems" -DefaultValue @())
        if ($missingItems.Count -gt 0) {
            Write-WarningMessage "  Missing items:"
            foreach ($item in $missingItems) {
                Write-WarningMessage "    - $item"
            }
        }

        $install = Get-IssueValue -Issue $issue -PropertyName "Install"
        if (-not [string]::IsNullOrWhiteSpace($install)) {
            Write-Step "  Install: $install"
        }

        $verify = Get-IssueValue -Issue $issue -PropertyName "Verify"
        if (-not [string]::IsNullOrWhiteSpace($verify)) {
            Write-Step "  Verify: $verify"
        }

        $docs = @(Get-IssueValue -Issue $issue -PropertyName "Docs" -DefaultValue @()) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
        foreach ($doc in $docs) {
            Write-Step "  Docs: $doc"
        }
    }

    if ($AllowMissing) {
        Write-WarningMessage "Continuing because -AllowMissing was set."
        return
    }

    throw $ErrorMessage
}

function Assert-Command {
    param([string] $Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found on PATH."
    }
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string] $WorkingDirectory,
        [Parameter(Mandatory = $true)][string] $FilePath,
        [string[]] $Arguments = @()
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$FilePath failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Get-RepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path (Get-AgentsRoot) "..")).Path
}

function Get-DefaultArtifactOutputDir {
    $repoRoot = Get-RepoRoot
    return Join-Path $repoRoot "Client"
}

function Wait-HttpEndpoint {
    param(
        [string] $Url,
        [int] $TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
            if ($response.StatusCode -ge 200 -and $response.StatusCode -lt 500) {
                return
            }
        }
        catch {
            Start-Sleep -Milliseconds 500
        }
    }

    throw "Timed out waiting for $Url."
}

# Loopback TCP port를 새 listener로 열 수 있는지 확인한다.
function Test-LoopbackPortAvailable {
    param([int] $Port)

    $listener = $null
    try {
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
        $listener.Start()
        return $true
    }
    catch [System.Net.Sockets.SocketException] {
        return $false
    }
    finally {
        if ($listener) {
            $listener.Stop()
        }
    }
}

# TCP listen port를 소유한 process id를 빠르게 조회한다.
function Get-PortOwnerProcessIds {
    param([int] $Port)

    $processIds = @()
    $netstat = Get-Command "netstat.exe" -ErrorAction SilentlyContinue
    if ($netstat) {
        $escapedPort = [regex]::Escape([string] $Port)
        $pattern = "^\s*TCP\s+(\S+):$escapedPort\s+\S+\s+LISTENING\s+(\d+)\s*$"
        $loopbackAddresses = @("127.0.0.1", "0.0.0.0", "::1", "::", "[::1]", "[::]")
        foreach ($line in @(& $netstat.Source -ano -p tcp)) {
            if ($line -match $pattern -and $loopbackAddresses -contains $Matches[1]) {
                $processIds += [int] $Matches[2]
            }
        }
    }

    if ($processIds.Count -eq 0 -and (Get-Command "Get-NetTCPConnection" -ErrorAction SilentlyContinue)) {
        $processIds += Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
            Where-Object { @("127.0.0.1", "0.0.0.0", "::1", "::") -contains $_.LocalAddress } |
            Select-Object -ExpandProperty OwningProcess -Unique
    }

    return @($processIds | Select-Object -Unique)
}

# process id 목록을 사용자 확인용 한 줄 요약으로 만든다.
function Get-PortOwnerProcessSummary {
    param([int[]] $ProcessIds)

    $summaries = foreach ($processId in $ProcessIds) {
        $process = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($process) {
            $processPath = ""
            try {
                $processPath = [string] $process.Path
            }
            catch {
                $processPath = ""
            }

            if ([string]::IsNullOrWhiteSpace($processPath)) {
                "$processId/$($process.ProcessName)"
            }
            else {
                "$processId/$($process.ProcessName): $processPath"
            }
        }
        else {
            [string] $processId
        }
    }
    return ($summaries -join "; ")
}

# TCP listen port를 소유한 process를 사용자 확인용 한 줄 요약으로 만든다.
function Get-PortOwnerSummary {
    param([int] $Port)

    return Get-PortOwnerProcessSummary -ProcessIds @(Get-PortOwnerProcessIds -Port $Port)
}

# TCP listen port를 점유한 process들을 종료한다.
function Stop-PortOwnerProcesses {
    param(
        [int] $Port,
        [int[]] $InitialProcessIds = @(),
        [int] $MaxPasses = 3
    )

    $attempted = @{}
    for ($pass = 0; $pass -lt $MaxPasses; $pass++) {
        if ($pass -eq 0 -and $InitialProcessIds.Count -gt 0) {
            $candidateIds = @($InitialProcessIds)
        }
        else {
            $candidateIds = @(Get-PortOwnerProcessIds -Port $Port)
        }

        $processIds = @(
            $candidateIds |
                Where-Object { -not $attempted.ContainsKey([string] ([int] $_)) } |
                Select-Object -Unique
        )
        if ($processIds.Count -eq 0) {
            return
        }

        foreach ($processId in $processIds) {
            $attempted[[string] ([int] $processId)] = $true
            Write-Step "Stop process using port $Port pid $processId"
            Stop-ProcessTree -ProcessId ([int] $processId)
        }

        Start-Sleep -Milliseconds 500
        if (Test-LoopbackPortAvailable -Port $Port) {
            return
        }
    }
}

# Y/n 입력을 받되 Enter는 yes로 처리한다.
function Read-YesNoDefaultYes {
    param([string] $Prompt)

    if ([Console]::IsInputRedirected) {
        return $false
    }

    $answer = Read-Host "$Prompt [Y/n]"
    if ([string]::IsNullOrWhiteSpace($answer)) {
        return $true
    }

    $normalized = $answer.Trim().ToLowerInvariant()
    return $normalized -eq "y" -or $normalized -eq "yes"
}

# Agents API port가 다시 사용 가능해질 때까지 대기한다.
function Wait-AgentsPortAvailable {
    param(
        [int] $Port,
        [int] $TimeoutSeconds = 10
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-LoopbackPortAvailable -Port $Port) {
            return $true
        }
        Start-Sleep -Milliseconds 300
    }
    return $false
}

# Agents API 시작 전에 port 충돌을 처리한다.
function Assert-AgentsPortAvailable {
    param([int] $Port)

    if (Test-LoopbackPortAvailable -Port $Port) {
        return
    }

    $ownerProcessIds = @(Get-PortOwnerProcessIds -Port $Port)
    $ownerSummary = Get-PortOwnerProcessSummary -ProcessIds $ownerProcessIds
    if ([string]::IsNullOrWhiteSpace($ownerSummary)) {
        $ownerSummary = "unknown owner"
    }

    Write-WarningMessage "Agents API port $Port is already in use: $ownerSummary"
    if (Read-YesNoDefaultYes -Prompt "Stop process(es) using port $Port and continue?") {
        Stop-PortOwnerProcesses -Port $Port -InitialProcessIds $ownerProcessIds
        if (Wait-AgentsPortAvailable -Port $Port) {
            return
        }

        throw "Agents API port $Port is still in use after stopping selected process(es)."
    }

    throw "Agents API port $Port is already in use. Stop the process using it or rerun with a free -AgentsPort. Owner: $ownerSummary."
}

# Process와 child process를 함께 종료한다.
function Stop-ProcessTree {
    param([int] $ProcessId)

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $process) {
        return
    }

    $taskKill = Get-Command "taskkill.exe" -ErrorAction SilentlyContinue
    if ($taskKill) {
        & $taskKill.Source /PID $ProcessId /T /F 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return
        }
        $global:LASTEXITCODE = 0
    }

    $killTreeMethod = [System.Diagnostics.Process].GetMethod("Kill", [type[]]@([bool]))
    if ($killTreeMethod) {
        $process.Kill($true)
        $global:LASTEXITCODE = 0
        return
    }

    $children = Get-CimInstance `
        Win32_Process `
        -Filter "ParentProcessId = $ProcessId" `
        -OperationTimeoutSec 1 `
        -ErrorAction SilentlyContinue
    foreach ($child in $children) {
        Stop-ProcessTree -ProcessId ([int] $child.ProcessId)
    }

    Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    $global:LASTEXITCODE = 0
}

function Set-AgentsRuntimeEnvironment {
    param([string] $ArtifactOutputDir = "")

    if ([string]::IsNullOrWhiteSpace($ArtifactOutputDir)) {
        $ArtifactOutputDir = Get-DefaultArtifactOutputDir
    }

    $env:SCENARIO_ARTIFACT_OUTPUT_DIR = $ArtifactOutputDir
    $env:SCENARIO_ARTIFACT_WRITE_ENABLED = "true"
}

function Get-AgentsServerArguments {
    param(
        [int] $Port,
        [switch] $Reload
    )

    $uvicornArgs = @("run", "uvicorn", "app.main:app", "--host", "127.0.0.1", "--port", [string] $Port)
    if ($Reload) {
        $uvicornArgs += "--reload"
    }
    return $uvicornArgs
}

function Start-AgentsApi {
    param(
        [int] $Port,
        [switch] $Reload,
        [string] $LogPrefix = "agents",
        [string] $ArtifactOutputDir = ""
    )

    $agentsDir = Get-AgentsRoot
    Assert-Command "uv"
    Assert-AgentsPortAvailable -Port $Port

    if (-not $env:OPENAI_API_KEY) {
        Write-WarningMessage "OPENAI_API_KEY is not set. OpenAI-backed generation will fail until it is configured."
    }

    Set-AgentsRuntimeEnvironment -ArtifactOutputDir $ArtifactOutputDir

    $logsDir = Join-Path (Get-RepoRoot) "logs"
    New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
    $stdoutLog = Join-Path $logsDir "$LogPrefix-agents.out.log"
    $stderrLog = Join-Path $logsDir "$LogPrefix-agents.err.log"

    $uvCommand = (Get-Command "uv" -ErrorAction Stop).Source
    $uvicornArgs = Get-AgentsServerArguments -Port $Port -Reload:$Reload

    Write-Step "Start Agents API on http://127.0.0.1:$Port"
    $process = Start-Process `
        -FilePath $uvCommand `
        -ArgumentList $uvicornArgs `
        -WorkingDirectory $agentsDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -PassThru

    Start-Sleep -Seconds 1
    if ($process.HasExited) {
        throw "Agents API exited early. Check $stderrLog."
    }

    try {
        Wait-HttpEndpoint -Url "http://127.0.0.1:$Port/health"
    }
    catch {
        if (-not $process.HasExited) {
            Stop-ProcessTree -ProcessId $process.Id
        }
        throw
    }

    Write-Step "Agents logs: $stdoutLog, $stderrLog"
    return $process
}

function Invoke-AgentsApiForeground {
    param(
        [int] $Port,
        [switch] $Reload,
        [string] $ArtifactOutputDir = ""
    )

    $agentsDir = Get-AgentsRoot
    Assert-Command "uv"
    Assert-AgentsPortAvailable -Port $Port
    Set-AgentsRuntimeEnvironment -ArtifactOutputDir $ArtifactOutputDir

    $uvicornArgs = Get-AgentsServerArguments -Port $Port -Reload:$Reload
    Write-Step "Run Agents API on http://127.0.0.1:$Port"
    Invoke-External -WorkingDirectory $agentsDir -FilePath "uv" -Arguments $uvicornArgs
}
