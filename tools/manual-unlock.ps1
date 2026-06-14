# Human-only helper for inspecting and force-unlocking Git LFS locks by exact path.
param(
    [Parameter(Mandatory = $true)]
    [string] $Path,

    [switch] $Unlock,

    [string] $ConfirmPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "manual-unlock"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot

# Normalizes user input to repository-relative Git paths.
function ConvertTo-GitPath {
    param([string] $InputPath)

    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw "Path is required."
    }
    if ([System.IO.Path]::IsPathRooted($InputPath)) {
        throw "Use a repository-relative path, not an absolute path."
    }
    if ($InputPath -match '[*?\[\]]') {
        throw "Wildcard paths are not allowed."
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

# Extracts a normalized path from a Git LFS lock object.
function Get-LockPath {
    param([object] $Lock)

    $property = $Lock.PSObject.Properties["path"]
    if ($null -eq $property -or [string]::IsNullOrWhiteSpace([string] $property.Value)) {
        return ""
    }

    return ConvertTo-GitPath ([string] $property.Value)
}

# Extracts a display owner from a Git LFS lock object.
function Get-LockOwner {
    param([object] $Lock)

    $owner = $Lock.PSObject.Properties["owner"]
    if ($null -eq $owner -or $null -eq $owner.Value) {
        return ""
    }

    $name = $owner.Value.PSObject.Properties["name"]
    if ($null -eq $name) {
        return ""
    }

    return [string] $name.Value
}

Write-WarningMessage "Human-only script. Coding agents must not run this unless the user explicitly requested exact-path unlock in the current turn."

$repoPath = ConvertTo-GitPath $Path
$locksJson = git -C $repoRoot lfs locks --json
if ($LASTEXITCODE -ne 0) {
    throw "Failed to query Git LFS locks."
}

$locks = @(($locksJson -join "`n") | ConvertFrom-Json)
$matchingLocks = @($locks | Where-Object { (Get-LockPath $_) -eq $repoPath })

if ($matchingLocks.Count -eq 0) {
    Write-Step "No active lock: $repoPath"
    exit 0
}

foreach ($lock in $matchingLocks) {
    $id = [string] $lock.id
    $owner = Get-LockOwner $lock
    $lockedAt = [string] $lock.locked_at
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
    $id = [string] $lock.id
    if ([string]::IsNullOrWhiteSpace($id)) {
        git -C $repoRoot lfs unlock --force $repoPath
    }
    else {
        git -C $repoRoot lfs unlock --force "--id=$id"
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to unlock $repoPath."
    }
}

Write-Success "Unlocked: $repoPath"
