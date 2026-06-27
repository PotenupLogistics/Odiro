# Shared helper functions for Odiro development scripts.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:OdiroRepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$script:ToolPrefix = "tools"
# Ctrl+C cleanup 대상 child process registry.
$script:ManagedProcessCleanupItems = @{}
# Cleanup 재진입 방지 상태.
$script:ManagedProcessCleanupStarted = $false
# 등록된 Console.CancelKeyPress delegate.
$script:CancelCleanupHandler = $null

function Get-RepoRoot {
    return $script:OdiroRepoRoot
}

function Get-ClientDir {
    param([string] $RepoRoot)
    return Join-Path $RepoRoot "Client"
}

function Get-AgentsDir {
    param([string] $RepoRoot)
    return Join-Path $RepoRoot "Agents"
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
        [string] $ErrorMessage = "Setup prerequisites are missing. Install the missing tools."
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
        Write-WarningMessage "Continuing with missing prerequisites allowed."
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

# Ctrl+C 또는 finally cleanup 때 종료할 child process를 등록한다.
function Register-ManagedProcess {
    param(
        [System.Diagnostics.Process] $Process,
        [string] $Label = "process"
    )

    if (-not $Process) {
        return
    }

    $script:ManagedProcessCleanupItems[[string] $Process.Id] = [pscustomobject]@{
        Id = [int] $Process.Id
        Label = $Label
    }
}

# Cleanup registry에서 process를 제거한다.
function Unregister-ManagedProcess {
    param([System.Diagnostics.Process] $Process)

    if (-not $Process) {
        return
    }

    $script:ManagedProcessCleanupItems.Remove([string] $Process.Id) | Out-Null
}

# 등록된 child process tree를 한 번만 정리한다.
function Stop-ManagedProcessTrees {
    if ($script:ManagedProcessCleanupStarted) {
        return
    }

    $script:ManagedProcessCleanupStarted = $true
    try {
        foreach ($item in @($script:ManagedProcessCleanupItems.Values)) {
            $process = Get-Process -Id $item.Id -ErrorAction SilentlyContinue
            if (-not $process) {
                continue
            }

            Write-Step "Stop $($item.Label) pid $($item.Id)"
            Stop-ProcessTree -ProcessId $item.Id
        }
    }
    finally {
        $script:ManagedProcessCleanupItems.Clear()
        $script:ManagedProcessCleanupStarted = $false
    }
}

# Ctrl+C를 child process tree cleanup으로 연결한다.
function Register-CancelProcessCleanup {
    if ($script:CancelCleanupHandler) {
        return
    }

    $eventInfo = [System.Console].GetEvent("CancelKeyPress")
    if (-not $eventInfo) {
        throw "System.Console.CancelKeyPress event is unavailable."
    }

    $script:CancelCleanupHandler = [System.ConsoleCancelEventHandler] {
        param($Sender, $EventArgs)

        $EventArgs.Cancel = $true
        try {
            Write-WarningMessage "Ctrl+C received. Stopping child processes..."
            Stop-ManagedProcessTrees
        }
        catch {
            Write-ErrorMessage $_.Exception.Message
        }

        [Environment]::Exit(130)
    }
    $eventInfo.AddEventHandler($null, $script:CancelCleanupHandler)
}

# 등록된 Ctrl+C cleanup handler를 해제한다.
function Unregister-CancelProcessCleanup {
    if (-not $script:CancelCleanupHandler) {
        return
    }

    $eventInfo = [System.Console].GetEvent("CancelKeyPress")
    if ($eventInfo) {
        $eventInfo.RemoveEventHandler($null, $script:CancelCleanupHandler)
    }
    $script:CancelCleanupHandler = $null
}
