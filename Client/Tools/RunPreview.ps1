# Launches the Unreal project in game preview mode without packaging.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\Common.ps1"
Set-ToolPrefix "run/client"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$clientDir = Get-ClientRoot
$projectFile = Get-ClientProjectFile
$unrealEditor = Resolve-UnrealEditor -ProjectFile $projectFile
$previewWindowArgs = @("-windowed", "-ResX=1920", "-ResY=1080")
$previewArgs = @($args)
$arguments = @($projectFile, "-game", "-NoSplash") + $previewWindowArgs + $previewArgs

Write-Step "UnrealEditor: $unrealEditor"
Write-Step "Project: $projectFile"
Write-Step "Preview window args: $($previewWindowArgs -join ' ')"
Write-Step "Packaged-style args: $($previewArgs -join ' ')"

$process = Start-Process `
    -FilePath $unrealEditor `
    -ArgumentList (Join-ProcessArguments -Arguments $arguments) `
    -WorkingDirectory $clientDir `
    -Wait `
    -PassThru

if ($process.ExitCode -ne 0) {
    throw "Client preview exited with code $($process.ExitCode)."
}
