# Checks Go availability for Bridge build and tests.
param(
    # Treat missing prerequisites as warnings.
    [switch] $AllowMissing,
    # Return issue objects for the caller to handle.
    [switch] $PassThru
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot "common.ps1")
Set-ToolPrefix "check/bridge"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Write-Step "Check Bridge prerequisites"

# Verifies that the Go toolchain is available for Bridge builds.
function Test-GoPrerequisite {
    $goCommand = Get-Command "go" -ErrorAction SilentlyContinue
    if ($goCommand) {
        $goVersion = (& go version) -join " "
        Write-Step "go: $goVersion"
        return @()
    }

    $issue = @{
        Name = "Go"
        Detail = "go was not found on PATH."
        Reason = "Bridge builds and IPC tests use the Go toolchain."
        Install = "winget install --id GoLang.Go -e --source winget"
        Verify = "Open a new shell, then run: go version"
        Docs = @(
            "docs/guides/development-environment.md#go",
            "https://go.dev/doc/install"
        )
    }
    return @(New-PrerequisiteIssue @issue)
}

$issues = @(Test-GoPrerequisite)

if ($PassThru) {
    return $issues
}

Complete-Prerequisites `
    -Issues $issues `
    -AllowMissing:$AllowMissing `
    -SuccessMessage "Bridge prerequisites OK." `
    -ErrorMessage "Bridge prerequisites are missing. Install Go, or rerun Bridge setup with -AllowMissingPrerequisites to continue anyway."
