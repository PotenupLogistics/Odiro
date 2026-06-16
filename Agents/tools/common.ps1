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

function Stop-ProcessTree {
    param([int] $ProcessId)

    $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $ProcessId" -ErrorAction SilentlyContinue
    foreach ($child in $children) {
        Stop-ProcessTree -ProcessId ([int] $child.ProcessId)
    }

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($process) {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    }
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
    Set-AgentsRuntimeEnvironment -ArtifactOutputDir $ArtifactOutputDir

    $uvicornArgs = Get-AgentsServerArguments -Port $Port -Reload:$Reload
    Write-Step "Run Agents API on http://127.0.0.1:$Port"
    Invoke-External -WorkingDirectory $agentsDir -FilePath "uv" -Arguments $uvicornArgs
}
