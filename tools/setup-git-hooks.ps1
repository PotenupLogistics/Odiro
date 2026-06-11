$ErrorActionPreference = "Stop"

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
Write-Host "[git] core.hooksPath=.githooks" -ForegroundColor Cyan
