# Configures the repository to use the checked-in Git hooks directory.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/git-hooks"

Assert-Command "git"

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) {
    throw "Not inside a Git repository."
}

$repoRoot = $repoRoot.Trim()
if ($repoRoot.Length -eq 0) {
    throw "Not inside a Git repository."
}

git -C $repoRoot config --local core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure core.hooksPath."
}
Write-Step "Git hooks configured: core.hooksPath=.githooks"
