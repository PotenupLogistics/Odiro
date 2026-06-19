# Synchronizes shared IDE run configurations with the repository task workflow.
param(
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "ide-config"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot
$clientDir = Join-Path $repoRoot "Client"
$riderRunDir = Join-Path $clientDir ".run"
$riderIdeaDir = Join-Path $clientDir ".idea"

# Writes deterministic UTF-8 text files so setup does not churn IDE config diffs.
function Set-TextFile {
    param(
        [string] $Path,
        [string[]] $Lines
    )

    $content = (($Lines -join "`r`n") + "`r`n")
    if ($DryRun) {
        Write-Step "Would write: $Path"
        return
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $content, $encoding)
}

# Adds a JetBrains XML option element.
function Add-Option {
    param(
        [xml] $Document,
        [System.Xml.XmlElement] $Parent,
        [string] $Name,
        [string] $Value = ""
    )

    $option = $Document.CreateElement("option")
    $option.SetAttribute("name", $Name)
    if (-not [string]::IsNullOrEmpty($Value)) {
        $option.SetAttribute("value", $Value)
    }
    [void] $Parent.AppendChild($option)
}

# Saves XML without a declaration to match JetBrains shared config style.
function Save-Xml {
    param(
        [xml] $Document,
        [string] $Path
    )

    if ($DryRun) {
        Write-Step "Would write: $Path"
        return
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Indent = $true
    $settings.OmitXmlDeclaration = $true
    $settings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $settings.NewLineChars = "`n"

    $writer = [System.Xml.XmlWriter]::Create($Path, $settings)
    try {
        $Document.Save($writer)
    }
    finally {
        $writer.Close()
    }
}

# Creates a Rider Shell Script run configuration that invokes a generated .cmd helper.
function New-RiderShellRunConfig {
    param(
        [string] $Name,
        [string] $ScriptName
    )

    [xml] $document = '<component name="ProjectRunConfigurationManager" />'
    $configuration = $document.CreateElement("configuration")
    $configuration.SetAttribute("default", "false")
    $configuration.SetAttribute("name", $Name)
    $configuration.SetAttribute("type", "ShConfigurationType")
    [void] $document.component.AppendChild($configuration)

    Add-Option $document $configuration "SCRIPT_TEXT"
    Add-Option $document $configuration "INDEPENDENT_SCRIPT_PATH" "true"
    Add-Option $document $configuration "SCRIPT_PATH" "`$PROJECT_DIR`$/.run/$ScriptName"
    Add-Option $document $configuration "SCRIPT_OPTIONS"
    Add-Option $document $configuration "INDEPENDENT_SCRIPT_WORKING_DIRECTORY" "true"
    Add-Option $document $configuration "SCRIPT_WORKING_DIRECTORY" "`$PROJECT_DIR`$/.."
    Add-Option $document $configuration "INDEPENDENT_INTERPRETER_PATH" "true"
    Add-Option $document $configuration "INTERPRETER_PATH" "C:\Windows\System32\cmd.exe"
    Add-Option $document $configuration "INTERPRETER_OPTIONS" "/C"
    Add-Option $document $configuration "EXECUTE_IN_TERMINAL" "true"
    Add-Option $document $configuration "EXECUTE_SCRIPT_FILE" "true"

    $envs = $document.CreateElement("envs")
    [void] $configuration.AppendChild($envs)

    $method = $document.CreateElement("method")
    $method.SetAttribute("v", "2")
    [void] $configuration.AppendChild($method)

    return $document
}

# Adds one Unreal launch profile to a Rider Uproject run configuration.
function Add-UnrealLaunchProfile {
    param(
        [xml] $Document,
        [System.Xml.XmlElement] $Parent,
        [int] $Index,
        [string] $Configuration,
        [string] $ExecutablePath,
        [string] $MandatoryProgramParameters,
        [string] $CustomProgramParameters,
        [string] $WorkingDirectory
    )

    $profile = $Document.CreateElement("configuration_$Index")
    $profile.SetAttribute("setup", "1")
    [void] $Parent.AppendChild($profile)

    Add-Option $Document $profile "CONFIGURATION" $Configuration
    Add-Option $Document $profile "PLATFORM" "Win64"
    Add-Option $Document $profile "CURRENT_LAUNCH_PROFILE" "Local"
    Add-Option $Document $profile "EXECUTABLE_PATH" $ExecutablePath
    Add-Option $Document $profile "MANDATORY_PROGRAM_PARAMETERS" $MandatoryProgramParameters
    Add-Option $Document $profile "CUSTOM_PROGRAM_PARAMETERS" $CustomProgramParameters
    Add-Option $Document $profile "WORKING_DIRECTORY" $WorkingDirectory
    Add-Option $Document $profile "PASS_PARENT_ENVS" "1"
    Add-Option $Document $profile "USE_EXTERNAL_CONSOLE" "1"
    Add-Option $Document $profile "TERMINAL_INTERACTION_BEHAVIOR" "FORCE_CONSOLE"
    Add-Option $Document $profile "PROJECT_FILE_PATH" "`$PROJECT_DIR`$/OdiroSim.uproject"
}

# Creates a Rider Unreal/Uproject run configuration so C++ debugging attaches to UnrealEditor.
function New-RiderPreviewModeRunConfig {
    param(
        [string] $Name,
        [string] $CustomProgramParameters
    )

    [xml] $document = '<component name="ProjectRunConfigurationManager" />'
    $configuration = $document.CreateElement("configuration")
    $configuration.SetAttribute("default", "false")
    $configuration.SetAttribute("name", $Name)
    $configuration.SetAttribute("type", "Uproject")
    $configuration.SetAttribute("factoryName", "rider.uproject")
    [void] $document.component.AppendChild($configuration)

    Add-UnrealLaunchProfile $document $configuration 1 "DebugGame Editor" "`$UE_INSTALL_DIR`$/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" ' "$PROJECT_DIR$/OdiroSim.uproject"' $CustomProgramParameters "`$UE_INSTALL_DIR`$"
    Add-UnrealLaunchProfile $document $configuration 2 "DebugGame" "`$PROJECT_DIR`$/Binaries/Win64/OdiroSim-Win64-DebugGame.exe" "" "" "`$UE_INSTALL_DIR`$"
    Add-UnrealLaunchProfile $document $configuration 3 "Development Editor" "`$UE_INSTALL_DIR`$/Engine/Binaries/Win64/UnrealEditor.exe" ' "$PROJECT_DIR$/OdiroSim.uproject"' $CustomProgramParameters "`$UE_INSTALL_DIR`$"
    Add-UnrealLaunchProfile $document $configuration 4 "Development" "`$PROJECT_DIR`$/Binaries/Win64/OdiroSim.exe" "" "" "`$UE_INSTALL_DIR`$"
    Add-UnrealLaunchProfile $document $configuration 5 "Shipping" "`$PROJECT_DIR`$/Binaries/Win64/OdiroSim-Win64-Shipping.exe" "" "" "`$UE_INSTALL_DIR`$"

    Add-Option $document $configuration "DEFAULT_PROJECT_PATH" "`$PROJECT_DIR`$/OdiroSim.uproject"
    Add-Option $document $configuration "PROJECT_FILE_PATH" "`$PROJECT_DIR`$/OdiroSim.uproject"
    Add-Option $document $configuration "AUTO_SELECT_PRIORITY" "10010"

    $method = $document.CreateElement("method")
    $method.SetAttribute("v", "2")
    [void] $configuration.AppendChild($method)

    $buildOption = $document.CreateElement("option")
    $buildOption.SetAttribute("name", "Build")
    [void] $method.AppendChild($buildOption)

    return $document
}

# Creates a Rider startup file without legacy auto-run entries.
function New-RiderStartupConfig {
    [xml] $document = '<project version="4" />'
    $component = $document.CreateElement("component")
    $component.SetAttribute("name", "ProjectStartupSharedConfiguration")
    [void] $document.project.AppendChild($component)

    $configurations = $document.CreateElement("configurations")
    [void] $component.AppendChild($configurations)

    return $document
}

# Removes exact legacy Rider configs generated by the old Unreal preview workflow.
function Remove-LegacyRiderConfig {
    param([string] $Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    if ($DryRun) {
        Write-Step "Would remove: $Path"
        return
    }

    Remove-Item -LiteralPath $Path -Force
}

$helperScripts = @{
    "OdiroPreviewServices.cmd" = @(
        "@echo off",
        "setlocal EnableExtensions",
        "set ""REPO_ROOT=%~dp0..\..""",
        "call ""%REPO_ROOT%\task-run.bat"" -SkipClient",
        "exit /b %ERRORLEVEL%"
    )
}

$riderRunConfigs = @(
    @{ Name = "Preview Services"; ScriptName = "OdiroPreviewServices.cmd"; FileName = "PreviewServices.run.xml" }
)

foreach ($script in $helperScripts.GetEnumerator()) {
    Set-TextFile -Path (Join-Path $riderRunDir $script.Key) -Lines $script.Value
}

foreach ($config in $riderRunConfigs) {
    $document = New-RiderShellRunConfig -Name $config.Name -ScriptName $config.ScriptName
    Save-Xml -Document $document -Path (Join-Path $riderRunDir $config.FileName)
}

$previewArgs = "-game -NoSplash -windowed -ResX=1920 -ResY=1080"
Save-Xml `
    -Document (New-RiderPreviewModeRunConfig -Name "Preview Mode" -CustomProgramParameters $previewArgs) `
    -Path (Join-Path $riderRunDir "PreviewMode.run.xml")
Save-Xml `
    -Document (New-RiderPreviewModeRunConfig -Name "Preview Mode With Flags" -CustomProgramParameters "$previewArgs `$Prompt:Preview flags:$") `
    -Path (Join-Path $riderRunDir "PreviewModeWithFlags.run.xml")

Save-Xml -Document (New-RiderStartupConfig) -Path (Join-Path $riderIdeaDir "startup.xml")

$setEnginePathScript = Join-Path $riderRunDir "SetEnginePath.ps1"
if (Test-Path -LiteralPath $setEnginePathScript -PathType Leaf) {
    if ($DryRun) {
        Write-Step "Would run: $setEnginePathScript"
    }
    else {
        try {
            & $setEnginePathScript
        }
        catch {
            Write-WarningMessage "Failed to update Rider UE_INSTALL_DIR path macro: $($_.Exception.Message)"
        }
    }
}

foreach ($legacyFileName in @(
        "OdiroBuild.cmd",
        "OdiroDevelopment.cmd",
        "OdiroLfsLocksShowStatus.cmd",
        "OdiroManualUnlockForce.cmd",
        "OdiroPreviewMode.cmd",
        "OdiroPreviewModeWithFlags.cmd",
        "OdiroPushOpenPullRequest.cmd",
        "OdiroSetup.cmd",
        "Build.run.xml",
        "Development.run.xml",
        "ForceUnlock.run.xml",
        "GitLfsLocksShowStatus.run.xml",
        "OdiroRunPreview.cmd",
        "GeneratePreviewConfigs.run.xml",
        "Preview.local.run.xml",
        "PreviewSimulator.local.run.xml",
        "Preview.run.xml",
        "Preview - Simulator.run.xml",
        "RunPreview.run.xml",
        "PushOpenPullRequest.run.xml",
        "Setup.run.xml"
    )) {
    Remove-LegacyRiderConfig -Path (Join-Path $riderRunDir $legacyFileName)
}

Write-Success "IDE run configurations synchronized."
