# Runs repository setup install tasks after prerequisites are satisfied.
param(
    [switch] $SkipGitHooks,
    [switch] $SkipAgents
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot

if (Test-Path -LiteralPath (Join-Path $repoRoot ".gitmodules") -PathType Leaf) {
    git -C $repoRoot submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize Git submodules."
    }
    Write-Step "Git submodules initialized."
}

if (-not $SkipGitHooks) {
    & "$PSScriptRoot\set-git-config.ps1"
}

if (-not $SkipAgents) {
    & (Join-Path $repoRoot "Agents\tools\install.ps1")
}

Write-Success "Install phase complete."
