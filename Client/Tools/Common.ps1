# Shared helper functions for Client development scripts.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:ClientRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$script:ToolPrefix = "client"

function Get-ClientRoot {
    return $script:ClientRoot
}

function Get-ClientProjectFile {
    $projectFile = Join-Path (Get-ClientRoot) "ProtoRobotSim.uproject"
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "Client project file not found: $projectFile"
    }
    return $projectFile
}

function Write-ToolMessage {
    param(
        [string] $Message,
        [string] $Prefix = "",
        [ConsoleColor] $Color = [ConsoleColor]::White
    )

    if ([string]::IsNullOrWhiteSpace($Prefix)) {
        $Prefix = $script:ToolPrefix
    }

    Write-Host "[$Prefix] $Message" -ForegroundColor $Color
}

function Set-ToolPrefix {
    param([string] $Prefix)
    $script:ToolPrefix = $Prefix
}

function Write-Step {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Cyan
}

function Write-Success {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Green
}

function Write-WarningMessage {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Yellow
}

function Write-ErrorMessage {
    param([string] $Message)
    Write-ToolMessage -Message "ERROR: $Message" -Color Red
}

function New-PrerequisiteIssue {
    param(
        [string] $Name,
        [string] $Detail,
        [string] $Reason = "",
        [string[]] $MissingItems = @(),
        [string] $Install,
        [string] $Verify = "",
        [string[]] $Docs = @()
    )

    return [pscustomobject]@{
        Name = $Name
        Detail = $Detail
        Reason = $Reason
        MissingItems = $MissingItems
        Install = $Install
        Verify = $Verify
        Docs = $Docs
    }
}

function Get-IssueValue {
    param(
        [object] $Issue,
        [string] $PropertyName,
        [object] $DefaultValue = ""
    )

    $property = $Issue.PSObject.Properties[$PropertyName]
    if ($property) {
        return $property.Value
    }
    return $DefaultValue
}

function Complete-Prerequisites {
    param(
        [object[]] $Issues,
        [switch] $AllowMissing,
        [string] $SuccessMessage = "Setup prerequisites OK.",
        [string] $ErrorMessage = "Setup prerequisites are missing. Install the missing tools, or rerun setup with -AllowMissingPrerequisites to continue anyway."
    )

    if ($Issues.Count -eq 0) {
        Write-Success $SuccessMessage
        return
    }

    Write-WarningMessage "Missing setup prerequisites:"
    foreach ($issue in $Issues) {
        Write-WarningMessage "- $(Get-IssueValue -Issue $issue -PropertyName "Name")"

        $detail = Get-IssueValue -Issue $issue -PropertyName "Detail"
        if (-not [string]::IsNullOrWhiteSpace($detail)) {
            Write-WarningMessage "  Status: $detail"
        }

        $reason = Get-IssueValue -Issue $issue -PropertyName "Reason"
        if (-not [string]::IsNullOrWhiteSpace($reason)) {
            Write-WarningMessage "  Required for: $reason"
        }

        $missingItems = @(Get-IssueValue -Issue $issue -PropertyName "MissingItems" -DefaultValue @())
        if ($missingItems.Count -gt 0) {
            Write-WarningMessage "  Missing items:"
            foreach ($item in $missingItems) {
                Write-WarningMessage "    - $item"
            }
        }

        $install = Get-IssueValue -Issue $issue -PropertyName "Install"
        if (-not [string]::IsNullOrWhiteSpace($install)) {
            Write-Step "  Install: $install"
        }

        $verify = Get-IssueValue -Issue $issue -PropertyName "Verify"
        if (-not [string]::IsNullOrWhiteSpace($verify)) {
            Write-Step "  Verify: $verify"
        }

        $docs = @(Get-IssueValue -Issue $issue -PropertyName "Docs" -DefaultValue @()) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
        foreach ($doc in $docs) {
            Write-Step "  Docs: $doc"
        }
    }

    if ($AllowMissing) {
        Write-WarningMessage "Continuing because -AllowMissing was set."
        return
    }

    throw $ErrorMessage
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string] $WorkingDirectory,
        [Parameter(Mandatory = $true)][string] $FilePath,
        [string[]] $Arguments = @()
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$FilePath failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Join-ProcessArguments {
    param([object[]] $Arguments)

    return ($Arguments | ForEach-Object {
        $value = [string] $_
        if ($value -notmatch '[\s"]') {
            $value
        }
        else {
            '"' + $value.Replace('"', '\"') + '"'
        }
    }) -join " "
}

function Get-EngineAssociation {
    param([string] $ProjectFile)
    $project = Get-Content -Raw -LiteralPath $ProjectFile | ConvertFrom-Json
    if (-not $project.EngineAssociation) {
        return $null
    }
    return [string] $project.EngineAssociation
}

