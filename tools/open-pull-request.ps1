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
Set-ToolPrefix "open-pull-request"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Assert-Command "git"
if (-not $DryRun) {
    Assert-Command "gh"
}

$repoRoot = Get-RepoRoot

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
