# Builds the Unreal Editor target for the Client project.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\Common.ps1"
Set-ToolPrefix "build/client"

$clientDir = Get-ClientRoot
$projectFile = Get-ClientProjectFile
$buildBat = Resolve-UnrealBuildBatch -ProjectFile $projectFile
$bridgePortFile = Join-Path $clientDir "Saved\UE_MCP_Bridge\port.json"
if (Test-Path -LiteralPath $bridgePortFile -PathType Leaf) {
    try {
        $bridgePort = Get-Content -LiteralPath $bridgePortFile -Raw | ConvertFrom-Json
        $bridgePid = $bridgePort.PSObject.Properties["pid"]
        if ($bridgePid -and (Get-Process -Id ([int] $bridgePid.Value) -ErrorAction SilentlyContinue)) {
            Write-WarningMessage "Unreal Editor appears open. If Live Coding is active, use Editor Reload MCP before this build wrapper."
        }
    }
    catch {
        Write-WarningMessage "Unable to inspect UE_MCP_Bridge port state: $($_.Exception.Message)"
    }
}

Write-Step "Build Client editor target"
Write-Step "Unreal Build.bat: $buildBat"
Invoke-External `
    -WorkingDirectory $clientDir `
    -FilePath $buildBat `
    -Arguments @(
        "OdiroSimEditor",
        "Win64",
        "Development",
        "-Project=$projectFile",
        "-WaitMutex",
        "-FromMsBuild"
    )
