# Lists current Git LFS locks with owner information for human inspection.
param(
    [string] $PathPrefix = "",

    [switch] $ProblemMatcher,

    [switch] $Watch,

    [ValidateRange(15, 3600)]
    [int] $IntervalSeconds = 60,

    [switch] $OnlyWhenVSCodeActive,

    [ValidateRange(1, 60)]
    [int] $FocusCheckSeconds = 2,

    [string[]] $VSCodeProcessNames = @("Code", "Code - Insiders", "VSCodium")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"

# Writes a VSCode problemMatcher-compatible error anchored to the task definition.
function Write-ProblemMatcherError {
    param([string] $Message)

    $singleLineMessage = ($Message -replace "(`r`n|`n|`r)", " ").Trim()
    Write-Output ".vscode/tasks.json(1,1): error: $singleLineMessage"
}

# Loads the Windows foreground-window API used to avoid polling while VSCode is inactive.
function Initialize-ForegroundWindowApi {
    if ("OdiroForegroundWindowApi" -as [type]) {
        return
    }

    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class OdiroForegroundWindowApi
{
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
}
"@
}

# Returns the process name that owns the current foreground window.
function Get-ForegroundProcessName {
    try {
        Initialize-ForegroundWindowApi

        $foregroundWindow = [OdiroForegroundWindowApi]::GetForegroundWindow()
        if ($foregroundWindow -eq [IntPtr]::Zero) {
            return ""
        }

        [uint32] $processId = 0
        [void] [OdiroForegroundWindowApi]::GetWindowThreadProcessId($foregroundWindow, [ref] $processId)
        if ($processId -eq 0) {
            return ""
        }

        $process = Get-Process -Id ([int] $processId) -ErrorAction SilentlyContinue
        if ($null -eq $process) {
            return ""
        }

        return [string] $process.ProcessName
    }
    catch {
        return ""
    }
}

# Reports whether Git LFS lock polling should run for the current window focus.
function Test-LfsLockPollingActive {
    if (-not $OnlyWhenVSCodeActive) {
        return $true
    }

    $foregroundProcessName = Get-ForegroundProcessName
    foreach ($processName in $VSCodeProcessNames) {
        if ([string]::Equals($foregroundProcessName, $processName, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    return $false
}

# Normalizes the optional lock path prefix used for filtering rows.
function ConvertTo-LfsLockPathPrefix {
    param([string] $PathPrefix)

    $normalizedPrefix = ($PathPrefix -replace '\\', '/').Trim().TrimStart('./')
    if ($normalizedPrefix -match '(^|/)\.\.($|/)') {
        throw "Parent directory segments are not allowed in PathPrefix."
    }

    return $normalizedPrefix
}

# Returns Git LFS locks as sorted rows ready for terminal or Problems output.
function Get-LfsLockRows {
    param(
        [string] $RepoRoot,
        [string] $NormalizedPrefix
    )

    $stderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $locksJson = @(git -C $RepoRoot lfs locks --json 2>$stderrPath)
        $gitExitCode = $LASTEXITCODE
        $locksStderrText = Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue
        if ($null -eq $locksStderrText) {
            $locksStderr = ""
        }
        else {
            $locksStderr = $locksStderrText.Trim()
        }
    }
    finally {
        Remove-Item -LiteralPath $stderrPath -Force -ErrorAction SilentlyContinue
    }

    if ($gitExitCode -ne 0) {
        if ([string]::IsNullOrWhiteSpace($locksStderr)) {
            throw "Failed to query Git LFS locks."
        }
        throw "Failed to query Git LFS locks.`n$locksStderr"
    }

    if ($locksStderr -match '(?im)\b(fatal|error|failed)\b') {
        throw "Git LFS locks reported an error.`n$locksStderr"
    }
    elseif (-not [string]::IsNullOrWhiteSpace($locksStderr)) {
        Write-WarningMessage $locksStderr
    }

    $locksText = ($locksJson -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($locksText)) {
        throw "Git LFS locks returned empty JSON."
    }

    $locksPayload = $locksText | ConvertFrom-Json
    if ($null -eq $locksPayload) {
        $locks = @()
    }
    elseif ($locksPayload -is [array]) {
        $locks = @($locksPayload)
    }
    elseif ($locksPayload.PSObject.Properties["locks"]) {
        $locks = @($locksPayload.PSObject.Properties["locks"].Value)
    }
    else {
        $locks = @($locksPayload)
    }

    $rows = @()
    foreach ($lock in $locks) {
        $pathProperty = $lock.PSObject.Properties["path"]
        $path = ""
        if ($null -ne $pathProperty) {
            $path = ([string] $pathProperty.Value -replace '\\', '/').Trim()
        }
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }
        if (-not [string]::IsNullOrWhiteSpace($NormalizedPrefix) -and -not $path.StartsWith($NormalizedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $owner = ""
        $ownerProperty = $lock.PSObject.Properties["owner"]
        if ($null -ne $ownerProperty -and $null -ne $ownerProperty.Value) {
            foreach ($ownerField in @("name", "login", "username", "email")) {
                $field = $ownerProperty.Value.PSObject.Properties[$ownerField]
                if ($null -ne $field -and -not [string]::IsNullOrWhiteSpace([string] $field.Value)) {
                    $owner = [string] $field.Value
                    break
                }
            }
        }
        if ([string]::IsNullOrWhiteSpace($owner)) {
            $owner = "<unknown>"
        }

        $lockedAt = ""
        $lockedAtProperty = $lock.PSObject.Properties["locked_at"]
        if ($null -ne $lockedAtProperty) {
            $lockedAt = [string] $lockedAtProperty.Value
        }
        $lockedAtDisplay = $lockedAt
        if (-not [string]::IsNullOrWhiteSpace($lockedAt)) {
            try {
                $lockedAtDisplay = ([datetimeoffset]::Parse($lockedAt)).UtcDateTime.ToString("yyyy'/'MM'/'dd HH:mm")
            }
            catch {
                $lockedAtDisplay = $lockedAt
            }
        }

        $id = ""
        $idProperty = $lock.PSObject.Properties["id"]
        if ($null -ne $idProperty) {
            $id = [string] $idProperty.Value
        }

        $rows += [pscustomobject]@{
            Path = $path
            Owner = $owner
            LockedAt = $lockedAt
            LockedAtDisplay = $lockedAtDisplay
            Id = $id
        }
    }

    return @($rows | Sort-Object Path)
}

# Emits one VSCode watch cycle so old Git LFS lock Problems are replaced.
function Write-ProblemMatcherRefresh {
    param(
        [string] $RepoRoot,
        [string] $NormalizedPrefix
    )

    Write-Output "Git LFS Locks refresh started"
    try {
        $rows = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix $NormalizedPrefix)
        foreach ($row in $rows) {
            $message = "Locked by '$($row.Owner)' at $($row.LockedAtDisplay) ($($row.Id))"
            Write-Output "$($row.Path)(1,1): warning: $message"
        }
    }
    catch {
        Write-ProblemMatcherError $_.Exception.Message
    }
    finally {
        Write-Output "Git LFS Locks refresh completed"
    }
}

# Converts one value to a single table field.
function Format-TableField {
    param([string] $Value)

    if ($null -eq $Value) {
        return ""
    }

    return (($Value -replace "`t", " ") -replace "(`r`n|`n|`r)", " ").Trim()
}

# Calculates a display width for one compact table column.
function Get-TableColumnWidth {
    param(
        [string] $Header,
        [string[]] $Values
    )

    $width = $Header.Length
    foreach ($value in $Values) {
        if ($null -ne $value -and $value.Length -gt $width) {
            $width = $value.Length
        }
    }

    return $width
}

# Writes lock rows in compact table form for terminal inspection.
function Write-LfsLockRowsAsTable {
    param([object[]] $Rows)

    if ($Rows.Count -eq 0) {
        Write-Success "No active Git LFS locks."
        return
    }

    $tableRows = @(
        foreach ($row in $Rows) {
            [pscustomobject]@{
                Owner = Format-TableField -Value ([string] $row.Owner)
                LockedAt = Format-TableField -Value ([string] $row.LockedAtDisplay)
                Id = Format-TableField -Value ([string] $row.Id)
                Path = Format-TableField -Value ([string] $row.Path)
            }
        }
    )

    $ownerWidth = Get-TableColumnWidth -Header "Owner" -Values @($tableRows.Owner)
    $lockedAtWidth = Get-TableColumnWidth -Header "LockedAt" -Values @($tableRows.LockedAt)
    $idWidth = Get-TableColumnWidth -Header "Id" -Values @($tableRows.Id)

    Write-Output ("{0}  {1}  {2}  {3}" -f `
        "Owner".PadRight($ownerWidth), `
        "LockedAt".PadRight($lockedAtWidth), `
        "Id".PadRight($idWidth), `
        "Path")
    Write-Output ("{0}  {1}  {2}  {3}" -f `
        ("-" * $ownerWidth), `
        ("-" * $lockedAtWidth), `
        ("-" * $idWidth), `
        "----")

    foreach ($row in $tableRows) {
        Write-Output ("{0}  {1}  {2}  {3}" -f `
            $row.Owner.PadRight($ownerWidth), `
            $row.LockedAt.PadRight($lockedAtWidth), `
            $row.Id.PadRight($idWidth), `
            $row.Path)
    }
}

# Polls Git LFS locks for VSCode Problems until the task is stopped.
function Watch-ProblemMatcherLocks {
    param(
        [string] $RepoRoot,
        [string] $NormalizedPrefix
    )

    $lastRefreshAtUtc = [datetime]::MinValue
    $wasPollingActive = $false

    while ($true) {
        $isPollingActive = Test-LfsLockPollingActive
        if ($isPollingActive) {
            $isFocusRefresh = -not $wasPollingActive
            $isIntervalRefresh = (([datetime]::UtcNow - $lastRefreshAtUtc).TotalSeconds -ge $IntervalSeconds)

            if ($isFocusRefresh -or $isIntervalRefresh) {
                Write-ProblemMatcherRefresh -RepoRoot $RepoRoot -NormalizedPrefix $NormalizedPrefix
                $lastRefreshAtUtc = [datetime]::UtcNow
            }
        }

        $wasPollingActive = $isPollingActive
        Start-Sleep -Seconds $FocusCheckSeconds
    }
}

# Runs the command-line entry point when this script is executed directly.
function Invoke-ListLfsLocksScript {
    Set-ToolPrefix "list-lfs-locks"

    try {
        Assert-Command "git"

        $repoRoot = Get-RepoRoot
        $normalizedPrefix = ConvertTo-LfsLockPathPrefix -PathPrefix $PathPrefix

        if ($Watch -and -not $ProblemMatcher) {
            throw "-Watch requires -ProblemMatcher."
        }

        if ($ProblemMatcher) {
            if ($Watch) {
                Watch-ProblemMatcherLocks -RepoRoot $repoRoot -NormalizedPrefix $normalizedPrefix
            }
            else {
                Write-ProblemMatcherRefresh -RepoRoot $repoRoot -NormalizedPrefix $normalizedPrefix
            }
            exit 0
        }

        $rows = @(Get-LfsLockRows -RepoRoot $repoRoot -NormalizedPrefix $normalizedPrefix)
        Write-LfsLockRowsAsTable -Rows $rows
    }
    catch {
        if ($ProblemMatcher) {
            Write-ProblemMatcherError $_.Exception.Message
        }
        else {
            Write-ErrorMessage $_.Exception.Message
        }
        exit 1
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    Invoke-ListLfsLocksScript
}
