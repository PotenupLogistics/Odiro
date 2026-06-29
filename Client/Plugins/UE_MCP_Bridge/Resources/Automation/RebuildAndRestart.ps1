# Detached helper that owns editor shutdown, rebuild, and relaunch.
param(
    [Parameter(Mandatory = $true)][string] $ProjectRoot,
    [Parameter(Mandatory = $true)][string] $JobId
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$projectRootPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
. (Join-Path $PSScriptRoot "UnrealProjectTools.ps1")

# Returns the shared runtime state directory.
function Get-ReloadStateDirectory {
    $stateDir = Join-Path $projectRootPath "Saved\UE_MCP_Bridge"
    if (-not (Test-Path -LiteralPath $stateDir -PathType Container)) {
        New-Item -ItemType Directory -Path $stateDir -Force | Out-Null
    }
    return $stateDir
}

# Reads a JSON file into a PowerShell object, returning null on missing files.
function Read-JsonFile {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

# Writes a JSON file with UTF-8 no BOM encoding.
function Write-JsonFile {
    param(
        [string] $Path,
        [object] $Value
    )
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 50), $script:Utf8NoBom)
}

# Returns a property value without tripping StrictMode on missing JSON fields.
function Get-ObjectProperty {
    param(
        [object] $Object,
        [string] $Name,
        [object] $DefaultValue = $null
    )
    if ($null -eq $Object) {
        return $DefaultValue
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($property -and $null -ne $property.Value) {
        return $property.Value
    }
    return $DefaultValue
}

# Updates the current job file with a new phase or terminal status.
function Update-ReloadJob {
    param(
        [string] $Status = "running",
        [string] $Phase,
        [string] $ErrorMessage = "",
        [int] $ExitCode = 0
    )
    $jobPath = Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$JobId.json"
    $job = Read-JsonFile -Path $jobPath
    if ($null -eq $job) {
        $job = [pscustomobject]@{ jobId = $JobId; requestedAt = [DateTime]::UtcNow.ToString("o") }
    }
    $job | Add-Member -NotePropertyName status -NotePropertyValue $Status -Force
    $job | Add-Member -NotePropertyName phase -NotePropertyValue $Phase -Force
    $job | Add-Member -NotePropertyName updatedAt -NotePropertyValue ([DateTime]::UtcNow.ToString("o")) -Force
    if ($ErrorMessage) {
        $job | Add-Member -NotePropertyName error -NotePropertyValue $ErrorMessage -Force
    }
    if ($ExitCode -ne 0) {
        $job | Add-Member -NotePropertyName exitCode -NotePropertyValue $ExitCode -Force
    }
    Write-JsonFile -Path $jobPath -Value $job
}

# Records the latest successful rebuild fingerprint for later idempotent MCP requests.
function Set-LastSuccessfulReload {
    param([object] $Job)
    $sourceFingerprint = Get-ObjectProperty -Object $Job -Name "sourceFingerprintAtStart"
    if ($null -eq $sourceFingerprint) {
        return
    }
    Write-JsonFile -Path (Join-Path (Get-ReloadStateDirectory) "last-success.json") -Value ([pscustomobject]@{
        jobId = $JobId
        operation = "rebuild_restart"
        sourceFingerprint = $sourceFingerprint
        completedAt = [DateTime]::UtcNow.ToString("o")
    })
}

# Updates the maintenance sentinel phase while the helper is active.
function Update-MaintenancePhase {
    param([string] $Phase)
    $maintenancePath = Join-Path (Get-ReloadStateDirectory) "maintenance.json"
    $maintenance = Read-JsonFile -Path $maintenancePath
    if ($null -eq $maintenance) {
        $maintenance = [pscustomobject]@{ jobId = $JobId }
    }
    $maintenance | Add-Member -NotePropertyName phase -NotePropertyValue $Phase -Force
    $maintenance | Add-Member -NotePropertyName updatedAt -NotePropertyValue ([DateTime]::UtcNow.ToString("o")) -Force
    Write-JsonFile -Path $maintenancePath -Value $maintenance
}

# Acquires the editor lifecycle lock for the whole rebuild.
function Enter-EditorLock {
    $path = Join-Path (Get-ReloadStateDirectory) "editor.lock"
    return [System.IO.File]::Open($path, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
}

# Reads the current bridge port state.
function Get-PortState {
    return Read-JsonFile -Path (Join-Path (Get-ReloadStateDirectory) "port.json")
}

# Sends one JSON-RPC request to the editor's WebSocket bridge.
function Invoke-BridgeMethod {
    param(
        [string] $Method,
        [object] $Params = @{},
        [int] $TimeoutMs = 10000
    )
    $portState = Get-PortState
    $port = Get-ObjectProperty -Object $portState -Name "port"
    if ($null -eq $portState -or -not $port) {
        throw "UE_MCP_Bridge port.json is missing."
    }

    $client = [System.Net.WebSockets.ClientWebSocket]::new()
    $cts = [System.Threading.CancellationTokenSource]::new()
    $cts.CancelAfter($TimeoutMs)
    try {
        $client.ConnectAsync([Uri]::new("ws://127.0.0.1:$([int] $port)"), $cts.Token).GetAwaiter().GetResult() | Out-Null
        $request = @{
            jsonrpc = "2.0"
            id = [guid]::NewGuid().ToString("N")
            method = $Method
            params = $Params
        }
        $bytes = $script:Utf8NoBom.GetBytes(($request | ConvertTo-Json -Depth 50 -Compress))
        $client.SendAsync([ArraySegment[byte]]::new($bytes), [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $cts.Token).GetAwaiter().GetResult() | Out-Null
        $buffer = [byte[]]::new(8192)
        $memory = [System.IO.MemoryStream]::new()
        do {
            $receive = $client.ReceiveAsync([ArraySegment[byte]]::new($buffer), $cts.Token).GetAwaiter().GetResult()
            if ($receive.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
                throw "Bridge WebSocket closed before response."
            }
            $memory.Write($buffer, 0, $receive.Count)
        } while (-not $receive.EndOfMessage)
        return ($script:Utf8NoBom.GetString($memory.ToArray()) | ConvertFrom-Json)
    }
    finally {
        $client.Dispose()
        $cts.Dispose()
    }
}

# Extracts the result from a bridge response and preserves JSON-RPC errors.
function Get-BridgeResult {
    param([object] $Response)
    if ($null -eq $Response) {
        throw "Bridge returned an empty response."
    }
    $errorObject = Get-ObjectProperty -Object $Response -Name "error"
    if ($errorObject) {
        $message = Get-ObjectProperty -Object $errorObject -Name "message" -DefaultValue "Bridge returned an error."
        throw $message
    }
    $result = Get-ObjectProperty -Object $Response -Name "result"
    if ($null -eq $result) {
        throw "Bridge response did not include a result."
    }
    return $result
}

# Waits until the bridge reports no active non-coordination requests.
function Wait-BridgeDrain {
    param([int] $TimeoutSeconds = 300)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ($true) {
        $status = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 10000)
        if ([int] (Get-ObjectProperty -Object $status -Name "activeRequests" -DefaultValue 0) -le 0) {
            return $status
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Timed out waiting for active MCP requests to drain."
        }
        Start-Sleep -Seconds 1
    }
}

# Waits for an editor process id to exit.
function Wait-EditorExit {
    param(
        [int] $EditorPid,
        [int] $TimeoutSeconds = 90
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ($EditorPid -gt 0 -and (Get-Process -Id $EditorPid -ErrorAction SilentlyContinue)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Timed out waiting for UnrealEditor pid $EditorPid to exit."
        }
        Start-Sleep -Seconds 1
    }
}

# Runs Build.bat and writes stdout/stderr to job log files.
function Invoke-ClientEditorBuild {
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRootPath
    $buildBat = Resolve-ReloadUnrealBuildBatch -ProjectFile $projectFile
    $logDir = Join-Path (Get-ReloadStateDirectory) "logs"
    if (-not (Test-Path -LiteralPath $logDir -PathType Container)) {
        New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    }
    $stdoutPath = Join-Path $logDir "$JobId-build.out.log"
    $stderrPath = Join-Path $logDir "$JobId-build.err.log"
    $arguments = @(
        (Get-EditorTargetName),
        "Win64",
        "Development",
        "-Project=$projectFile",
        "-WaitMutex",
        "-FromMsBuild"
    )

    $process = Start-Process `
        -FilePath $buildBat `
        -ArgumentList (Join-ReloadProcessArguments -Arguments $arguments) `
        -WorkingDirectory $projectRootPath `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -Wait `
        -PassThru

    $exitCode = [int] (Get-ObjectProperty -Object $process -Name "ExitCode" -DefaultValue 0)
    if ($exitCode -ne 0) {
        throw "Build failed with exit code $exitCode. Logs: $stdoutPath, $stderrPath"
    }
    return [pscustomobject]@{ stdout = $stdoutPath; stderr = $stderrPath }
}

# Returns the editor target name from the project file name.
function Get-EditorTargetName {
    return Get-ReloadEditorTargetName -ProjectFile (Get-ReloadProjectFile -ProjectRoot $projectRootPath)
}

# Starts the editor and waits for the bridge port lockfile to advertise it.
function Start-EditorAndWaitForBridge {
    param([int] $TimeoutSeconds = 180)
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRootPath
    $editor = Resolve-ReloadUnrealEditor -ProjectFile $projectFile
    $portPath = Join-Path (Get-ReloadStateDirectory) "port.json"
    if (Test-Path -LiteralPath $portPath -PathType Leaf) {
        Remove-Item -LiteralPath $portPath -Force
    }

    $process = Start-Process `
        -FilePath $editor `
        -ArgumentList (Join-ReloadProcessArguments -Arguments @($projectFile)) `
        -WorkingDirectory $projectRootPath `
        -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ($true) {
        $portState = Get-PortState
        $pidValue = Get-ObjectProperty -Object $portState -Name "pid"
        if ($null -ne $portState -and $pidValue -and (Get-Process -Id ([int] $pidValue) -ErrorAction SilentlyContinue)) {
            return [pscustomobject]@{ editorPid = $process.Id; portState = $portState }
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Timed out waiting for UE_MCP_Bridge port.json after editor restart."
        }
        Start-Sleep -Seconds 2
    }
}

$editorLock = $null
try {
    $editorLock = Enter-EditorLock

    Update-ReloadJob -Phase "draining"
    Update-MaintenancePhase -Phase "draining"
    $status = Wait-BridgeDrain

    Update-ReloadJob -Phase "saving"
    Update-MaintenancePhase -Phase "saving"
    $null = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_save_dirty" -Params @{ includeMaps = $true; includeContent = $true } -TimeoutMs 120000)

    Update-ReloadJob -Phase "closing"
    Update-MaintenancePhase -Phase "closing"
    $editorPid = [int] (Get-ObjectProperty -Object $status -Name "editorPid" -DefaultValue 0)
    $null = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_request_exit" -Params @{ delaySeconds = 0.5; force = $false } -TimeoutMs 10000)
    Wait-EditorExit -EditorPid $editorPid

    Update-ReloadJob -Phase "building"
    Update-MaintenancePhase -Phase "building"
    $logs = Invoke-ClientEditorBuild

    Update-ReloadJob -Phase "restarting"
    Update-MaintenancePhase -Phase "restarting"
    $restart = Start-EditorAndWaitForBridge

    Update-ReloadJob -Status "completed" -Phase "completed"
    $jobPath = Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$JobId.json"
    $job = Read-JsonFile -Path $jobPath
    $job | Add-Member -NotePropertyName logs -NotePropertyValue $logs -Force
    $job | Add-Member -NotePropertyName restart -NotePropertyValue $restart -Force
    $job | Add-Member -NotePropertyName sourceFingerprintAtCompletion -NotePropertyValue (Get-ObjectProperty -Object $job -Name "sourceFingerprintAtStart") -Force
    Write-JsonFile -Path $jobPath -Value $job
    Set-LastSuccessfulReload -Job $job

    $maintenancePath = Join-Path (Get-ReloadStateDirectory) "maintenance.json"
    if (Test-Path -LiteralPath $maintenancePath -PathType Leaf) {
        Remove-Item -LiteralPath $maintenancePath -Force
    }
}
catch {
    Update-MaintenancePhase -Phase "failed"
    Update-ReloadJob -Status "failed" -Phase "failed" -ErrorMessage $_.Exception.Message
}
finally {
    if ($null -ne $editorLock) {
        $editorLock.Dispose()
    }
}
