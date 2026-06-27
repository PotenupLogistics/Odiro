# Pushes the current branch and opens a GitHub pull request into main.
param(
    [string] $RemoteName = "origin",
    [string] $BaseBranch = "main",
    [string] $Title = "",
    [string] $Body = "",
    [switch] $Draft,
    [switch] $AllowDirty,
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "push-pr"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Assert-Command "git"
if (-not $DryRun) {
    Assert-Command "gh"
}

$repoRoot = Get-RepoRoot

# Normalizes Git paths for lock and diff comparisons.
function ConvertTo-GitPath {
    param([string] $Path)

    return ($Path -replace '\\', '/').TrimStart('./')
}

# Keeps lock preflight limited to Unreal binary asset files.
function Test-UnrealAssetPath {
    param([string] $Path)

    return (ConvertTo-GitPath $Path) -match '\.(uasset|umap|ubulk|uexp)$'
}

# Checks whether a local Git ref exists.
function Test-GitRefExists {
    param([string] $RefName)

    git -C $repoRoot show-ref --verify --quiet $RefName
    return $LASTEXITCODE -eq 0
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

# Extracts a human-readable owner from a Git LFS lock object.
function Get-LockOwnerName {
    param([object] $Lock)

    $ownerProperty = $Lock.PSObject.Properties["owner"]
    if ($null -eq $ownerProperty -or $null -eq $ownerProperty.Value) {
        return ""
    }

    $owner = $ownerProperty.Value
    if ($owner -is [string]) {
        return $owner
    }

    foreach ($name in @("name", "login")) {
        $property = $owner.PSObject.Properties[$name]
        if ($null -ne $property -and -not [string]::IsNullOrWhiteSpace([string] $property.Value)) {
            return [string] $property.Value
        }
    }

    return ""
}

# Refreshes a remote branch ref when it already exists on the server.
function Update-RemoteBranchRefIfPresent {
    param(
        [string] $RemoteName,
        [string] $BranchName
    )

    if ([string]::IsNullOrWhiteSpace($BranchName)) {
        return
    }

    $remoteBranch = "refs/heads/$BranchName"
    $localRemoteBranch = "refs/remotes/$RemoteName/$BranchName"
    $lsRemoteOutput = @(git -C $repoRoot ls-remote --exit-code --heads $RemoteName $BranchName 2>&1)
    if ($LASTEXITCODE -eq 2) {
        return
    }
    if ($LASTEXITCODE -ne 0) {
        $global:LASTEXITCODE = 0
        Write-WarningMessage "Could not check remote branch $RemoteName/$BranchName for LFS lock preflight; using local refs."
        return
    }

    $fetchOutput = @(git -C $repoRoot fetch --quiet $RemoteName "+${remoteBranch}:${localRemoteBranch}" 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $global:LASTEXITCODE = 0
        Write-WarningMessage "Could not refresh $RemoteName/$BranchName for LFS lock preflight; using local refs."
    }
}

# Returns the commit/ref used to approximate the pre-push asset range.
function Get-PushScanBase {
    param(
        [string] $RemoteName,
        [string] $BaseBranch,
        [string] $Branch
    )

    $remoteBranchRef = "refs/remotes/$RemoteName/$Branch"
    if (Test-GitRefExists -RefName $remoteBranchRef) {
        return $remoteBranchRef
    }

    $remoteBaseRef = "refs/remotes/$RemoteName/$BaseBranch"
    $localBaseRef = "refs/heads/$BaseBranch"
    $baseRef = ""
    if (Test-GitRefExists -RefName $remoteBaseRef) {
        $baseRef = $remoteBaseRef
    }
    elseif (Test-GitRefExists -RefName $localBaseRef) {
        $baseRef = $localBaseRef
        Write-Step "Remote $BaseBranch is not present locally; using local $BaseBranch for new branch asset range."
    }

    if ([string]::IsNullOrWhiteSpace($baseRef)) {
        throw "Cannot inspect new branch asset changes because neither $remoteBaseRef nor $localBaseRef is present."
    }

    $base = git -C $repoRoot merge-base HEAD $baseRef
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($base)) {
        throw "Cannot inspect new branch asset changes because $baseRef has no merge-base with HEAD."
    }

    return $base.Trim()
}

# Returns Unreal binary assets that will be included in the branch push.
function Get-PushedUnrealAssetPaths {
    param(
        [string] $RemoteName,
        [string] $BaseBranch,
        [string] $Branch
    )

    $base = Get-PushScanBase -RemoteName $RemoteName -BaseBranch $BaseBranch -Branch $Branch
    $paths = @(git -C $repoRoot diff --name-only $base HEAD)
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect pushed path range: $base..HEAD"
    }

    return @(
        $paths |
            ForEach-Object { ConvertTo-GitPath $_ } |
            Where-Object { Test-UnrealAssetPath $_ } |
            Sort-Object -Unique
    )
}

