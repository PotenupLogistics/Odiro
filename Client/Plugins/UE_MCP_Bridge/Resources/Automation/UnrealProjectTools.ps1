# Plugin-local Unreal project/tool resolution helpers.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Returns the single .uproject file owned by a project root.
function Get-ReloadProjectFile {
    param([Parameter(Mandatory = $true)][string] $ProjectRoot)

    $root = (Resolve-Path -LiteralPath $ProjectRoot).Path
    $projectFiles = @(Get-ChildItem -LiteralPath $root -Filter "*.uproject" -File)
    if ($projectFiles.Count -eq 0) {
        throw "Unreal project file not found under project root: $root"
    }
    if ($projectFiles.Count -gt 1) {
        $names = ($projectFiles | ForEach-Object { $_.FullName }) -join ", "
        throw "Multiple Unreal project files found under project root: $names"
    }
    return $projectFiles[0].FullName
}

# Returns the editor target name implied by a project file name.
function Get-ReloadEditorTargetName {
    param([Parameter(Mandatory = $true)][string] $ProjectFile)

    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)
    return ("{0}Editor" -f $projectName)
}

# Joins process arguments with Windows-safe quoting.
function Join-ReloadProcessArguments {
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

# Reads EngineAssociation from a .uproject descriptor.
function Get-ReloadEngineAssociation {
    param([Parameter(Mandatory = $true)][string] $ProjectFile)

    $project = Get-Content -Raw -LiteralPath $ProjectFile | ConvertFrom-Json
    $property = $project.PSObject.Properties["EngineAssociation"]
    if ($property -and $property.Value) {
        return [string] $property.Value
    }
    return $null
}

# Resolves a tool path from a base directory and candidate relative paths.
function Resolve-ReloadToolInDirectory {
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

# Walks upward from a path to resolve a nearby tool path.
function Resolve-ReloadToolNearPath {
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
        $resolved = Resolve-ReloadToolInDirectory -Directory $current.FullName -RelativePaths $RelativePaths
        if ($resolved) {
            return $resolved
        }
        $current = $current.Parent
    }

    return $null
}

# Resolves an installed Unreal Engine directory from registry and default paths.
function Resolve-ReloadUnrealRegistryDirectory {
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
        $item = Get-ItemProperty -LiteralPath $key
        $installedDirectory = $item.PSObject.Properties["InstalledDirectory"]
        if ($installedDirectory -and $installedDirectory.Value -and (Test-Path -LiteralPath ([string] $installedDirectory.Value))) {
            return (Resolve-Path -LiteralPath ([string] $installedDirectory.Value)).Path
        }
    }

    $programFiles = [Environment]::GetFolderPath("ProgramFiles")
    $defaultInstall = Join-Path $programFiles "Epic Games\UE_$EngineAssociation"
    if (Test-Path -LiteralPath $defaultInstall) {
        return (Resolve-Path -LiteralPath $defaultInstall).Path
    }

    return $null
}

# Resolves Unreal's Build.bat without project-local tool script dependencies.
function Resolve-ReloadUnrealBuildBatch {
    param([Parameter(Mandatory = $true)][string] $ProjectFile)

    $relativePaths = @(
        "Build\BatchFiles\Build.bat",
        "Engine\Build\BatchFiles\Build.bat"
    )

    if ($env:UE_BUILD_BAT -and (Test-Path -LiteralPath $env:UE_BUILD_BAT -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:UE_BUILD_BAT).Path
    }

    $fromEngineDir = Resolve-ReloadToolInDirectory -Directory $env:UE_ENGINE_DIR -RelativePaths $relativePaths
    if ($fromEngineDir) {
        return $fromEngineDir
    }

    $fromEditor = Resolve-ReloadToolNearPath -StartPath $env:UE_EDITOR_EXE -RelativePaths $relativePaths
    if ($fromEditor) {
        return $fromEditor
    }

    $pathEditor = Get-Command "UnrealEditor.exe" -ErrorAction SilentlyContinue
    if ($pathEditor) {
        $fromPathEditor = Resolve-ReloadToolNearPath -StartPath $pathEditor.Source -RelativePaths $relativePaths
        if ($fromPathEditor) {
            return $fromPathEditor
        }
    }

    $engineAssociation = Get-ReloadEngineAssociation -ProjectFile $ProjectFile
    $engineDir = Resolve-ReloadUnrealRegistryDirectory -EngineAssociation $engineAssociation
    $fromRegistry = Resolve-ReloadToolInDirectory -Directory $engineDir -RelativePaths $relativePaths
    if ($fromRegistry) {
        return $fromRegistry
    }

    throw "Unreal Build.bat was not found. Set UE_BUILD_BAT, UE_ENGINE_DIR, UE_EDITOR_EXE, or check EngineAssociation."
}

# Resolves UnrealEditor.exe without project-local tool script dependencies.
function Resolve-ReloadUnrealEditor {
    param([Parameter(Mandatory = $true)][string] $ProjectFile)

    $relativePaths = @(
        "Binaries\Win64\UnrealEditor.exe",
        "Engine\Binaries\Win64\UnrealEditor.exe"
    )

    if ($env:UE_EDITOR_EXE -and (Test-Path -LiteralPath $env:UE_EDITOR_EXE -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:UE_EDITOR_EXE).Path
    }

    $fromEngineDir = Resolve-ReloadToolInDirectory -Directory $env:UE_ENGINE_DIR -RelativePaths $relativePaths
    if ($fromEngineDir) {
        return $fromEngineDir
    }

    $pathEditor = Get-Command "UnrealEditor.exe" -ErrorAction SilentlyContinue
    if ($pathEditor) {
        return $pathEditor.Source
    }

    $engineAssociation = Get-ReloadEngineAssociation -ProjectFile $ProjectFile
    $engineDir = Resolve-ReloadUnrealRegistryDirectory -EngineAssociation $engineAssociation
    $fromRegistry = Resolve-ReloadToolInDirectory -Directory $engineDir -RelativePaths $relativePaths
    if ($fromRegistry) {
        return $fromRegistry
    }

    throw "UnrealEditor.exe was not found. Set UE_EDITOR_EXE, UE_ENGINE_DIR, add UnrealEditor.exe to PATH, or check EngineAssociation."
}
