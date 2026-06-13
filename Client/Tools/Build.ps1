# Builds the Unreal Editor target for the Client project.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\Common.ps1"
Set-ToolPrefix "build/client"

$clientDir = Get-ClientRoot
$projectFile = Get-ClientProjectFile
$buildBat = Resolve-UnrealBuildBatch -ProjectFile $projectFile

Write-Step "Build Client editor target"
Write-Step "Unreal Build.bat: $buildBat"
Invoke-External `
    -WorkingDirectory $clientDir `
    -FilePath $buildBat `
    -Arguments @(
        "ProtoRobotSimEditor",
        "Win64",
        "Development",
        "-Project=$projectFile",
        "-WaitMutex",
        "-FromMsBuild"
    )
