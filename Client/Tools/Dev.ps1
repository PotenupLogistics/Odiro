# Opens the Unreal Editor for Client development and Live Coding.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\Common.ps1"
Set-ToolPrefix "dev/client"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$clientDir = Get-ClientRoot
. (Join-Path $clientDir "Plugins\UE_MCP_Bridge\Resources\Automation\CrashReportTools.ps1")
$projectFile = Get-ClientProjectFile
$unrealEditor = Resolve-UnrealEditor -ProjectFile $projectFile
$noWait = $false
$editorArgs = @()
foreach ($argument in $args) {
    if ($argument -eq "-NoWait") {
        $noWait = $true
        continue
    }
    $editorArgs += $argument
}
$arguments = @($projectFile) + $editorArgs

Write-Step "UnrealEditor: $unrealEditor"
Write-Step "Project: $projectFile"
Write-Step "Editor args: $($editorArgs -join ' ')"
Assert-NoPendingUnrealCrashReportClients

if ($noWait) {
    $process = Start-Process `
        -FilePath $unrealEditor `
        -ArgumentList (Join-ProcessArguments -Arguments $arguments) `
        -WorkingDirectory $clientDir `
        -PassThru

    Write-Step "Unreal Editor pid: $($process.Id)"
    return
}

$process = Start-Process `
    -FilePath $unrealEditor `
    -ArgumentList (Join-ProcessArguments -Arguments $arguments) `
    -WorkingDirectory $clientDir `
    -Wait `
    -PassThru

if ($process.ExitCode -ne 0) {
    throw "Unreal Editor exited with code $($process.ExitCode)."
}
