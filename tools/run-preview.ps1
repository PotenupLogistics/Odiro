# Runs product-like local preview: Agents API, Bridge service, and Client preview mode.
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
        SkipBridge = $false
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
            "^-SkipBridge$" {
                $result.SkipBridge = $true
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
$bridgeRunScript = Join-Path $repoRoot "Bridge\tools\run.ps1"
$clientPreview = Join-Path $repoRoot "Client\Task-RunPreview.bat"
$analysisEndpointArg = "-ProjectRunAnalysisEndpointUrl=http://127.0.0.1:$($options.AgentsPort)/api/v2/analysis/run"
if (-not ($options.PreviewArgs | Where-Object { $_ -like "-ProjectRunAnalysisEndpointUrl=*" })) {
    $options.PreviewArgs += $analysisEndpointArg
}

# Waits until the Bridge host reports its listening endpoint.
function Wait-BridgeService {
    param(
        [System.Diagnostics.Process] $Process,
        [string] $StdoutLog,
        [string] $StderrLog,
        [int] $TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if ($Process.HasExited) {
            throw "Bridge service exited early with code $($Process.ExitCode). Check $StderrLog."
        }

        if (Test-Path -LiteralPath $StdoutLog -PathType Leaf) {
            $stdoutText = Get-Content -LiteralPath $StdoutLog -Raw -ErrorAction SilentlyContinue
            if ($stdoutText -match '(?m)^listening transport=') {
                return
            }
        }

        Start-Sleep -Milliseconds 500
    }

    throw "Timed out waiting for Bridge service. Check $StdoutLog and $StderrLog."
}

if ($options.SkipAgents -and $options.SkipBridge -and $options.SkipClient) {
    throw "Nothing to run. Remove -SkipAgents, -SkipBridge, or -SkipClient."
}

if ($options.SkipClient) {
    if (-not $options.SkipAgents -and -not $options.SkipBridge) {
        $agentsProcess = $null
        Register-CancelProcessCleanup
        try {
            $agentsProcess = & $agentsRunScript -Port $options.AgentsPort -Background -LogPrefix "run"
            Register-ManagedProcess -Process $agentsProcess -Label "Agents API"
            & $bridgeRunScript
        }
        finally {
            Stop-ManagedProcessTrees
            Unregister-CancelProcessCleanup
        }
    }
    elseif (-not $options.SkipAgents) {
        & $agentsRunScript -Port $options.AgentsPort
    }
    elseif (-not $options.SkipBridge) {
        & $bridgeRunScript
    }
    exit 0
}

$agentsProcess = $null
$bridgeProcess = $null
Register-CancelProcessCleanup
try {
    if (-not $options.SkipAgents) {
        $agentsProcess = & $agentsRunScript -Port $options.AgentsPort -Background -LogPrefix "run"
        Register-ManagedProcess -Process $agentsProcess -Label "Agents API"
    }

    if (-not $options.SkipBridge) {
        if (-not (Test-Path -LiteralPath $bridgeRunScript -PathType Leaf)) {
            throw "Bridge run script not found: $bridgeRunScript"
        }

        $logsDir = Join-Path $repoRoot "logs"
        New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
        $bridgeStdoutLog = Join-Path $logsDir "run-bridge.out.log"
        $bridgeStderrLog = Join-Path $logsDir "run-bridge.err.log"

        Write-Step "Start Bridge service"
        $bridgeProcess = Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $bridgeRunScript) `
            -WorkingDirectory $repoRoot `
            -WindowStyle Hidden `
            -RedirectStandardOutput $bridgeStdoutLog `
            -RedirectStandardError $bridgeStderrLog `
            -PassThru
        Register-ManagedProcess -Process $bridgeProcess -Label "Bridge service"

        Wait-BridgeService -Process $bridgeProcess -StdoutLog $bridgeStdoutLog -StderrLog $bridgeStderrLog
        Write-Step "Bridge logs: $bridgeStdoutLog, $bridgeStderrLog"
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
    Stop-ManagedProcessTrees
    Unregister-CancelProcessCleanup
}
