# Runs the Agents API server for development, using uvicorn reload by default.
param(
    [int] $Port = 8711,
    [switch] $NoReload,
    [switch] $Background,
    [string] $LogPrefix = "dev",
    [string] $ArtifactOutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "dev/agents"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$reload = -not $NoReload

if ($Background) {
    Start-AgentsApi -Port $Port -Reload:$reload -LogPrefix $LogPrefix -ArtifactOutputDir $ArtifactOutputDir
    return
}

Invoke-AgentsApiForeground -Port $Port -Reload:$reload -ArtifactOutputDir $ArtifactOutputDir
