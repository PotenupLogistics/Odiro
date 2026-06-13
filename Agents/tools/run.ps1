# Runs the Agents API server for product-like local execution.
param(
    [int] $Port = 8711,
    [switch] $Background,
    [string] $LogPrefix = "run",
    [string] $ArtifactOutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "run/agents"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

if ($Background) {
    Start-AgentsApi -Port $Port -LogPrefix $LogPrefix -ArtifactOutputDir $ArtifactOutputDir
    return
}

Invoke-AgentsApiForeground -Port $Port -ArtifactOutputDir $ArtifactOutputDir
