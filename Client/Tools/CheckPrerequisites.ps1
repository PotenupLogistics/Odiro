# Checks Client Unreal Engine and Visual Studio prerequisites.
param(
    [switch] $AllowMissing,
    [switch] $PassThru
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\Common.ps1"
Set-ToolPrefix "check/client"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$RequiredUnrealEngineVersion = "5.7"
$MinimumVisualStudioVersion = [version]"17.8"
$RecommendedVisualStudioVersion = [version]"17.14"
$MinimumWindowsSdkVersion = [version]"10.0.19041.0"

$UnrealDocs = @(
    "docs/guides/development-environment.md#unreal-engine-57",
    "https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine",
    "https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine"
)

$VisualStudioDocs = @(
    "docs/guides/development-environment.md#visual-studio-2022",
    "https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine",
    "https://learn.microsoft.com/en-us/visualstudio/gamedev/unreal/get-started/vs-tools-unreal-install",
    "https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-professional"
)

$RequiredVisualStudioItems = @(
    [pscustomobject]@{
        Id = "Microsoft.VisualStudio.Workload.NativeGame"
        Name = "Game development with C++ workload"
        Purpose = "core workload for Unreal C++ development on Windows"
    },
    [pscustomobject]@{
        Id = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
        Name = "MSVC v143 x64/x86 build tools"
        Purpose = "C++ compiler and tools used by Unreal Build Tool for Win64 targets"
    },
    [pscustomobject]@{
        Id = "Component.Unreal.Ide"
        Name = "Visual Studio Tools for Unreal Engine"
        Purpose = "Unreal C++ authoring, macro expansion, logging, and class/module tooling in Visual Studio"
    },
    [pscustomobject]@{
        Id = "Component.Unreal.Debugger"
        Name = "Visual Studio debugger tools for Unreal Engine Blueprints"
        Purpose = "Blueprint-aware debugging from Visual Studio"
    },
    [pscustomobject]@{
        Id = "Microsoft.VisualStudio.Component.Unreal.TestAdapter"
        Name = "Unreal Engine Test Adapter"
        Purpose = "Unreal test discovery and execution from Visual Studio"
    }
)

function Format-VisualStudioItem {
    param([object] $Item)
    return "$($Item.Name) ($($Item.Id)) - $($Item.Purpose)"
}

function Get-VSWherePath {
    $candidate = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return $candidate
    }
    return $null
}

function Get-VisualStudioInstallations {
    param([string] $VSWhere)

    if (-not $VSWhere) {
        return @()
    }

    $raw = & $VSWhere -products * -version "[17.0,18.0)" -format json -utf8
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($raw)) {
        return @()
    }

    $parsed = $raw | ConvertFrom-Json
    if ($null -eq $parsed) {
        return @()
    }
    if ($parsed -is [array]) {
        return $parsed
    }
    return @($parsed)
}

function Test-VisualStudioItem {
    param(
        [string] $VSWhere,
        [string] $Id
    )

    if (-not $VSWhere) {
        return $false
    }

    $matches = & $VSWhere -products * -version "[17.0,18.0)" -requires $Id -format value -property installationPath
    return $LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace(($matches -join ""))
}

function Get-WindowsSdkVersions {
    $includeRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Include"
    if (-not (Test-Path -LiteralPath $includeRoot -PathType Container)) {
        return @()
    }

    $versions = @()
    foreach ($directory in Get-ChildItem -LiteralPath $includeRoot -Directory) {
        try {
            $versions += [version] $directory.Name
        }
        catch {
        }
    }
    return $versions | Sort-Object -Descending
}

