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

# Runs a project PowerShell script and fails this install phase on non-zero exit.
function Invoke-InstallPhaseScript {
    param(
        [string] $Path,
        [string] $FailureMessage
    )

    $global:LASTEXITCODE = 0
    & $Path
    $scriptSucceeded = $?
    $scriptExitCode = $LASTEXITCODE
    if (-not $scriptSucceeded -or $scriptExitCode -ne 0) {
        throw $FailureMessage
    }
}

if (Test-Path -LiteralPath (Join-Path $repoRoot ".gitmodules") -PathType Leaf) {
    git -C $repoRoot submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize Git submodules."
    }
}

Invoke-InstallPhaseScript `
    -Path "$PSScriptRoot\sync-ide-run-configs.ps1" `
    -FailureMessage "IDE run config setup failed."

if (-not $SkipGitHooks) {
    Invoke-InstallPhaseScript `
        -Path "$PSScriptRoot\set-git-config.ps1" `
        -FailureMessage "Git config setup failed."
}

if (-not $SkipAgents) {
    Invoke-InstallPhaseScript `
        -Path (Join-Path $repoRoot "Agents\tools\install.ps1") `
        -FailureMessage "Agents install phase failed."
}

Write-Success "Install phase complete."
