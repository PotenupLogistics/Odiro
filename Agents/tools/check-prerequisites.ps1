# Checks uv availability for Agents Python dependency management.
param(
    [switch] $CheckOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "check/agents"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Write-Step "Check Agents prerequisites"

function Test-UvPrerequisite {
    $uvCommand = Get-Command "uv" -ErrorAction SilentlyContinue
    if ($uvCommand) {
        $uvVersion = (& uv --version) -join " "
        Write-Step "uv: $uvVersion"
        return @()
    }

    return @(
        New-PrerequisiteIssue `
            -Name "uv" `
            -Detail "uv was not found on PATH." `
            -Reason "Agents dependency sync and API server execution use uv commands such as uv sync and uv run." `
            -Install "winget install --id=astral-sh.uv -e" `
            -Verify "Open a new shell, then run: uv --version" `
            -Docs @(
                "docs/guides/development-environment.md#uv",
                "https://docs.astral.sh/uv/getting-started/installation/"
            )
    )
}

$issues = @(Test-UvPrerequisite)

if ($CheckOnly) {
    return $issues
}

Complete-Prerequisites `
    -Issues $issues `
    -AllowMissing `
    -SuccessMessage "Agents prerequisites OK." `
    -ErrorMessage "Agents prerequisites are missing. Install the missing tools."
