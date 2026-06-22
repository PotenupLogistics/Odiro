# Human-only helper for inspecting and force-unlocking Git LFS locks by exact path or per-lock prompt.
[CmdletBinding(DefaultParameterSetName = "Path")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Path")]
    [string] $Path,

    [Parameter(Mandatory = $true, ParameterSetName = "All")]
    [switch] $UnlockAll,

    [Parameter(ParameterSetName = "Path")]
    [switch] $Unlock,

    [Parameter(ParameterSetName = "Path")]
    [string] $ConfirmPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
. "$PSScriptRoot\list-lfs-locks.ps1"
Set-ToolPrefix "manual-unlock"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot
Assert-Command "git"

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

# Reads the Git LFS API endpoint from the repository's effective LFS config.
function Get-LfsApiEndpoint {
    param([string] $RepoRoot)

    $lines = @(git -C $RepoRoot lfs env)
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to read Git LFS environment."
    }

    foreach ($line in $lines) {
        if ($line -match '^Endpoint=(?<Url>\S+)') {
            return ([string] $Matches["Url"]).TrimEnd("/")
        }
    }

    throw "Git LFS endpoint was not found."
}

# Verifies that an API-unlocked Git LFS lock id is no longer listed.
function Assert-LfsLockRemovedById {
    param(
        [string] $RepoRoot,
        [string] $Id
    )

    $remainingLocks = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
    $remainingLock = @($remainingLocks | Where-Object { [string] $_.Id -eq $Id })
    if ($remainingLock.Count -gt 0) {
        throw "Git LFS lock id $Id is still listed after unlock."
    }
}

# Force-unlocks one Git LFS lock by API id without touching the stored local path.
function Invoke-ForceUnlockById {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id
    )

    Assert-Command "gh"

    $endpoint = Get-LfsApiEndpoint -RepoRoot $RepoRoot
    $unlockUrl = "$endpoint/locks/$Id/unlock"
    $body = @{ force = $true } | ConvertTo-Json -Compress

    $output = @(
        $body | gh api `
            -X POST `
            $unlockUrl `
            -H "Accept: application/vnd.git-lfs+json" `
            -H "Content-Type: application/vnd.git-lfs+json" `
            --input - `
            2>&1
    )
    if ($LASTEXITCODE -ne 0) {
        $detail = (($output | ForEach-Object { [string] $_ }) -join "`n").Trim()
        if ([string]::IsNullOrWhiteSpace($detail)) {
            throw "Failed to unlock $Path by Git LFS lock id $Id."
        }

        throw "Failed to unlock $Path by Git LFS lock id $Id.`n$detail"
    }

    Assert-LfsLockRemovedById -RepoRoot $RepoRoot -Id $Id
}

# Force-unlocks one Git LFS lock by id when available, otherwise by path.
function Invoke-ForceUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        git -C $RepoRoot lfs unlock --force $Path
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to unlock $Path."
        }
        return
    }

    Invoke-ForceUnlockById -RepoRoot $RepoRoot -Path $Path -Id $Id
}

# Prompts for explicit per-lock consent, defaulting every response to no.
function Read-UnlockConfirmation {
    param(
        [object] $LockRow,
        [string] $GitPath
    )

    Write-Host ""
    Write-Host "Path     : $GitPath"
    Write-Host "Owner    : $($LockRow.Owner)"
    Write-Host "LockedAt : $($LockRow.LockedAt)"
    Write-Host "Id       : $($LockRow.Id)"

    $answer = Read-Host "Unlock this path? Type y to unlock [y/N]"
    if ($null -eq $answer) {
        return $false
    }

    $answer = $answer.Trim()
    return [string]::Equals($answer, "y", [System.StringComparison]::OrdinalIgnoreCase)
}

# Prompts through every current Git LFS lock and unlocks only selected rows.
function Invoke-UnlockAllLocks {
    param([string] $RepoRoot)

    $rows = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
    if ($rows.Count -eq 0) {
        Write-Success "No active Git LFS locks."
        return
    }

    Write-Step "Active Git LFS locks: $($rows.Count)"
    $unlockedCount = 0
    $skippedCount = 0

    foreach ($row in $rows) {
        $gitPath = ConvertLockPathToGitPath ([string] $row.Path)
        if (Read-UnlockConfirmation -LockRow $row -GitPath $gitPath) {
            Invoke-ForceUnlock -RepoRoot $RepoRoot -Path $gitPath -Id ([string] $row.Id)
            Write-Success "Unlocked: $gitPath"
            $unlockedCount += 1
        }
        else {
            Write-Step "Skipped: $gitPath"
            $skippedCount += 1
        }
    }

    Write-Success "Unlock prompts complete. Unlocked $unlockedCount, skipped $skippedCount."
}

Write-WarningMessage "Human-only script. Coding agents must not run this unless the user explicitly requested unlock in the current turn."

if ($UnlockAll) {
    Invoke-UnlockAllLocks -RepoRoot $repoRoot
    exit 0
}

$repoPath = ConvertTo-GitPath $Path
$locks = @(Get-LfsLockRows -RepoRoot $repoRoot -NormalizedPrefix "")
$matchingLocks = @($locks | Where-Object { (ConvertLockPathToGitPath ([string] $_.Path)) -eq $repoPath })

if ($matchingLocks.Count -eq 0) {
    Write-Step "No active lock: $repoPath"
    exit 0
}

foreach ($lock in $matchingLocks) {
    $id = [string] $lock.Id
    $owner = [string] $lock.Owner
    $lockedAt = [string] $lock.LockedAt
    Write-Step "Lock id=$id path=$repoPath owner=$owner locked_at=$lockedAt"
}

if (-not $Unlock) {
    Write-Step "Dry-run only. Add -Unlock -ConfirmPath $repoPath to unlock."
    exit 0
}

$confirmGitPath = ConvertTo-GitPath $ConfirmPath
if ($confirmGitPath -ne $repoPath) {
    throw "ConfirmPath must exactly match Path after normalization."
}

foreach ($lock in $matchingLocks) {
    Invoke-ForceUnlock -RepoRoot $repoRoot -Path $repoPath -Id ([string] $lock.Id)
}

Write-Success "Unlocked: $repoPath"
