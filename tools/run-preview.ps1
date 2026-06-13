# Runs product-like local preview: Agents API plus Client preview, without packaging.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "run"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

function Parse-RunArguments {
    $result = [ordered]@{
        SkipAgents = $false
        SkipClient = $false
        AgentsPort = 8711
        PreviewArgs = @()
    }

    for ($index = 0; $index -lt $args.Count; $index++) {
        $argument = $args[$index]
        switch -Regex ($argument) {
            "^-SkipAgents$" {
                $result.SkipAgents = $true
                continue
            }
            "^-SkipClient$" {
                $result.SkipClient = $true
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
                    $result.PreviewArgs += $args[($index + 1)..($args.Count - 1)]
                }
                return $result
            }
            default {
                $result.PreviewArgs += $argument
            }
        }
    }

    return $result
}

$options = Parse-RunArguments @args
$repoRoot = Get-RepoRoot
$agentsRunScript = Join-Path $repoRoot "Agents\tools\run.ps1"
$clientPreview = Join-Path $repoRoot "Client\Task-RunPreview.bat"

if ($options.SkipAgents -and $options.SkipClient) {
    throw "Nothing to run. Remove -SkipAgents or -SkipClient."
}

if ($options.SkipClient) {
    & $agentsRunScript -Port $options.AgentsPort
    exit 0
}

$agentsProcess = $null
try {
    if (-not $options.SkipAgents) {
        $agentsProcess = & $agentsRunScript -Port $options.AgentsPort -Background -LogPrefix "run"
    }

    if (-not (Test-Path -LiteralPath $clientPreview -PathType Leaf)) {
        throw "Client preview script not found: $clientPreview"
    }

    Write-Step "Run Client preview"
    & $clientPreview @($options.PreviewArgs)
    if ($LASTEXITCODE -ne 0) {
        throw "Client preview failed with exit code $LASTEXITCODE."
    }
}
finally {
    if ($agentsProcess -and -not $agentsProcess.HasExited) {
        Write-Step "Stop Agents API pid $($agentsProcess.Id)"
        Stop-ProcessTree -ProcessId $agentsProcess.Id
    }
}
