$ErrorActionPreference = 'Stop'

$runDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $runDir '..')).Path
$projectFile = Join-Path $projectRoot 'OdiroSim.uproject'

if (-not (Test-Path $projectFile)) {
    throw "Project file not found: $projectFile"
}

$uproject = Get-Content $projectFile -Raw | ConvertFrom-Json
$association = [string]$uproject.EngineAssociation

function Add-Candidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$Path
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        $Candidates.Add($Path)
    }
}

function Add-EditorCandidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$EditorPath
    )

    if ([string]::IsNullOrWhiteSpace($EditorPath) -or -not (Test-Path $EditorPath)) {
        return
    }

    $editorDir = Split-Path (Resolve-Path $EditorPath).Path -Parent
    Add-Candidate $Candidates (Join-Path $editorDir '..\..\..')
}

function Test-EngineRoot {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    return Test-Path (Join-Path $Path 'Engine\Binaries\Win64\UnrealEditor.exe')
}

function Find-EngineRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    Add-Candidate $candidates $env:UE_ENGINE_DIR
    Add-EditorCandidate $candidates $env:UE_EDITOR_EXE

    $pathEditor = Get-Command UnrealEditor.exe -ErrorAction SilentlyContinue
    if ($pathEditor) {
        Add-EditorCandidate $candidates $pathEditor.Source
    }

    if (-not [string]::IsNullOrWhiteSpace($association)) {
        $registryPaths = @(
            'HKCU:\Software\Epic Games\Unreal Engine\Builds',
            "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association",
            "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association"
        )

        foreach ($registryPath in $registryPaths) {
            if (-not (Test-Path $registryPath)) {
                continue
            }

            $props = Get-ItemProperty $registryPath
            if ($registryPath -like '*Builds') {
                if ($props.PSObject.Properties.Name -contains $association) {
                    Add-Candidate $candidates ([string]$props.$association)
                }
            } elseif ($props.InstalledDirectory) {
                Add-Candidate $candidates ([string]$props.InstalledDirectory)
            }
        }

        Add-Candidate $candidates (Join-Path $env:ProgramFiles "Epic Games\UE_$association")
    }

    foreach ($candidate in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        $resolved = $candidate
        if (Test-Path $candidate) {
            $resolved = (Resolve-Path $candidate).Path
        }

        if (Test-EngineRoot $resolved) {
            return $resolved
        }
    }

    throw "Unreal Engine root not found. EngineAssociation=$association"
}

function Set-RiderPathVariable {
    param(
        [string]$Name,
        [string]$Value
    )

    $jetBrainsRoot = Join-Path $env:APPDATA 'JetBrains'
    if (-not (Test-Path $jetBrainsRoot)) {
        throw "JetBrains config directory not found: $jetBrainsRoot"
    }

    $riderConfigDirs = Get-ChildItem $jetBrainsRoot -Directory -Filter 'Rider*' |
        Where-Object { $_.Name -match '^Rider\d{4}\.\d+$' } |
        Sort-Object LastWriteTime -Descending

    if (-not $riderConfigDirs) {
        throw "Rider config directory not found under: $jetBrainsRoot"
    }

    $updatedFiles = @()
    foreach ($riderConfigDir in $riderConfigDirs) {
        $optionsDir = Join-Path $riderConfigDir.FullName 'options'
        $pathMacrosFile = Join-Path $optionsDir 'path.macros.xml'
        New-Item -ItemType Directory -Path $optionsDir -Force | Out-Null

        if (Test-Path $pathMacrosFile) {
            [xml]$xml = Get-Content $pathMacrosFile -Raw
        } else {
            [xml]$xml = '<application><component name="PathMacrosImpl" /></application>'
        }

        $component = $xml.application.component |
            Where-Object { $_.name -eq 'PathMacrosImpl' } |
            Select-Object -First 1

        if (-not $component) {
            $component = $xml.CreateElement('component')
            $component.SetAttribute('name', 'PathMacrosImpl')
            [void]$xml.application.AppendChild($component)
        }

        $macro = @($component.macro | Where-Object { $_.name -eq $Name }) | Select-Object -First 1
        if (-not $macro) {
            $macro = $xml.CreateElement('macro')
            $macro.SetAttribute('name', $Name)
            [void]$component.AppendChild($macro)
        }

        $macro.SetAttribute('value', $Value)

        $settings = [System.Xml.XmlWriterSettings]::new()
        $settings.Indent = $true
        $settings.OmitXmlDeclaration = $true
        $writer = [System.Xml.XmlWriter]::Create($pathMacrosFile, $settings)
        try {
            $xml.Save($writer)
        } finally {
            $writer.Close()
        }

        $updatedFiles += $pathMacrosFile
    }

    return $updatedFiles
}

$engineRoot = Find-EngineRoot
$updatedFiles = Set-RiderPathVariable 'UE_INSTALL_DIR' $engineRoot

Write-Host "[OdiroSim] UE_INSTALL_DIR = $engineRoot"
Write-Host "[OdiroSim] Updated Rider Path Variables:"
foreach ($file in $updatedFiles) {
    Write-Host "  $file"
}
Write-Host "[OdiroSim] Restart Rider if it is already open."