# Returns current Git LFS lock ownership by normalized path.
function Get-LfsLockOwnership {
    $locksJson = @(git -C $repoRoot lfs locks --verify --json 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $detail = (($locksJson | ForEach-Object { [string] $_ }) -join "`n").Trim()
        if ([string]::IsNullOrWhiteSpace($detail)) {
            throw "Failed to query Git LFS locks."
        }
        throw "Failed to query Git LFS locks.`n$detail"
    }

    $locksPayload = ($locksJson -join "`n") | ConvertFrom-Json
    $oursByPath = @{}
    $theirsByPath = @{}

    foreach ($lock in Get-JsonArrayProperty -Object $locksPayload -Name "ours") {
        $lockPath = Get-LockPath $lock
        if (-not [string]::IsNullOrWhiteSpace($lockPath)) {
            $oursByPath[$lockPath] = $lock
        }
    }

    foreach ($lock in Get-JsonArrayProperty -Object $locksPayload -Name "theirs") {
        $lockPath = Get-LockPath $lock
        if (-not [string]::IsNullOrWhiteSpace($lockPath)) {
            $theirsByPath[$lockPath] = $lock
        }
    }

    return [pscustomobject]@{
        OursByPath = $oursByPath
        TheirsByPath = $theirsByPath
    }
}

# Builds the exact push blockers that the local pre-push lock policy would reject.
function Get-LfsCheckoutCandidates {
    param([string[]] $AssetPaths)

    if ($AssetPaths.Count -eq 0) {
        return @()
    }

    $ownership = Get-LfsLockOwnership
    $candidates = @()
    foreach ($asset in $AssetPaths) {
        if ($ownership.OursByPath.ContainsKey($asset)) {
            continue
        }

        $otherLock = $null
        if ($ownership.TheirsByPath.ContainsKey($asset)) {
            $otherLock = $ownership.TheirsByPath[$asset]
        }

        $ownerName = ""
        $lockId = ""
        if ($null -ne $otherLock) {
            $ownerName = Get-LockOwnerName $otherLock
            $idProperty = $otherLock.PSObject.Properties["id"]
            if ($null -ne $idProperty) {
                $lockId = [string] $idProperty.Value
            }
        }

        $candidates += [pscustomobject]@{
            Path = $asset
            Owner = $ownerName
            LockId = $lockId
        }
    }

    return $candidates
}

# Locks one asset through Git LFS; this is the CLI equivalent of Unreal checkout.
function Invoke-LfsCheckout {
    param([string] $Path)

    $output = @(git -C $repoRoot lfs lock $Path 2>&1)
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Checked out: $Path"
        return
    }

    $detail = (($output | ForEach-Object { [string] $_ }) -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($detail)) {
        throw "Failed to checkout $Path with: git lfs lock $Path"
    }

    throw "Failed to checkout $Path with: git lfs lock $Path`n$detail"
}

