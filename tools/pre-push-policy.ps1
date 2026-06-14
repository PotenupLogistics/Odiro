# Implements the pre-push policy for main updates and Unreal asset locks.
param(
    [Parameter(Mandatory = $true)]
    [string] $UpdatesFile,

    [string] $RemoteName = "",

    [string] $RemoteUrl = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "pre-push"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot

# Identifies Git's all-zero object id used for deleted or absent refs.
function Test-ZeroOid {
    param([string] $Oid)

    return -not [string]::IsNullOrWhiteSpace($Oid) -and $Oid -match '^0+$'
}

# Normalizes Git paths for lock and diff comparisons.
function ConvertTo-GitPath {
    param([string] $Path)

    return ($Path -replace '\\', '/').TrimStart('./')
}

# Keeps lock policy limited to Unreal binary asset files.
function Test-UnrealAssetPath {
    param([string] $Path)

    return (ConvertTo-GitPath $Path) -match '\.(uasset|umap|ubulk|uexp)$'
}

# Checks whether a commit object exists in the local repository.
function Test-GitCommitExists {
    param([string] $Oid)

    git -C $repoRoot cat-file -e "$Oid^{commit}" 2>$null
    return $LASTEXITCODE -eq 0
}

# Checks fast-forward ancestry for push and local main update policy.
function Test-GitAncestor {
    param(
        [string] $Ancestor,
        [string] $Descendant
    )

    git -C $repoRoot merge-base --is-ancestor $Ancestor $Descendant 2>$null
    if ($LASTEXITCODE -eq 0) {
        return $true
    }
    if ($LASTEXITCODE -eq 1) {
        return $false
    }
    throw "Unable to classify ancestry between $Ancestor and $Descendant."
}

# Returns changed paths for one pre-push ref update.
function Get-ChangedPaths {
    param(
        [string] $OldOid,
        [string] $NewOid
    )

    if (Test-ZeroOid $NewOid) {
        return @()
    }

    if (Test-ZeroOid $OldOid) {
        $remoteMainName = if ([string]::IsNullOrWhiteSpace($RemoteName)) { "origin" } else { $RemoteName }
        $remoteMainRef = "refs/remotes/$remoteMainName/main"
        $baseRef = ""
        git -C $repoRoot show-ref --verify --quiet $remoteMainRef
        if ($LASTEXITCODE -eq 0) {
            $baseRef = $remoteMainRef
        }
        else {
            git -C $repoRoot show-ref --verify --quiet refs/heads/main
            if ($LASTEXITCODE -eq 0) {
                $baseRef = "refs/heads/main"
                Write-Step "Remote main is not present locally; using local main for new branch asset range."
            }
        }

        if ([string]::IsNullOrWhiteSpace($baseRef)) {
            throw "Cannot inspect new branch asset changes because neither $remoteMainRef nor local main is present."
        }

        $base = git -C $repoRoot merge-base $NewOid $baseRef
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($base)) {
            $baseOid = $base.Trim()
            return @(git -C $repoRoot diff --name-only $baseOid $NewOid)
        }

        throw "Cannot inspect new branch asset changes because $baseRef has no merge-base with $NewOid."
    }

    if (-not (Test-GitCommitExists $OldOid)) {
        throw "Cannot inspect pushed asset changes because remote object $OldOid is not present locally. Fetch $RemoteName and retry."
    }

    return @(git -C $repoRoot diff --name-only $OldOid $NewOid)
}

# Reads a JSON property as an array even when Git LFS omits it.
function Get-JsonArrayProperty {
    param(
        [object] $Object,
        [string] $Name
    )

    if ($null -eq $Object) {
        return @()
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return @()
    }

    return @($property.Value)
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

if (-not (Test-Path -LiteralPath $UpdatesFile)) {
    throw "Pre-push updates file not found: $UpdatesFile"
}

$updates = @(Get-Content -LiteralPath $UpdatesFile | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$changedAssets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$skippedAssetLockUpdateCount = 0

foreach ($line in $updates) {
    $parts = $line -split '\s+'
    if ($parts.Count -lt 4) {
        throw "Unexpected pre-push update line: $line"
    }

    $localRef = $parts[0]
    $localOid = $parts[1]
    $remoteRef = $parts[2]
    $remoteOid = $parts[3]
    $skipAssetLockVerification = $false

    if ($remoteRef -eq "refs/heads/main") {
        if (Test-ZeroOid $localOid) {
            throw "main delete push is blocked."
        }
        if (Test-ZeroOid $remoteOid) {
            throw "main creation push is blocked."
        }
        if (-not (Test-GitCommitExists $remoteOid)) {
            throw "Cannot classify main push because remote main object $remoteOid is not present locally. Fetch $RemoteName and retry."
        }
        if (Test-GitAncestor -Ancestor $remoteOid -Descendant $localOid) {
            throw "main fast-forward push is blocked. Merge to main through PR, or use an intentional non-fast-forward force push."
        }
        Write-Step "main non-fast-forward push allowed: $localRef -> $remoteRef"
        $skipAssetLockVerification = $true
    }

    if ($skipAssetLockVerification) {
        $skippedAssetLockUpdateCount += 1
        Write-Step "Skipping asset lock verification for intentional main history rewrite."
        continue
    }

    foreach ($path in Get-ChangedPaths -OldOid $remoteOid -NewOid $localOid) {
        $gitPath = ConvertTo-GitPath $path
        if (Test-UnrealAssetPath $gitPath) {
            [void] $changedAssets.Add($gitPath)
        }
    }
}

if ($changedAssets.Count -eq 0) {
    if ($skippedAssetLockUpdateCount -gt 0) {
        Write-Success "No non-rewrite Unreal binary asset changes require lock verification."
    }
    else {
        Write-Success "No pushed Unreal binary asset changes."
    }
    exit 0
}

$locksJson = git -C $repoRoot lfs locks --verify --json
if ($LASTEXITCODE -ne 0) {
    throw "Failed to query Git LFS locks."
}

$locksPayload = ($locksJson -join "`n") | ConvertFrom-Json
$oursByPath = @{}
foreach ($lock in Get-JsonArrayProperty -Object $locksPayload -Name "ours") {
    $lockPath = Get-LockPath $lock
    if (-not [string]::IsNullOrWhiteSpace($lockPath)) {
        $oursByPath[$lockPath] = $lock
    }
}

$missing = @()
foreach ($asset in ($changedAssets | Sort-Object)) {
    if (-not $oursByPath.ContainsKey($asset)) {
        $missing += $asset
    }
}

if ($missing.Count -gt 0) {
    $details = ($missing | ForEach-Object { "  - $_" }) -join "`n"
    throw "Unreal binary asset push requires active Git LFS locks owned by this user:`n$details"
}

Write-Success "Verified Git LFS locks for $($changedAssets.Count) pushed Unreal binary asset(s)."
