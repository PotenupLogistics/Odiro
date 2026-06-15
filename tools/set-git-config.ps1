# Configures and checks repository-local Git, hook, and Git LFS locking settings.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/git-config"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Assert-Command "git"

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) {
    throw "Not inside a Git repository."
}

$repoRoot = $repoRoot.Trim()
if ($repoRoot.Length -eq 0) {
    throw "Not inside a Git repository."
}

# Reads a local Git config value without treating a missing key as fatal.
function Get-LocalGitConfig {
    param([string] $Name)

    $value = @(git -C $repoRoot config --local --get $Name)
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return (($value -join "`n").Trim())
}

# Reads a config value from a repository config file.
function Get-GitConfigFromFile {
    param(
        [string] $File,
        [string] $Name
    )

    $value = @(git -C $repoRoot config --file $File --get $Name)
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return (($value -join "`n").Trim())
}

# Sets one local Git config key and logs whether it was already correct.
function Set-ExpectedLocalGitConfig {
    param(
        [string] $Name,
        [string] $Expected
    )

    $actual = Get-LocalGitConfig -Name $Name
    if ($actual -eq $Expected) {
        Write-Step "Already configured: $Name=$Expected"
        return
    }

    git -C $repoRoot config --local $Name $Expected
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure $Name."
    }

    if ([string]::IsNullOrWhiteSpace($actual)) {
        Write-Step "Configured: $Name=$Expected"
    }
    else {
        Write-Step "Updated: $Name '$actual' -> '$Expected'"
    }
}

# Checks one expected Git attribute set for Unreal binary assets.
function Assert-UnrealAssetAttributes {
    param([string] $Path)

    $output = @(git -C $repoRoot check-attr filter lockable text eol diff merge -- $Path)
    if ($LASTEXITCODE -ne 0) {
        throw "git check-attr failed for $Path."
    }

    $actualByName = @{}
    foreach ($line in $output) {
        $parts = $line -split ":", 3
        if ($parts.Count -ge 3) {
            $actualByName[$parts[1].Trim()] = $parts[2].Trim()
        }
    }

    $expected = @{
        filter = "unspecified"
        lockable = "set"
        text = "unset"
        eol = "unset"
        diff = "unset"
        merge = "unset"
    }

    foreach ($name in $expected.Keys) {
        $actual = $actualByName[$name]
        if ($actual -ne $expected[$name]) {
            throw "Expected $Path $name=$($expected[$name]), got '$actual'."
        }
    }

    Write-Step "Attributes OK: $Path"
}

git -C $repoRoot lfs version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Git LFS was not found. Install git-lfs and rerun setup."
}
Write-Step "Git LFS available."

Set-ExpectedLocalGitConfig -Name "core.hooksPath" -Expected ".githooks"

git -C $repoRoot lfs install --local --manual | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to initialize Git LFS local config."
}
Write-Step "Git LFS local config checked."

git -C $repoRoot lfs update --manual | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to verify Git LFS hook snippets."
}
Write-Step "Git LFS hook snippets verified."

Set-ExpectedLocalGitConfig -Name "merge.ff" -Expected "false"
Set-ExpectedLocalGitConfig -Name "pull.ff" -Expected "true"
Set-ExpectedLocalGitConfig -Name "pull.rebase" -Expected "true"
Set-ExpectedLocalGitConfig -Name "rebase.autoStash" -Expected "true"
Set-ExpectedLocalGitConfig -Name "branch.autoSetupRebase" -Expected "always"
Set-ExpectedLocalGitConfig -Name "lfs.locksverify" -Expected "true"
Set-ExpectedLocalGitConfig -Name "lfs.setlockablereadonly" -Expected "true"

$lfsConfig = Join-Path $repoRoot ".lfsconfig"
if (-not (Test-Path -LiteralPath $lfsConfig -PathType Leaf)) {
    throw ".lfsconfig is missing."
}
if ((Get-GitConfigFromFile -File $lfsConfig -Name "lfs.locksverify") -ne "true") {
    throw ".lfsconfig must set lfs.locksverify=true."
}
Write-Step ".lfsconfig OK: lfs.locksverify=true"

Assert-UnrealAssetAttributes -Path "*.uasset"
Assert-UnrealAssetAttributes -Path "*.umap"

$head = git -C $repoRoot rev-parse --verify HEAD
if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($head)) {
    $headOid = $head.Trim()
    git -C $repoRoot lfs post-checkout 0000000000000000000000000000000000000000 $headOid 1
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply read-only state to current lockable files."
    }
    Write-Step "Git LFS lockable read-only state applied to current checkout."
}

Write-Success "Git config complete."
