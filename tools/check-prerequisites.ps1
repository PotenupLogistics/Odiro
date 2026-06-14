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

# Verifies Git LFS availability for asset locking hooks and setup.
function Test-GitLfsPrerequisite {
    $gitCommand = Get-Command "git" -ErrorAction SilentlyContinue
    if (-not $gitCommand) {
        return @(
            New-PrerequisiteIssue `
                -Name "Git" `
                -Detail "git was not found on PATH." `
                -Reason "Repository setup, hooks, and Git LFS locking require Git." `
                -Install "Install Git for Windows." `
                -Verify "Open a new shell, then run: git --version" `
                -Docs @("docs/guides/development-environment.md#git-setup")
        )
    }

    $gitVersion = (& git --version) -join " "
    Write-Step "git: $gitVersion"

    $lfsVersion = (& git lfs version 2>$null) -join " "
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($lfsVersion)) {
        Write-Step "git-lfs: $lfsVersion"
        return @()
    }

    return @(
        New-PrerequisiteIssue `
            -Name "Git LFS" `
            -Detail "git lfs was not found or is not available from Git." `
            -Reason "Unreal asset checkout uses Git LFS locking and lockable read-only hooks." `
            -Install "winget install --id GitHub.GitLFS -e" `
            -Verify "Open a new shell, then run: git lfs version" `
            -Docs @("docs/guides/development-environment.md#git-setup")
    )
}

$repoRoot = Get-RepoRoot
$issues = @()
$issues += @(Test-GitLfsPrerequisite)
$issues += @(& (Join-Path $repoRoot "Agents\tools\check-prerequisites.ps1") -PassThru)
$issues += @(& (Join-Path $repoRoot "Bridge\tools\check-prerequisites.ps1") -PassThru)
$issues += @(& (Join-Path $repoRoot "Client\Tools\CheckPrerequisites.ps1") -PassThru)

if ($PassThru) {
    return $issues
}

Complete-Prerequisites `
    -Issues $issues `
    -AllowMissing:$AllowMissing `
    -SuccessMessage "Repository prerequisites OK."
