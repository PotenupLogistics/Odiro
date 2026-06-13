# Syncs Agents Python development dependencies with uv.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/agents"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$agentsDir = Get-AgentsRoot

if (-not (Test-Path -LiteralPath (Join-Path $agentsDir "pyproject.toml") -PathType Leaf)) {
    throw "Agents pyproject.toml not found: $agentsDir"
}

Assert-Command "uv"
Write-Step "Sync Agents development dependencies"
Invoke-External -WorkingDirectory $agentsDir -FilePath "uv" -Arguments @("sync", "--dev")

Write-Success "Agents install phase complete."