function Get-VisualStudioInstallCommand {
    param(
        [object[]] $MissingItems,
        [object] $VisualStudio
    )

    $addArguments = ($MissingItems | ForEach-Object { "--add $($_.Id)" }) -join " "
    if ([string]::IsNullOrWhiteSpace($addArguments)) {
        return ""
    }

    if ($VisualStudio) {
        $installer = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vs_installer.exe"
        return "`"$installer`" modify --installPath `"$($VisualStudio.installationPath)`" $addArguments --includeRecommended --passive --norestart"
    }

    return "winget install --id Microsoft.VisualStudio.2022.Community -e --override `"$addArguments --includeRecommended --passive --norestart`""
}

function Test-UnrealEnginePrerequisite {
    $projectFile = Get-ClientProjectFile
    $engineAssociation = Get-EngineAssociation -ProjectFile $projectFile
    if ($engineAssociation -ne $RequiredUnrealEngineVersion) {
        return @(
            New-PrerequisiteIssue `
                -Name "Unreal Engine $RequiredUnrealEngineVersion" `
                -Detail "Client/OdiroSim.uproject EngineAssociation is '$engineAssociation'." `
                -Reason "Client build, run preview, and editor development scripts resolve UE $RequiredUnrealEngineVersion tools from this project association." `
                -Install "Install Unreal Engine $RequiredUnrealEngineVersion with Epic Games Launcher, then keep Client/OdiroSim.uproject EngineAssociation=$RequiredUnrealEngineVersion." `
                -Verify ".\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
                -Docs $UnrealDocs
        )
    }

    try {
        $unrealEditor = Resolve-UnrealEditor -ProjectFile $projectFile
        Write-Step "Unreal Engine ${RequiredUnrealEngineVersion}: $unrealEditor"
        return @()
    }
    catch {
        return @(
            New-PrerequisiteIssue `
                -Name "Unreal Engine $RequiredUnrealEngineVersion" `
                -Detail $_.Exception.Message `
                -Reason "Client build, run preview, and editor development scripts need UnrealEditor.exe and Engine\Build\BatchFiles\Build.bat." `
                -Install "Install Unreal Engine $RequiredUnrealEngineVersion in Epic Games Launcher, or set UE_EDITOR_EXE / UE_ENGINE_DIR." `
                -Verify ".\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
                -Docs $UnrealDocs
        )
    }
}

function Test-VisualStudioPrerequisite {
    $issues = @()
    $vswhere = Get-VSWherePath
    $visualStudioInstallations = Get-VisualStudioInstallations -VSWhere $vswhere
    $visualStudio = $visualStudioInstallations |
        Sort-Object { [version] $_.installationVersion } -Descending |
        Select-Object -First 1

    if (-not $visualStudio) {
        $issues += New-PrerequisiteIssue `
            -Name "Visual Studio 2022" `
            -Detail "Visual Studio 2022 17.8 or later was not found." `
            -Reason "UE 5.7 Windows C++ builds need the VS 2022 toolchain; 17.14 is recommended for this repo." `
            -MissingItems ($RequiredVisualStudioItems | ForEach-Object { Format-VisualStudioItem -Item $_ }) `
            -Install (Get-VisualStudioInstallCommand -MissingItems $RequiredVisualStudioItems -VisualStudio $null) `
            -Verify "Open a new shell, then run: .\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
            -Docs $VisualStudioDocs
    }
    else {
        $visualStudioVersion = [version] $visualStudio.installationVersion
        Write-Step "Visual Studio 2022: $($visualStudio.displayName) $visualStudioVersion"

        if ($visualStudioVersion -lt $MinimumVisualStudioVersion) {
            $issues += New-PrerequisiteIssue `
                -Name "Visual Studio 2022 $MinimumVisualStudioVersion+" `
                -Detail "Installed Visual Studio version is $visualStudioVersion. UE 5.7 expects 17.8+; 17.14 is recommended." `
                -Reason "UE 5.7 binary integration does not support older VS 2022 versions." `
                -Install "Open Visual Studio Installer and update Visual Studio 2022 to $RecommendedVisualStudioVersion or later." `
                -Verify "Open a new shell, then run: .\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
                -Docs $VisualStudioDocs
        }

        $missingVisualStudioItems = @()
        foreach ($item in $RequiredVisualStudioItems) {
            if (-not (Test-VisualStudioItem -VSWhere $vswhere -Id $item.Id)) {
                $missingVisualStudioItems += [pscustomobject] $item
            }
        }

        if ($missingVisualStudioItems.Count -gt 0) {
            $issues += New-PrerequisiteIssue `
                -Name "Visual Studio 2022 Unreal C++ components" `
                -Detail "Visual Studio is installed, but this repo's Windows Unreal C++ development items are missing. Android, iOS, macOS, and .NET MAUI workloads are not part of this check." `
                -Reason "Client build/run/dev tasks target Windows Unreal C++ development, Visual Studio Unreal tooling, and Unreal test/debug integration." `
                -MissingItems ($missingVisualStudioItems | ForEach-Object { Format-VisualStudioItem -Item $_ }) `
                -Install (Get-VisualStudioInstallCommand -MissingItems $missingVisualStudioItems -VisualStudio $visualStudio) `
                -Verify "Open a new shell, then run: .\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
                -Docs $VisualStudioDocs
        }
    }

    $windowsSdkVersions = Get-WindowsSdkVersions
    $latestWindowsSdk = $windowsSdkVersions | Select-Object -First 1
    if (-not $latestWindowsSdk -or $latestWindowsSdk -lt $MinimumWindowsSdkVersion) {
        $installedSdkVersions = if ($windowsSdkVersions.Count -gt 0) {
            $windowsSdkVersions -join ", "
        }
        else {
            "none found"
        }

        $issues += New-PrerequisiteIssue `
            -Name "Windows SDK $MinimumWindowsSdkVersion+" `
            -Detail "Installed Windows SDK versions: $installedSdkVersions" `
            -Reason "Unreal Build Tool needs a Windows SDK when compiling the Win64 Client target." `
            -Install "Install Windows 10 SDK 10.0.19041.0+ or Windows 11 SDK from Visual Studio Installer." `
            -Verify "Open a new shell, then run: .\Client\Tools\CheckPrerequisites.ps1 -AllowMissing" `
            -Docs $VisualStudioDocs
    }
    else {
        Write-Step "Windows SDK: $latestWindowsSdk"
    }

    return $issues
}

Write-Step "Check Client prerequisites"

$issues = @()
$issues += @(Test-UnrealEnginePrerequisite)
$issues += @(Test-VisualStudioPrerequisite)

if ($PassThru) {
    return $issues
}

Complete-Prerequisites `
    -Issues $issues `
    -AllowMissing:$AllowMissing `
    -SuccessMessage "Client prerequisites OK." `
    -ErrorMessage "Client prerequisites are missing. Install the missing tools, or rerun Client setup with -AllowMissingPrerequisites to continue anyway."
