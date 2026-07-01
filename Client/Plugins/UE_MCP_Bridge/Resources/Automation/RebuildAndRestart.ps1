# Detached helper that owns editor shutdown, rebuild, and relaunch.
param(
    [Parameter(Mandatory = $true)][string] $ProjectRoot,
    [Parameter(Mandatory = $true)][string] $JobId,
    [string] $EditorArgsJson = "[]",
    [int] $MaxParallelActions = 0,
    [switch] $McpSafeLaunch,
    [switch] $ColdStart
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$projectRootPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
. (Join-Path $PSScriptRoot "UnrealProjectTools.ps1")
. (Join-Path $PSScriptRoot "CrashReportTools.ps1")
$script:EditorLaunchArgs = @()
try {
    $parsedEditorArgs = $EditorArgsJson | ConvertFrom-Json
    if ($null -ne $parsedEditorArgs) {
        $script:EditorLaunchArgs = @($parsedEditorArgs)
    }
}
catch {
    throw "Invalid EditorArgsJson: $($_.Exception.Message)"
}

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

# Adds an editor launch argument once, preserving caller-provided ordering.
function Add-EditorLaunchArg {
    param([string] $Argument)
    if (-not $Argument) {
        return
    }
    if ($script:EditorLaunchArgs -notcontains $Argument) {
        $script:EditorLaunchArgs += $Argument
    }
}

# Returns the editor arguments used by reload-started editor processes.
function Get-EditorLaunchArgs {
    Add-EditorLaunchArg -Argument "-NoSplash"
    if ($McpSafeLaunch) {
        Add-EditorLaunchArg -Argument "-DDC=InstalledNoZenLocalFallback"
        Add-EditorLaunchArg -Argument "-d3d11"
        Add-EditorLaunchArg -Argument "-noraytracing"
    }
    return @($script:EditorLaunchArgs)
}

# Returns the newest editor log path available for restart diagnostics.
function Get-LatestEditorLogPath {
    $logsDir = Join-Path $projectRootPath "Saved\Logs"
    if (-not (Test-Path -LiteralPath $logsDir -PathType Container)) {
        return ""
    }
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRootPath
    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($projectFile)
    $logs = @(Get-ChildItem -LiteralPath $logsDir -File -Filter "$projectName*.log" | Sort-Object LastWriteTimeUtc -Descending)
    if ($logs.Count -eq 0) {
        return ""
    }
    return $logs[0].FullName
}

# Returns the tail of a log file without requiring exclusive access.
function Get-LogExcerpt {
    param(
        [string] $Path,
        [int] $TailLines = 80
    )
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    return ((Get-Content -LiteralPath $Path -Tail $TailLines -ErrorAction SilentlyContinue) -join "`n")
}

# Classifies editor startup failures during rebuild/restart.
function Get-EditorStartupDiagnosis {
    param(
        [int] $EditorPid,
        [string] $LatestLog = ""
    )
    if (-not $LatestLog) {
        $LatestLog = Get-LatestEditorLogPath
    }
    $processAlive = $false
    if ($EditorPid -gt 0) {
        $processAlive = $null -ne (Get-Process -Id $EditorPid -ErrorAction SilentlyContinue)
    }
    $excerpt = Get-LogExcerpt -Path $LatestLog
    $crashReportClients = @(Get-UnrealCrashReportClientStates)
    $pendingCrashReports = @($crashReportClients | Where-Object { $_.blocksEditorLaunch })
    $code = "port_timeout"
    if ($pendingCrashReports.Count -gt 0 -and ($EditorPid -le 0 -or -not $processAlive)) {
        $code = "crash_report_pending"
    }
    elseif ($EditorPid -le 0 -and -not $excerpt) {
        $code = "no_editor_process"
    }
    elseif ($excerpt -match "Code not found for generated code \(package /Script/ChaosSolverEngine\)" -or
        $excerpt -match "Shader compiler returned a non-zero error code" -or
        $excerpt -match "AllowShaderCompiling") {
        $code = "editor_crashed_before_bridge"
    }
    elseif ($excerpt -match "Waiting for ZenServer to be ready" -and
        $excerpt -notmatch "ZenLocal: Using ZenServer") {
        $code = "modal_blocked"
    }
    elseif ($EditorPid -gt 0 -and -not $processAlive) {
        $code = "editor_crashed_before_bridge"
    }

    return [pscustomobject]@{
        code = $code
        editorPid = $EditorPid
        processAlive = $processAlive
        crashReportClients = $crashReportClients
        pendingCrashReports = $pendingCrashReports
        latestEditorLog = $LatestLog
        logExcerpt = $excerpt
    }
}

# Updates the current job file with a new phase or terminal status.
function Update-ReloadJob {
    param(
        [string] $Status = "running",
        [string] $Phase,
        [string] $ErrorMessage = "",
        [int] $ExitCode = 0,
        [hashtable] $Properties = @{}
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
    foreach ($key in $Properties.Keys) {
        $job | Add-Member -NotePropertyName $key -NotePropertyValue $Properties[$key] -Force
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

# Returns the requested UBT parallel action cap from explicit args, environment, or the job reason.
function Get-RequestedMaxParallelActions {
    if ($MaxParallelActions -gt 0) {
        return $MaxParallelActions
    }

    if ($env:UE_MCP_MAX_PARALLEL_ACTIONS -and $env:UE_MCP_MAX_PARALLEL_ACTIONS -match '^\d+$') {
        return [int] $env:UE_MCP_MAX_PARALLEL_ACTIONS
    }

    $jobPath = Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$JobId.json"
    $job = Read-JsonFile -Path $jobPath
    $reason = [string] (Get-ObjectProperty -Object $job -Name "reason" -DefaultValue "")
    if ($reason -match '(?i)MaxParallel(?:Actions|Count)\s*=\s*(\d+)') {
        return [int] $Matches[1]
    }

    return 0
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
        try {
            if ($client.State -ne [System.Net.WebSockets.WebSocketState]::Closed -and
                $client.State -ne [System.Net.WebSockets.WebSocketState]::Aborted) {
                $client.Abort()
            }
        }
        catch {
        }
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

# Writes a short progress line for visible rebuild helper windows.
function Write-ReloadConsoleStatus {
    param([string] $Message)
    Write-Host ("[{0}] {1}" -f (Get-Date).ToString("HH:mm:ss"), $Message)
}

# Mirrors newly appended process log lines without taking exclusive file access.
function Write-NewProcessLogLines {
    param(
        [string] $Path,
        [long] $Position = 0,
        [string] $ForegroundColor = ""
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{ position = $Position; lines = 0 }
    }

    $stream = $null
    $reader = $null
    $newPosition = $Position
    $lineCount = 0
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        if ($Position -lt 0 -or $Position -gt $stream.Length) {
            $Position = 0
        }
        [void] $stream.Seek($Position, [System.IO.SeekOrigin]::Begin)
        $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8, $true)
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($null -eq $line) {
                break
            }
            if ($ForegroundColor) {
                Write-Host $line -ForegroundColor $ForegroundColor
            }
            else {
                Write-Host $line
            }
            $lineCount++
        }
        $newPosition = $stream.Position
    }
    catch [System.IO.IOException] {
        return [pscustomobject]@{ position = $Position; lines = 0 }
    }
    catch [System.UnauthorizedAccessException] {
        return [pscustomobject]@{ position = $Position; lines = 0 }
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }

    return [pscustomobject]@{ position = $newPosition; lines = $lineCount }
}

# Runs Build.bat, persists stdout/stderr logs, and mirrors UBT output when visible.
function Invoke-ClientEditorBuild {
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRootPath
    $buildBat = Resolve-ReloadUnrealBuildBatch -ProjectFile $projectFile
    $requestedMaxParallelActions = Get-RequestedMaxParallelActions
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
    if ($requestedMaxParallelActions -gt 0) {
        $arguments += "-MaxParallelActions=$requestedMaxParallelActions"
    }

    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    Write-ReloadConsoleStatus "Starting UBT build for $(Get-EditorTargetName)."
    if ($requestedMaxParallelActions -gt 0) {
        Write-ReloadConsoleStatus "MaxParallelActions=$requestedMaxParallelActions"
    }
    Write-ReloadConsoleStatus "Stdout log: $stdoutPath"
    Write-ReloadConsoleStatus "Stderr log: $stderrPath"

    $process = Start-Process `
        -FilePath $buildBat `
        -ArgumentList (Join-ReloadProcessArguments -Arguments $arguments) `
        -WorkingDirectory $projectRootPath `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    $stdoutPosition = 0L
    $stderrPosition = 0L
    $lastActivityUtc = [DateTime]::UtcNow
    while (-not $process.HasExited) {
        $stdoutResult = Write-NewProcessLogLines -Path $stdoutPath -Position $stdoutPosition
        $stderrResult = Write-NewProcessLogLines -Path $stderrPath -Position $stderrPosition -ForegroundColor "Yellow"
        $stdoutPosition = [long] $stdoutResult.position
        $stderrPosition = [long] $stderrResult.position
        $linesWritten = [int] $stdoutResult.lines + [int] $stderrResult.lines
        if ($linesWritten -gt 0) {
            $lastActivityUtc = [DateTime]::UtcNow
        }
        elseif (([DateTime]::UtcNow - $lastActivityUtc).TotalSeconds -ge 20) {
            Write-ReloadConsoleStatus "UBT build still running (pid $($process.Id))."
            $lastActivityUtc = [DateTime]::UtcNow
        }
        Start-Sleep -Milliseconds 300
    }
    $process.WaitForExit()
    Write-NewProcessLogLines -Path $stdoutPath -Position $stdoutPosition | Out-Null
    Write-NewProcessLogLines -Path $stderrPath -Position $stderrPosition -ForegroundColor "Yellow" | Out-Null

    $exitCode = [int] (Get-ObjectProperty -Object $process -Name "ExitCode" -DefaultValue 0)
    if ($exitCode -ne 0) {
        Write-ReloadConsoleStatus "UBT build failed with exit code $exitCode."
        throw "Build failed with exit code $exitCode. Logs: $stdoutPath, $stderrPath"
    }
    Write-ReloadConsoleStatus "UBT build completed successfully."
    return [pscustomobject]@{
        stdout = $stdoutPath
        stderr = $stderrPath
        buildPid = $process.Id
        exitCode = $exitCode
        maxParallelActions = $requestedMaxParallelActions
        completedAt = [DateTime]::UtcNow.ToString("o")
    }
}

# Returns the editor target name from the project file name.
function Get-EditorTargetName {
    return Get-ReloadEditorTargetName -ProjectFile (Get-ReloadProjectFile -ProjectRoot $projectRootPath)
}

# Starts the editor and waits for the bridge port lockfile to advertise it.
function Start-EditorAndWaitForBridge {
    param([int] $TimeoutSeconds = 180)
    Assert-NoPendingUnrealCrashReportClients
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRootPath
    $editor = Resolve-ReloadUnrealEditor -ProjectFile $projectFile
    $editorArgs = Get-EditorLaunchArgs
    $portPath = Join-Path (Get-ReloadStateDirectory) "port.json"
    if (Test-Path -LiteralPath $portPath -PathType Leaf) {
        Remove-Item -LiteralPath $portPath -Force
    }

    Update-ReloadJob -Phase "launching_editor" -Properties @{
        editorExecutable = $editor
        editorArgs = $editorArgs
        mcpSafeLaunch = [bool] $McpSafeLaunch
    }
    Update-MaintenancePhase -Phase "launching_editor"
    $process = Start-Process `
        -FilePath $editor `
        -ArgumentList (Join-ReloadProcessArguments -Arguments (@($projectFile) + $editorArgs)) `
        -WorkingDirectory $projectRootPath `
        -PassThru

    Update-ReloadJob -Phase "waiting_for_bridge" -Properties @{ launchedEditorPid = $process.Id }
    Update-MaintenancePhase -Phase "waiting_for_bridge"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ($true) {
        $portState = Get-PortState
        $pidValue = Get-ObjectProperty -Object $portState -Name "pid"
        if ($null -ne $portState -and $pidValue -and (Get-Process -Id ([int] $pidValue) -ErrorAction SilentlyContinue)) {
            return [pscustomobject]@{ editorPid = $process.Id; portState = $portState }
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            $diagnosis = Get-EditorStartupDiagnosis -EditorPid $process.Id
            $code = [string] (Get-ObjectProperty -Object $diagnosis -Name "code" -DefaultValue "port_timeout")
            $phase = "port_timeout"
            if ($code -eq "editor_crashed_before_bridge") {
                $phase = "editor_crashed"
            }
            elseif ($code -eq "modal_blocked") {
                $phase = "modal_blocked"
            }
            elseif ($code -eq "crash_report_pending") {
                $phase = "crash_report_pending"
            }
            Update-ReloadJob -Status "failed" -Phase $phase -ErrorMessage "Timed out waiting for UE_MCP_Bridge port.json after editor restart." -Properties @{
                launchedEditorPid = $process.Id
                editorStartupDiagnosis = $diagnosis
            }
            Update-MaintenancePhase -Phase $phase
            throw "${code}: Timed out waiting for UE_MCP_Bridge port.json after editor restart."
        }
        Start-Sleep -Seconds 2
    }
}

$editorLock = $null
try {
    $editorLock = Enter-EditorLock

    if ($ColdStart) {
        Update-ReloadJob -Phase "cold_start" -Properties @{ helperPid = [System.Diagnostics.Process]::GetCurrentProcess().Id }
        Update-MaintenancePhase -Phase "cold_start"
    }
    else {
        Update-ReloadJob -Phase "draining" -Properties @{ helperPid = [System.Diagnostics.Process]::GetCurrentProcess().Id }
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
    }

    Update-ReloadJob -Phase "building"
    Update-MaintenancePhase -Phase "building"
    $logs = Invoke-ClientEditorBuild

    Update-ReloadJob -Phase "building_done" -Properties @{ logs = $logs }
    Update-MaintenancePhase -Phase "building_done"
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
    $jobPath = Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$JobId.json"
    $job = Read-JsonFile -Path $jobPath
    $currentPhase = [string] (Get-ObjectProperty -Object $job -Name "phase" -DefaultValue "")
    $failedPhase = "failed"
    if ($_.Exception.Message -match "crash_report_pending") {
        $failedPhase = "crash_report_pending"
    }
    elseif (@("port_timeout", "editor_crashed", "modal_blocked", "crash_report_pending") -contains $currentPhase) {
        $failedPhase = $currentPhase
    }
    elseif ($currentPhase -eq "waiting_for_bridge" -or $currentPhase -eq "launching_editor" -or $currentPhase -eq "building_done") {
        $failedPhase = "restart_failed"
    }
    Update-MaintenancePhase -Phase $failedPhase
    Update-ReloadJob -Status "failed" -Phase $failedPhase -ErrorMessage $_.Exception.Message
}
finally {
    if ($null -ne $editorLock) {
        $editorLock.Dispose()
    }
}
