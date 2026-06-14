$ErrorActionPreference = 'Stop'

$runDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $runDir '..')).Path
$projectFile = Join-Path $projectRoot 'OdiroSim.uproject'

if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file not found: $projectFile"
}

$uproject = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$association = [string]$uproject.EngineAssociation

function Add-Candidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$Path
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        [void]$Candidates.Add($Path)
    }
}

function Add-EditorCandidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$EditorPath
    )

    if ([string]::IsNullOrWhiteSpace($EditorPath) -or -not (Test-Path -LiteralPath $EditorPath)) {
        return
    }

    $editorDir = Split-Path (Resolve-Path -LiteralPath $EditorPath).Path -Parent
    Add-Candidate $Candidates (Join-Path $editorDir '..\..\..')
}

function Test-EngineRoot {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    return Test-Path -LiteralPath (Join-Path $Path 'Engine\Binaries\Win64\UnrealEditor.exe')
}

function Find-EngineRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    Add-Candidate $candidates $env:UE_ENGINE_DIR
    Add-EditorCandidate $candidates $env:UE_EDITOR_EXE

    $pathEditor = Get-Command UnrealEditor.exe -ErrorAction SilentlyContinue | Select-Object -First 1
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
            if (-not (Test-Path -LiteralPath $registryPath)) {
                continue
            }

            $props = Get-ItemProperty -LiteralPath $registryPath
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
        if (Test-Path -LiteralPath $candidate) {
            $resolved = (Resolve-Path -LiteralPath $candidate).Path
        }

        if (Test-EngineRoot $resolved) {
            return $resolved
        }
    }

    throw "Unreal Engine root not found. EngineAssociation=$association"
}

function Add-Option {
    param(
        [xml]$Document,
        [System.Xml.XmlElement]$Parent,
        [string]$Name,
        [string]$Value
    )

    $option = $Document.CreateElement('option')
    $option.SetAttribute('name', $Name)
    $option.SetAttribute('value', $Value)
    [void]$Parent.AppendChild($option)
}

function Add-LaunchProfile {
    param(
        [xml]$Document,
        [System.Xml.XmlElement]$Parent,
        [int]$Index,
        [string]$Configuration,
        [string]$ExecutablePath,
        [string]$MandatoryProgramParameters,
        [string]$CustomProgramParameters,
        [string]$WorkingDirectory
    )

    $profile = $Document.CreateElement("configuration_$Index")
    $profile.SetAttribute('setup', '1')
    [void]$Parent.AppendChild($profile)

    Add-Option $Document $profile 'CONFIGURATION' $Configuration
    Add-Option $Document $profile 'PLATFORM' 'Win64'
    Add-Option $Document $profile 'CURRENT_LAUNCH_PROFILE' 'Local'
    Add-Option $Document $profile 'EXECUTABLE_PATH' $ExecutablePath
    Add-Option $Document $profile 'MANDATORY_PROGRAM_PARAMETERS' $MandatoryProgramParameters
    Add-Option $Document $profile 'CUSTOM_PROGRAM_PARAMETERS' $CustomProgramParameters
    Add-Option $Document $profile 'WORKING_DIRECTORY' $WorkingDirectory
    Add-Option $Document $profile 'PASS_PARENT_ENVS' '1'
    Add-Option $Document $profile 'USE_EXTERNAL_CONSOLE' '1'
    Add-Option $Document $profile 'TERMINAL_INTERACTION_BEHAVIOR' 'FORCE_CONSOLE'
    Add-Option $Document $profile 'PROJECT_FILE_PATH' '$PROJECT_DIR$/OdiroSim.uproject'
}

function New-PreviewConfigurationDocument {
    param(
        [string]$Name,
        [string]$CustomProgramParameters,
        [string]$EngineRoot
    )

    $engineRootPath = $EngineRoot -replace '\\', '/'

    [xml]$document = '<component name="ProjectRunConfigurationManager" />'
    $configuration = $document.CreateElement('configuration')
    $configuration.SetAttribute('default', 'false')
    $configuration.SetAttribute('name', $Name)
    $configuration.SetAttribute('type', 'Uproject')
    $configuration.SetAttribute('factoryName', 'rider.uproject')
    [void]$document.component.AppendChild($configuration)

    Add-LaunchProfile $document $configuration 1 'DebugGame Editor' "$engineRootPath/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" ' "$PROJECT_DIR$/OdiroSim.uproject"' $CustomProgramParameters $engineRootPath
    Add-LaunchProfile $document $configuration 2 'DebugGame' '$PROJECT_DIR$/Binaries/Win64/OdiroSim-Win64-DebugGame.exe' '' '' $engineRootPath
    Add-LaunchProfile $document $configuration 3 'Development Editor' "$engineRootPath/Engine/Binaries/Win64/UnrealEditor.exe" ' "$PROJECT_DIR$/OdiroSim.uproject"' $CustomProgramParameters $engineRootPath
    Add-LaunchProfile $document $configuration 4 'Development' '$PROJECT_DIR$/Binaries/Win64/OdiroSim.exe' '' '' $engineRootPath
    Add-LaunchProfile $document $configuration 5 'Shipping' '$PROJECT_DIR$/Binaries/Win64/OdiroSim-Win64-Shipping.exe' '' '' $engineRootPath

    Add-Option $document $configuration 'DEFAULT_PROJECT_PATH' '$PROJECT_DIR$/OdiroSim.uproject'
    Add-Option $document $configuration 'PROJECT_FILE_PATH' '$PROJECT_DIR$/OdiroSim.uproject'
    Add-Option $document $configuration 'AUTO_SELECT_PRIORITY' '10010'

    $method = $document.CreateElement('method')
    $method.SetAttribute('v', '2')
    [void]$configuration.AppendChild($method)

    $buildOption = $document.CreateElement('option')
    $buildOption.SetAttribute('name', 'Build')
    [void]$method.AppendChild($buildOption)

    return $document
}

function Save-Xml {
    param(
        [xml]$Document,
        [string]$Path
    )

    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Indent = $true
    $settings.OmitXmlDeclaration = $true

    $writer = [System.Xml.XmlWriter]::Create($Path, $settings)
    try {
        $Document.Save($writer)
    } finally {
        $writer.Close()
    }
}

$engineRoot = Find-EngineRoot

$previewPath = Join-Path $runDir 'Preview.local.run.xml'
$simulatorPath = Join-Path $runDir 'PreviewSimulator.local.run.xml'

$previewDocument = New-PreviewConfigurationDocument `
    -Name 'Preview' `
    -CustomProgramParameters '-game -NoSplash' `
    -EngineRoot $engineRoot

$simulatorDocument = New-PreviewConfigurationDocument `
    -Name 'Preview - Simulator' `
    -CustomProgramParameters '-game -NoSplash -Simulate=$Prompt:SimulationSetup JSON path:Json/Input/SimulationSetupPlayable.json$' `
    -EngineRoot $engineRoot

Save-Xml $previewDocument $previewPath
Save-Xml $simulatorDocument $simulatorPath

Write-Host "[OdiroSim] Unreal Engine root: $engineRoot"
Write-Host "[OdiroSim] Generated Rider Preview configs:"
Write-Host "  $previewPath"
Write-Host "  $simulatorPath"
