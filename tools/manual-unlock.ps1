# Human-only helper for unlocking Git LFS locks by exact path or per-lock prompt.
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Path = "",

    [switch] $All,

    [switch] $Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
. "$PSScriptRoot\list-lfs-locks.ps1"

# Returns the supported command-line forms for manual Git LFS unlocks.
function Get-ManualUnlockUsage {
    return @"
Usage:
  tools/manual-unlock.ps1 <Path>
  tools/manual-unlock.ps1 -All
  tools/manual-unlock.ps1 -Force <Path>
  tools/manual-unlock.ps1 -Force -All
"@.Trim()
}

# Normalizes user input to repository-relative Git paths.
function ConvertTo-GitPath {
    param([string] $InputPath)

    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw "Path is required."
    }
    if ([System.IO.Path]::IsPathRooted($InputPath)) {
        throw "Use a repository-relative path, not an absolute path."
    }

    $gitPath = ($InputPath -replace '\\', '/').Trim()
    $gitPath = $gitPath.TrimStart('./')
    if ([string]::IsNullOrWhiteSpace($gitPath) -or $gitPath -eq "." -or $gitPath -eq "/") {
        throw "Repository root is not a valid unlock target."
    }
    if ($gitPath -match '(^|/)\.\.($|/)') {
        throw "Parent directory segments are not allowed."
    }
    if ($gitPath.StartsWith("-", [System.StringComparison]::Ordinal)) {
        throw "Path must not start with '-'."
    }

    return $gitPath
}

# Normalizes a Git LFS lock row path without treating literal path characters as user wildcards.
function ConvertLockPathToGitPath {
    param([string] $InputPath)

    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw "Lock path is empty."
    }

    $gitPath = ($InputPath -replace '\\', '/').Trim()
    $gitPath = $gitPath.TrimStart('./')
    if ([string]::IsNullOrWhiteSpace($gitPath) -or $gitPath -eq "." -or $gitPath -eq "/") {
        throw "Git LFS returned an invalid lock path."
    }
    if ($gitPath -match '(^|/)\.\.($|/)') {
        throw "Git LFS returned a lock path with parent directory segments."
    }

    return $gitPath
}

# Verifies that an unlocked Git LFS lock id is no longer listed.
function Assert-LfsLockRemovedById {
    param(
        [string] $RepoRoot,
        [string] $Id
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        return
    }

    $remainingLocks = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
    $remainingLock = @($remainingLocks | Where-Object { [string] $_.Id -eq $Id })
    if ($remainingLock.Count -gt 0) {
        throw "Git LFS lock id $Id is still listed after unlock."
    }
}

# Runs a native command and throws with its stderr/stdout when it fails.
function Invoke-CheckedNativeCommand {
    param(
        [string] $FailureMessage,
        [scriptblock] $Command
    )

    $output = @(& $Command 2>&1)
    if ($LASTEXITCODE -eq 0) {
        return $output
    }

    $detail = (($output | ForEach-Object { [string] $_ }) -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($detail)) {
        throw $FailureMessage
    }

    throw "$FailureMessage`n$detail"
}

# Unlocks a lock through the normal Git LFS path-based operation.
function Invoke-NormalUnlockByPath {
    param(
        [string] $RepoRoot,
        [string] $Path
    )

    Invoke-CheckedNativeCommand `
        -FailureMessage "Failed to unlock $Path." `
        -Command { git -C $RepoRoot lfs unlock $Path } |
        Out-Null
}

# Force-unlocks a lock through Git LFS, preferring the lock id to avoid path ambiguity.
function Invoke-ForceUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        Invoke-CheckedNativeCommand `
            -FailureMessage "Failed to force unlock $Path." `
            -Command { git -C $RepoRoot lfs unlock --force $Path } |
            Out-Null
        return
    }

    Invoke-CheckedNativeCommand `
        -FailureMessage "Failed to force unlock $Path by Git LFS lock id $Id." `
        -Command { git -C $RepoRoot lfs unlock --force "--id=$Id" } |
        Out-Null
}

# Unlocks one path using either normal or force mode and verifies removed lock ids.
function Invoke-LfsUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [object[]] $Locks,
        [switch] $Force
    )

    if ($Force) {
        foreach ($lock in $Locks) {
            Invoke-ForceUnlock -RepoRoot $RepoRoot -Path $Path -Id ([string] $lock.Id)
            Assert-LfsLockRemovedById -RepoRoot $RepoRoot -Id ([string] $lock.Id)
        }
        return
    }

    Invoke-NormalUnlockByPath -RepoRoot $RepoRoot -Path $Path
    foreach ($lock in $Locks) {
        Assert-LfsLockRemovedById -RepoRoot $RepoRoot -Id ([string] $lock.Id)
    }
}

