# Runs the development session: Agents auto-reload server plus Unreal Editor.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "dev"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

function Parse-DevArguments {
    $result = [ordered]@{
        SkipAgents = $false
        SkipEditor = $false
        NoAgentsReload = $false
        AgentsPort = 8711
        EditorArgs = @()
    }

    for ($index = 0; $index -lt $args.Count; $index++) {
        $argument = $args[$index]
        switch -Regex ($argument) {
            "^-SkipAgents$" {
                $result.SkipAgents = $true
                continue
            }
            "^-SkipEditor$|^-SkipClient$" {
                $result.SkipEditor = $true
                continue
            }
            "^-NoAgentsReload$|^-NoReload$" {
                $result.NoAgentsReload = $true
                continue
            }
            "^-AgentsPort$" {
                $index++
                if ($index -ge $args.Count) {
                    throw "-AgentsPort requires a numeric value."
                }
                $result.AgentsPort = [int] $args[$index]
                continue
            }
            "^--$" {
                if ($index + 1 -lt $args.Count) {
                    $result.EditorArgs += $args[($index + 1)..($args.Count - 1)]
                }
                return $result
            }
            default {
                $result.EditorArgs += $argument
            }
        }
    }

    return $result
}

$options = Parse-DevArguments @args
$repoRoot = Get-RepoRoot
$agentsDevScript = Join-Path $repoRoot "Agents\tools\dev.ps1"
$clientDev = Join-Path $repoRoot "Client\Task-Dev.bat"

if ($options.SkipAgents -and $options.SkipEditor) {
    throw "Nothing to run for the development session. Remove -SkipAgents or -SkipEditor."
}

if ($options.SkipEditor) {
    & $agentsDevScript -Port $options.AgentsPort -NoReload:$options.NoAgentsReload
    exit 0
}

$agentsProcess = $null
try {
    if (-not $options.SkipAgents) {
        $agentsProcess = & $agentsDevScript -Port $options.AgentsPort -Background -LogPrefix "dev" -NoReload:$options.NoAgentsReload
    }

    if (-not (Test-Path -LiteralPath $clientDev -PathType Leaf)) {
        throw "Client development script not found: $clientDev"
    }

    Write-Step "Open Client development session"
    & $clientDev @($options.EditorArgs)
    if ($LASTEXITCODE -ne 0) {
        throw "Client development session failed with exit code $LASTEXITCODE."
    }
}
finally {
    if ($agentsProcess -and -not $agentsProcess.HasExited) {
        Write-Step "Stop Agents API pid $($agentsProcess.Id)"
        Stop-ProcessTree -ProcessId $agentsProcess.Id
    }
}
