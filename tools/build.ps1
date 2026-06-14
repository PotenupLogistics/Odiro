# Builds repository projects that require a build step.
param(
    [string] $Target = "all"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "build"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot

if ($Target -ne "all" -and $Target -ne "bridge" -and $Target -ne "client") {
    throw "Invalid build target: $Target. Valid targets: all, bridge, client. Agents is a Python server and has no build task; use setup/run/dev tasks instead."
}

if ($Target -eq "all" -or $Target -eq "bridge") {
    $bridgeBuild = Join-Path $repoRoot "Bridge\tools\build.ps1"
    if (-not (Test-Path -LiteralPath $bridgeBuild -PathType Leaf)) {
        throw "Bridge build script not found: $bridgeBuild"
    }

    & $bridgeBuild
}

if ($Target -eq "all" -or $Target -eq "client") {
    $clientBuild = Join-Path $repoRoot "Client\Tools\Build.ps1"
    if (-not (Test-Path -LiteralPath $clientBuild -PathType Leaf)) {
        throw "Client build script not found: $clientBuild"
    }

    & $clientBuild
}

Write-Success "Build complete."