# Prompts for explicit per-lock consent, defaulting every response to no.
function Read-UnlockConfirmation {
    param(
        [object] $LockRow,
        [string] $GitPath,
        [switch] $Force
    )

    Write-Host ""
    Write-Host "Path     : $GitPath"
    Write-Host "Owner    : $($LockRow.Owner)"
    Write-Host "LockedAt : $($LockRow.LockedAt)"
    Write-Host "Id       : $($LockRow.Id)"

    if ($Force) {
        $answer = Read-Host "Force unlock this path? Type y to unlock [y/N]"
    }
    else {
        $answer = Read-Host "Unlock this path? Type y to unlock [y/N]"
    }

    if ($null -eq $answer) {
        return $false
    }

    $answer = $answer.Trim()
    return [string]::Equals($answer, "y", [System.StringComparison]::OrdinalIgnoreCase)
}

# Prompts through every current Git LFS lock and unlocks only selected rows.
function Invoke-UnlockAllLocks {
    param(
        [string] $RepoRoot,
        [switch] $Force
    )

    $rows = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
    if ($rows.Count -eq 0) {
        Write-Success "No active Git LFS locks."
        return
    }

    Write-Step "Active Git LFS locks: $($rows.Count)"
    $unlockedCount = 0
    $skippedCount = 0
    $failedCount = 0

    foreach ($row in $rows) {
        $gitPath = ConvertLockPathToGitPath -InputPath ([string] $row.Path)
        if (-not (Read-UnlockConfirmation -LockRow $row -GitPath $gitPath -Force:$Force)) {
            Write-Step "Skipped: $gitPath"
            $skippedCount += 1
            continue
        }

        try {
            Invoke-LfsUnlock -RepoRoot $RepoRoot -Path $gitPath -Locks @($row) -Force:$Force
            Write-Success "Unlocked: $gitPath"
            $unlockedCount += 1
        }
        catch {
            Write-WarningMessage "Failed: $gitPath"
            Write-WarningMessage $_.Exception.Message
            $failedCount += 1
        }
    }

    if ($failedCount -gt 0) {
        throw "Unlock prompts complete with $failedCount failure(s). Unlocked $unlockedCount, skipped $skippedCount."
    }

    Write-Success "Unlock prompts complete. Unlocked $unlockedCount, skipped $skippedCount."
}

# Runs the command-line entry point for manual Git LFS unlocks.
function Invoke-ManualUnlockScript {
    Set-ToolPrefix "manual-unlock"

    try {
        Assert-Command "git"

        if ($All -and -not [string]::IsNullOrWhiteSpace($Path)) {
            throw "-All cannot be combined with Path.`n$(Get-ManualUnlockUsage)"
        }
        if (-not $All -and [string]::IsNullOrWhiteSpace($Path)) {
            throw "Path is required unless -All is set.`n$(Get-ManualUnlockUsage)"
        }

        $repoRoot = Get-RepoRoot
        Write-WarningMessage "Human-only script. Coding agents must not run this unless the user explicitly requested unlock in the current turn."

        if ($All) {
            Invoke-UnlockAllLocks -RepoRoot $repoRoot -Force:$Force
            return
        }

        $repoPath = ConvertTo-GitPath -InputPath $Path
        $locks = @(Get-LfsLockRows -RepoRoot $repoRoot -NormalizedPrefix "")
        $matchingLocks = @(
            $locks | Where-Object {
                (ConvertLockPathToGitPath -InputPath ([string] $_.Path)) -eq $repoPath
            }
        )

        if ($matchingLocks.Count -eq 0) {
            Write-Step "No active lock: $repoPath"
            return
        }

        foreach ($lock in $matchingLocks) {
            Write-Step "Lock id=$($lock.Id) path=$repoPath owner=$($lock.Owner) locked_at=$($lock.LockedAt)"
        }

        Invoke-LfsUnlock -RepoRoot $repoRoot -Path $repoPath -Locks $matchingLocks -Force:$Force
        Write-Success "Unlocked: $repoPath"
    }
    catch {
        Write-ErrorMessage $_.Exception.Message
        exit 1
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    Invoke-ManualUnlockScript
}