function Resolve-ToolInDirectory {
    param(
        [string] $Directory,
        [string[]] $RelativePaths
    )

    if (-not $Directory) {
        return $null
    }

    foreach ($relativePath in $RelativePaths) {
        $candidate = Join-Path $Directory $relativePath
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Resolve-ToolNearPath {
    param(
        [string] $StartPath,
        [string[]] $RelativePaths
    )

    if (-not $StartPath -or -not (Test-Path -LiteralPath $StartPath)) {
        return $null
    }

    $current = Get-Item -LiteralPath $StartPath
    if (-not $current.PSIsContainer) {
        $current = $current.Directory
    }

    while ($null -ne $current) {
        $resolved = Resolve-ToolInDirectory -Directory $current.FullName -RelativePaths $RelativePaths
        if ($resolved) {
            return $resolved
        }
        $current = $current.Parent
    }

    return $null
}

function Resolve-UnrealRegistryDirectory {
    param([string] $EngineAssociation)

    if (-not $EngineAssociation) {
        return $null
    }

    $hkcuBuilds = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
    if (Test-Path -LiteralPath $hkcuBuilds) {
        $builds = Get-ItemProperty -LiteralPath $hkcuBuilds
        $value = $builds.PSObject.Properties[$EngineAssociation]
        if ($value -and (Test-Path -LiteralPath ([string] $value.Value))) {
            return (Resolve-Path -LiteralPath ([string] $value.Value)).Path
        }
    }

    $installKeys = @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$EngineAssociation",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$EngineAssociation"
    )

    foreach ($key in $installKeys) {
        if (-not (Test-Path -LiteralPath $key)) {
            continue
        }
        $value = (Get-ItemProperty -LiteralPath $key).InstalledDirectory
        if ($value -and (Test-Path -LiteralPath ([string] $value))) {
            return (Resolve-Path -LiteralPath ([string] $value)).Path
        }
    }

    $programFiles = [Environment]::GetFolderPath("ProgramFiles")
    $defaultInstall = Join-Path $programFiles "Epic Games\UE_$EngineAssociation"
    if (Test-Path -LiteralPath $defaultInstall) {
        return (Resolve-Path -LiteralPath $defaultInstall).Path
    }

    return $null
}

function Resolve-UnrealBuildBatch {
    param([string] $ProjectFile)

    $relativePaths = @(
        "Build\BatchFiles\Build.bat",
        "Engine\Build\BatchFiles\Build.bat"
    )

    if ($env:UE_BUILD_BAT -and (Test-Path -LiteralPath $env:UE_BUILD_BAT -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:UE_BUILD_BAT).Path
    }

    $fromEngineDir = Resolve-ToolInDirectory -Directory $env:UE_ENGINE_DIR -RelativePaths $relativePaths
    if ($fromEngineDir) {
        return $fromEngineDir
    }

    $fromEditor = Resolve-ToolNearPath -StartPath $env:UE_EDITOR_EXE -RelativePaths $relativePaths
    if ($fromEditor) {
        return $fromEditor
    }

    $pathEditor = Get-Command "UnrealEditor.exe" -ErrorAction SilentlyContinue
    if ($pathEditor) {
        $fromPathEditor = Resolve-ToolNearPath -StartPath $pathEditor.Source -RelativePaths $relativePaths
        if ($fromPathEditor) {
            return $fromPathEditor
        }
    }

    $engineAssociation = Get-EngineAssociation -ProjectFile $ProjectFile
    $engineDir = Resolve-UnrealRegistryDirectory -EngineAssociation $engineAssociation
    $fromRegistry = Resolve-ToolInDirectory -Directory $engineDir -RelativePaths $relativePaths
    if ($fromRegistry) {
        return $fromRegistry
    }

    throw "Unreal Build.bat was not found. Set UE_BUILD_BAT, UE_ENGINE_DIR, UE_EDITOR_EXE, or check EngineAssociation."
}

function Resolve-UnrealEditor {
    param([string] $ProjectFile)

    $relativePaths = @(
        "Binaries\Win64\UnrealEditor.exe",
        "Engine\Binaries\Win64\UnrealEditor.exe"
    )

    if ($env:UE_EDITOR_EXE -and (Test-Path -LiteralPath $env:UE_EDITOR_EXE -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:UE_EDITOR_EXE).Path
    }

    $fromEngineDir = Resolve-ToolInDirectory -Directory $env:UE_ENGINE_DIR -RelativePaths $relativePaths
    if ($fromEngineDir) {
        return $fromEngineDir
    }

    $pathEditor = Get-Command "UnrealEditor.exe" -ErrorAction SilentlyContinue
    if ($pathEditor) {
        return $pathEditor.Source
    }

    $engineAssociation = Get-EngineAssociation -ProjectFile $ProjectFile
    $engineDir = Resolve-UnrealRegistryDirectory -EngineAssociation $engineAssociation
    $fromRegistry = Resolve-ToolInDirectory -Directory $engineDir -RelativePaths $relativePaths
    if ($fromRegistry) {
        return $fromRegistry
    }

    throw "UnrealEditor.exe was not found. Set UE_EDITOR_EXE, UE_ENGINE_DIR, add UnrealEditor.exe to PATH, or check EngineAssociation."
}
