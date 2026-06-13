# Checks repository setup prerequisites across project-owned phases.
param(
    [switch] $AllowMissing,
    [switch] $PassThru
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "check"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Write-Step "Check repository prerequisites"

$repoRoot = Get-RepoRoot
$issues = @()
$issues += @(& (Join-Path $repoRoot "Agents\tools\check-prerequisites.ps1") -PassThru)
$issues += @(& (Join-Path $repoRoot "Client\Tools\CheckPrerequisites.ps1") -PassThru)

if ($PassThru) {
    return $issues
}

Complete-Prerequisites `
    -Issues $issues `
    -AllowMissing:$AllowMissing `
    -SuccessMessage "Repository prerequisites OK."
