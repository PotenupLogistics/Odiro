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

Push-Location $agentsDir
try {
    & uv sync --check --dev --quiet
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Agents install phase complete."
        return
    }
}
finally {
    Pop-Location
}

Write-Step "Sync Agents development dependencies"
Invoke-External -WorkingDirectory $agentsDir -FilePath "uv" -Arguments @("sync", "--dev")

Write-Success "Agents install phase complete."