# Offers checkout prompts for pushed assets that would fail LFS lock verification.
function Invoke-LfsCheckoutPreflight {
    param(
        [string] $RemoteName,
        [string] $BaseBranch,
        [string] $Branch,
        [switch] $DryRun
    )

    if (-not $DryRun) {
        Update-RemoteBranchRefIfPresent -RemoteName $RemoteName -BranchName $Branch
        Update-RemoteBranchRefIfPresent -RemoteName $RemoteName -BranchName $BaseBranch
    }

    $assets = @(Get-PushedUnrealAssetPaths -RemoteName $RemoteName -BaseBranch $BaseBranch -Branch $Branch)
    if ($assets.Count -eq 0) {
        Write-Success "No pushed Unreal binary asset changes."
        return
    }

    $candidates = @(Get-LfsCheckoutCandidates -AssetPaths $assets)
    if ($candidates.Count -eq 0) {
        Write-Success "Verified Git LFS locks for $($assets.Count) pushed Unreal binary asset(s)."
        return
    }

    Write-WarningMessage "Push would fail Git LFS lock verification for $($candidates.Count) asset(s):"
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate.Owner)) {
            Write-WarningMessage "  - $($candidate.Path) (not checked out by this user)"
        }
        elseif ([string]::IsNullOrWhiteSpace($candidate.LockId)) {
            Write-WarningMessage "  - $($candidate.Path) (locked by $($candidate.Owner))"
        }
        else {
            Write-WarningMessage "  - $($candidate.Path) (locked by $($candidate.Owner), id=$($candidate.LockId))"
        }
    }

    if ($DryRun) {
        Write-Step "Dry run: would ask whether to checkout each listed asset before push."
        return
    }

    $declined = @()
    foreach ($candidate in $candidates) {
        $prompt = "Checkout $($candidate.Path) with git lfs lock? [y/N]"
        if (-not [string]::IsNullOrWhiteSpace($candidate.Owner)) {
            $prompt = "Checkout $($candidate.Path) locked by $($candidate.Owner)? [y/N]"
        }

        $answer = Read-Host $prompt
        if ($answer -match '^(?i:y|yes)$') {
            Invoke-LfsCheckout -Path $candidate.Path
        }
        else {
            $declined += $candidate.Path
        }
    }

    if ($declined.Count -gt 0) {
        $details = ($declined | ForEach-Object { "  - $_" }) -join "`n"
        throw "Push cancelled because these Unreal binary assets are still not checked out by this user:`n$details"
    }

    $remaining = @(Get-LfsCheckoutCandidates -AssetPaths $assets)
    if ($remaining.Count -gt 0) {
        $details = ($remaining | ForEach-Object { "  - $($_.Path)" }) -join "`n"
        throw "Push cancelled because Git LFS lock verification is still missing checkout for:`n$details"
    }

    Write-Success "Verified Git LFS locks for $($assets.Count) pushed Unreal binary asset(s)."
}

$branch = (git -C $repoRoot symbolic-ref --quiet --short HEAD)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
    throw "Detached HEAD is not supported. Checkout a local branch first."
}
$branch = $branch.Trim()

if ($branch -eq $BaseBranch) {
    throw "Refusing to open a PR from $BaseBranch to itself. Checkout a topic branch first."
}

git -C $repoRoot remote get-url $RemoteName | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Git remote not found: $RemoteName"
}

$dirty = @(git -C $repoRoot status --porcelain)
if ($dirty.Count -gt 0) {
    $dirtySummary = ($dirty | Select-Object -First 10) -join "`n"
    if (-not $AllowDirty) {
        throw "Working tree has uncommitted changes. Commit or stash them first, or rerun with -AllowDirty to push only committed branch state.`n$dirtySummary"
    }
    Write-WarningMessage "Working tree has uncommitted changes; they will not be included in the pushed branch."
}

if ($DryRun) {
    Invoke-LfsCheckoutPreflight -RemoteName $RemoteName -BaseBranch $BaseBranch -Branch $branch -DryRun
    Write-Step "Would push: git push -u $RemoteName HEAD:refs/heads/$branch"
    Write-Step "Would check existing open PR: base=$BaseBranch head=$branch"
    Write-Step "Would create PR only when no open PR exists."
    Write-Success "Dry run complete."
    exit 0
}

gh auth status | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI is not authenticated. Run: gh auth login"
}

Invoke-LfsCheckoutPreflight -RemoteName $RemoteName -BaseBranch $BaseBranch -Branch $branch

Write-Step "Pushing $branch to $RemoteName."
git -C $repoRoot push -u $RemoteName "HEAD:refs/heads/$branch"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to push $branch to $RemoteName."
}

$existingJson = gh pr list --base $BaseBranch --head $branch --state open --json number,title,url --limit 1
if ($LASTEXITCODE -ne 0) {
    throw "Failed to query open pull requests."
}

$existingPrs = @($existingJson | ConvertFrom-Json)
if ($existingPrs.Count -gt 0) {
    $pr = $existingPrs[0]
    Write-Success "Open PR already exists: #$($pr.number) $($pr.title)"
    Write-Step "$($pr.url)"
    exit 0
}

$createArgs = @("pr", "create", "--base", $BaseBranch, "--head", $branch)
if ($Draft) {
    $createArgs += "--draft"
}

if ([string]::IsNullOrWhiteSpace($Title) -and [string]::IsNullOrWhiteSpace($Body)) {
    $createArgs += "--fill"
}
else {
    if (-not [string]::IsNullOrWhiteSpace($Title)) {
        $createArgs += @("--title", $Title)
    }
    if (-not [string]::IsNullOrWhiteSpace($Body)) {
        $createArgs += @("--body", $Body)
    }
    else {
        $createArgs += @("--body", "")
    }
}

Write-Step "Creating PR into $BaseBranch."
$createdUrl = & gh @createArgs
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create pull request."
}

Write-Success "Created PR: $createdUrl"
