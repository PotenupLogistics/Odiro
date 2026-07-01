# Editor lifecycle MCP server for coordinated Unreal rebuild/restart.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:Stdout = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $script:Utf8NoBom)
$script:Stdout.AutoFlush = $true
[Console]::InputEncoding = $script:Utf8NoBom
[Console]::OutputEncoding = $script:Utf8NoBom

# Writes diagnostics away from the MCP stdout protocol stream.
function Write-ReloadLog {
    param([string] $Message)
    [Console]::Error.WriteLine("[editor-reload-mcp] $Message")
}

# Returns the UE_MCP_Bridge plugin root from this script location.
function Get-PluginRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
}

. (Join-Path (Get-PluginRoot) "Resources\Automation\UnrealProjectTools.ps1")
. (Join-Path (Get-PluginRoot) "Resources\Automation\CrashReportTools.ps1")

# Returns the Unreal project root that owns the plugin and .uproject file.
function Get-ProjectRootFromPlugin {
    return (Resolve-Path -LiteralPath (Join-Path (Get-PluginRoot) "..\..")).Path
}

# Returns the shared runtime state directory used by bridge and reload tools.
function Get-ReloadStateDirectory {
    $stateDir = Join-Path (Get-ProjectRootFromPlugin) "Saved\UE_MCP_Bridge"
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
    $json = $Value | ConvertTo-Json -Depth 50
    [System.IO.File]::WriteAllText($Path, $json, $script:Utf8NoBom)
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

# Returns whether a maintenance/job phase is terminal and should not gate bridge calls.
function Test-TerminalPhase {
    param([string] $Phase)
    return @("completed", "failed", "restart_failed", "port_timeout", "editor_crashed", "modal_blocked", "crash_report_pending") -contains $Phase
}

# Returns whether a persisted maintenance sentinel is terminal and recoverable.
function Test-TerminalMaintenanceState {
    param([object] $Maintenance)
    if ($null -eq $Maintenance) {
        return $false
    }
    $phase = [string] (Get-ObjectProperty -Object $Maintenance -Name "phase" -DefaultValue "")
    if (Test-TerminalPhase -Phase $phase) {
        return $true
    }
    $jobId = [string] (Get-ObjectProperty -Object $Maintenance -Name "jobId" -DefaultValue "")
    if (-not $jobId) {
        return $false
    }
    $job = Read-JsonFile -Path (Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$jobId.json")
    $status = [string] (Get-ObjectProperty -Object $job -Name "status" -DefaultValue "")
    return @("completed", "failed") -contains $status
}

# Returns the newest editor log path available for restart diagnostics.
function Get-LatestEditorLogPath {
    $logsDir = Join-Path (Get-ProjectRootFromPlugin) "Saved\Logs"
    if (-not (Test-Path -LiteralPath $logsDir -PathType Container)) {
        return ""
    }
    $projectFile = Get-ReloadProjectFile -ProjectRoot (Get-ProjectRootFromPlugin)
    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($projectFile)
    $logs = @(Get-ChildItem -LiteralPath $logsDir -File -Filter "$projectName*.log" | Sort-Object LastWriteTimeUtc -Descending)
    if ($logs.Count -eq 0) {
        return ""
    }
    return $logs[0].FullName
}

# Returns the tail of a log file without assuming exclusive access.
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

# Classifies startup failures from process state and the latest editor log.
function Get-EditorStartupDiagnosis {
    param(
        [int] $EditorPid = 0,
        [string] $LatestLog = ""
    )
    $processAlive = $false
    if ($EditorPid -gt 0) {
        $processAlive = $null -ne (Get-Process -Id $EditorPid -ErrorAction SilentlyContinue)
    }
    if (-not $LatestLog) {
        $LatestLog = Get-LatestEditorLogPath
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

# Acquires an exclusive file lock used to coordinate per-session MCP processes.
function Enter-ReloadFileLock {
    param(
        [string] $Name,
        [int] $TimeoutMs = 10000
    )
    $path = Join-Path (Get-ReloadStateDirectory) $Name
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ($true) {
        try {
            return [System.IO.File]::Open($path, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        }
        catch {
            if ([DateTime]::UtcNow -ge $deadline) {
                throw "Timed out acquiring lock: $path"
            }
            Start-Sleep -Milliseconds 200
        }
    }
}

# Releases a file lock returned by Enter-ReloadFileLock.
function Exit-ReloadFileLock {
    param($Lock)
    if ($null -ne $Lock) {
        $Lock.Dispose()
    }
}

# Returns the current bridge port state advertised by the editor.
function Get-PortState {
    $path = Join-Path (Get-ReloadStateDirectory) "port.json"
    $state = Read-JsonFile -Path $path
    if ($null -eq $state) {
        return [pscustomobject]@{ path = $path; exists = $false; processAlive = $false; classification = "missing" }
    }
    $alive = $false
    $pidValue = Get-ObjectProperty -Object $state -Name "pid"
    if ($pidValue) {
        $alive = $null -ne (Get-Process -Id ([int] $pidValue) -ErrorAction SilentlyContinue)
    }
    $classification = "missing_pid"
    if ($pidValue -and -not $alive) {
        $classification = "stale_pid"
    }
    elseif ($pidValue -and $alive) {
        $classification = "process_alive"
    }
    $state | Add-Member -NotePropertyName path -NotePropertyValue $path -Force
    $state | Add-Member -NotePropertyName exists -NotePropertyValue $true -Force
    $state | Add-Member -NotePropertyName processAlive -NotePropertyValue $alive -Force
    $state | Add-Member -NotePropertyName classification -NotePropertyValue $classification -Force
    return $state
}

# Returns whether a forced rebuild may safely start without bridge coordination.
function Get-ColdStartEligibility {
    $portState = Get-PortState
    $projectRoot = (Get-ProjectRootFromPlugin).TrimEnd([char[]] @('\', '/'))
    $projectFile = Get-ReloadProjectFile -ProjectRoot $projectRoot
    $normalizedRoot = [System.IO.Path]::GetFullPath($projectRoot).TrimEnd([char[]] @('\', '/')).ToLowerInvariant()
    $normalizedProjectFile = [System.IO.Path]::GetFullPath($projectFile).ToLowerInvariant()
    $blockingProcesses = @()

    try {
        $editorProcesses = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" -ErrorAction Stop)
        foreach ($process in $editorProcesses) {
            $commandLine = [string] (Get-ObjectProperty -Object $process -Name "CommandLine" -DefaultValue "")
            $normalizedCommandLine = $commandLine.Replace('/', '\').ToLowerInvariant()
            $ownsProject = $normalizedCommandLine.Contains($normalizedProjectFile.Replace('/', '\')) -or
                $normalizedCommandLine.Contains($normalizedRoot.Replace('/', '\'))
            $unknownOwner = -not $commandLine
            if ($ownsProject -or $unknownOwner) {
                $blockingProcesses += [pscustomobject]@{
                    pid = [int] $process.ProcessId
                    name = [string] $process.Name
                    ownsProject = $ownsProject
                    unknownOwner = $unknownOwner
                    commandLine = $commandLine
                }
            }
        }
    }
    catch {
        $fallbackProcesses = @(Get-Process | Where-Object { $_.ProcessName -like "UnrealEditor*" })
        foreach ($process in $fallbackProcesses) {
            $blockingProcesses += [pscustomobject]@{
                pid = [int] $process.Id
                name = [string] $process.ProcessName
                ownsProject = $false
                unknownOwner = $true
                commandLine = ""
            }
        }
    }

    $portAlive = [bool] (Get-ObjectProperty -Object $portState -Name "processAlive" -DefaultValue $false)
    $allowed = (-not $portAlive) -and $blockingProcesses.Count -eq 0
    $reason = "safe_no_live_editor"
    if ($portAlive) {
        $reason = "bridge_port_process_alive"
    }
    elseif ($blockingProcesses.Count -gt 0) {
        $reason = "editor_process_alive"
    }

    return [pscustomobject]@{
        allowed = $allowed
        reason = $reason
        portState = $portState
        blockingEditorProcesses = $blockingProcesses
    }
}

# Builds a compact action-advice object for recoverable lifecycle responses.
function New-ReloadActionAdvice {
    param(
        [bool] $CanAutoRecover = $false,
        [bool] $BlockedByCrashReporter = $false,
        [string] $RecommendedNextTool = "",
        [hashtable] $RecommendedArgs = @{},
        [string] $RecommendedAction = ""
    )
    return [pscustomobject]@{
        canAutoRecover = $CanAutoRecover
        blockedByCrashReporter = $BlockedByCrashReporter
        recommendedNextTool = $RecommendedNextTool
        recommendedArgs = [pscustomobject] $RecommendedArgs
        recommendedAction = $RecommendedAction
    }
}

# Adds action-advice fields to an existing response object.
function Add-ReloadActionAdvice {
    param(
        [object] $Result,
        [object] $Advice
    )
    if ($null -eq $Result -or $null -eq $Advice) {
        return $Result
    }
    foreach ($property in $Advice.PSObject.Properties) {
        $Result | Add-Member -NotePropertyName $property.Name -NotePropertyValue $property.Value -Force
    }
    return $Result
}

# Returns a small summary of a recovery result suitable for embedding in job state.
function New-ReloadRecoverySummary {
    param([object] $Recovery)
    if ($null -eq $Recovery) {
        return $null
    }
    return [pscustomobject]@{
        code = [string] (Get-ObjectProperty -Object $Recovery -Name "code" -DefaultValue "")
        reason = [string] (Get-ObjectProperty -Object $Recovery -Name "reason" -DefaultValue "")
        actions = @((Get-ObjectProperty -Object $Recovery -Name "actions" -DefaultValue @()))
        success = [bool] (Get-ObjectProperty -Object $Recovery -Name "success" -DefaultValue $false)
    }
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
    if (-not (Get-ObjectProperty -Object $portState -Name "exists" -DefaultValue $false) -or -not $port) {
        throw "UE_MCP_Bridge port.json is missing."
    }

    $client = [System.Net.WebSockets.ClientWebSocket]::new()
    $cts = [System.Threading.CancellationTokenSource]::new()
    $cts.CancelAfter($TimeoutMs)
    try {
        $uri = [Uri]::new("ws://127.0.0.1:$([int] $port)")
        $client.ConnectAsync($uri, $cts.Token).GetAwaiter().GetResult() | Out-Null

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
            Write-ReloadLog "Failed to abort bridge WebSocket: $($_.Exception.Message)"
        }
        $client.Dispose()
        $cts.Dispose()
    }
}

# Extracts a bridge result or raises the JSON-RPC error message.
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

# Returns the latest reload state known to files and the editor bridge.
function Get-ReloadStatus {
    $stateDir = Get-ReloadStateDirectory
    $maintenancePath = Join-Path $stateDir "maintenance.json"
    $maintenance = Read-JsonFile -Path $maintenancePath
    $maintenanceTerminal = Test-TerminalMaintenanceState -Maintenance $maintenance
    $currentJob = $null
    $jobId = [string] (Get-ObjectProperty -Object $maintenance -Name "jobId" -DefaultValue "")
    if ($jobId) {
        $currentJob = Read-JsonFile -Path (Join-Path (Join-Path $stateDir "jobs") "$jobId.json")
    }
    $port = Get-PortState
    $bridge = $null
    $bridgeError = $null
    try {
        $bridgeResponse = Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 3000
        $bridge = Get-BridgeResult -Response $bridgeResponse
    }
    catch {
        $bridgeError = $_.Exception.Message
    }
    if ($bridge) {
        $port | Add-Member -NotePropertyName classification -NotePropertyValue "ready" -Force
    }
    elseif ((Get-ObjectProperty -Object $port -Name "exists" -DefaultValue $false) -and
        (Get-ObjectProperty -Object $port -Name "processAlive" -DefaultValue $false)) {
        $port | Add-Member -NotePropertyName classification -NotePropertyValue "process_alive_but_bridge_down" -Force
    }

    $latestEditorLog = Get-LatestEditorLogPath
    $portPid = [int] (Get-ObjectProperty -Object $port -Name "pid" -DefaultValue 0)
    $crashReportClients = @(Get-UnrealCrashReportClientStates)
    $pendingCrashReports = @($crashReportClients | Where-Object { $_.blocksEditorLaunch })

    $result = [pscustomobject]@{
        success = $true
        stateDirectory = $stateDir
        maintenancePath = $maintenancePath
        maintenance = ($null -ne $maintenance)
        maintenanceTerminal = $maintenanceTerminal
        maintenanceState = $maintenance
        currentJob = $currentJob
        portState = $port
        bridgeStatus = $bridge
        bridgeError = $bridgeError
        crashReportClients = $crashReportClients
        pendingCrashReports = $pendingCrashReports
        latestEditorLog = $latestEditorLog
        editorStartupDiagnosis = (Get-EditorStartupDiagnosis -EditorPid $portPid -LatestLog $latestEditorLog)
    }

    $portClassification = [string] (Get-ObjectProperty -Object $port -Name "classification" -DefaultValue "")
    if ($pendingCrashReports.Count -gt 0) {
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $true `
            -RecommendedNextTool "editor_reload_recover" `
            -RecommendedArgs @{ reason = "retry after closing Unreal Crash Reporter" } `
            -RecommendedAction "Close or submit the Unreal Crash Reporter dialog before relaunching the editor.")
    }
    if ($maintenanceTerminal -or $portClassification -eq "stale_pid") {
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $true `
            -BlockedByCrashReporter $false `
            -RecommendedNextTool "editor_reload_recover_and_restart" `
            -RecommendedArgs @{ wait = $true; reason = "recover stale editor reload state" } `
            -RecommendedAction "Recover stale reload state and cold-start the editor if no live editor remains.")
    }
    if ($bridgeError -and $portClassification -eq "process_alive_but_bridge_down") {
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $false `
            -RecommendedNextTool "editor_reload_get_status" `
            -RecommendedArgs @{} `
            -RecommendedAction "The editor process is alive but the bridge is unreachable; inspect the editor or close it before restart.")
    }
    return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice)
}

# Updates a reload job file with status, phase, and optional details.
function Update-ReloadJobFile {
    param(
        [string] $JobId,
        [string] $Status,
        [string] $Phase,
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
    foreach ($key in $Properties.Keys) {
        $job | Add-Member -NotePropertyName $key -NotePropertyValue $Properties[$key] -Force
    }
    Write-JsonFile -Path $jobPath -Value $job
    return $job
}

# Updates the maintenance sentinel while a reload MCP operation owns it.
function Update-MaintenanceFile {
    param(
        [string] $JobId,
        [string] $Reason,
        [string] $Operation,
        [string] $Phase
    )
    $maintenancePath = Join-Path (Get-ReloadStateDirectory) "maintenance.json"
    $maintenance = Read-JsonFile -Path $maintenancePath
    if ($null -eq $maintenance) {
        $maintenance = [pscustomobject]@{ requestedAt = [DateTime]::UtcNow.ToString("o") }
    }
    $maintenance | Add-Member -NotePropertyName jobId -NotePropertyValue $JobId -Force
    $maintenance | Add-Member -NotePropertyName reason -NotePropertyValue $Reason -Force
    $maintenance | Add-Member -NotePropertyName operation -NotePropertyValue $Operation -Force
    $maintenance | Add-Member -NotePropertyName phase -NotePropertyValue $Phase -Force
    $maintenance | Add-Member -NotePropertyName source -NotePropertyValue "EditorReloadMcp" -Force
    $maintenance | Add-Member -NotePropertyName updatedAt -NotePropertyValue ([DateTime]::UtcNow.ToString("o")) -Force
    Write-JsonFile -Path $maintenancePath -Value $maintenance
}

# Removes the maintenance sentinel owned by a completed operation.
function Clear-MaintenanceFile {
    param([string] $JobId = "")
    $maintenancePath = Join-Path (Get-ReloadStateDirectory) "maintenance.json"
    if (Test-Path -LiteralPath $maintenancePath -PathType Leaf) {
        if ($JobId) {
            $maintenance = Read-JsonFile -Path $maintenancePath
            $ownerJobId = Get-ObjectProperty -Object $maintenance -Name "jobId"
            if ($ownerJobId -and $ownerJobId -ne $JobId) {
                return
            }
        }
        Remove-Item -LiteralPath $maintenancePath -Force
    }
}

# Returns a lower-case SHA-256 hash for a file that may be open in another tool.
function Get-FileSha256Hex {
    param([string] $Path)
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
        $stream.Dispose()
    }
}

# Converts an absolute path to a Client-root-relative path for stable fingerprinting.
function ConvertTo-ClientRelativePath {
    param([string] $Path)
    $projectRoot = (Get-ProjectRootFromPlugin).TrimEnd([char[]] @('\', '/'))
    $fullPath = (Resolve-Path -LiteralPath $Path).Path
    if ($fullPath.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($projectRoot.Length).TrimStart([char[]] @('\', '/')).Replace('\', '/')
    }
    return $fullPath.Replace('\', '/')
}

# Builds a conservative fingerprint of C++ build inputs owned by the Unreal project.
function Get-SourceStateFingerprint {
    $projectRoot = Get-ProjectRootFromPlugin
    $extensions = @(".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".cs", ".uproject", ".uplugin")
    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]

    $sourceRoot = Join-Path $projectRoot "Source"
    if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
            Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() } |
            ForEach-Object { $files.Add($_) }
    }

    $pluginsRoot = Join-Path $projectRoot "Plugins"
    if (Test-Path -LiteralPath $pluginsRoot -PathType Container) {
        Get-ChildItem -LiteralPath $pluginsRoot -Recurse -File |
            Where-Object {
                $extension = $_.Extension.ToLowerInvariant()
                ($extension -eq ".uplugin") -or
                    (($extensions -contains $extension) -and ($_.FullName -match "\\Source\\"))
            } |
            ForEach-Object { $files.Add($_) }
    }

    $projectFiles = @(Get-ChildItem -LiteralPath $projectRoot -Filter "*.uproject" -File)
    foreach ($projectFile in $projectFiles) {
        $files.Add($projectFile)
    }

    $records = New-Object System.Text.StringBuilder
    $sortedFiles = @($files | Sort-Object { ConvertTo-ClientRelativePath -Path $_.FullName })
    foreach ($file in $sortedFiles) {
        $relativePath = ConvertTo-ClientRelativePath -Path $file.FullName
        $fileHash = Get-FileSha256Hex -Path $file.FullName
        [void] $records.Append($relativePath).Append("`t").Append($file.Length).Append("`t").Append($file.LastWriteTimeUtc.Ticks).Append("`t").Append($fileHash).Append("`n")
    }

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $script:Utf8NoBom.GetBytes($records.ToString())
        $hash = ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }

    return [pscustomobject]@{
        hash = $hash
        fileCount = $sortedFiles.Count
        generatedAt = [DateTime]::UtcNow.ToString("o")
    }
}

# Reads the most recent successful reload/build fingerprint.
function Get-LastSuccessfulReload {
    return Read-JsonFile -Path (Join-Path (Get-ReloadStateDirectory) "last-success.json")
}

# Records the latest successful reload/build fingerprint for idempotent requests.
function Set-LastSuccessfulReload {
    param(
        [string] $JobId,
        [string] $Operation,
        [object] $SourceFingerprint
    )
    if ($null -eq $SourceFingerprint) {
        return
    }
    Write-JsonFile -Path (Join-Path (Get-ReloadStateDirectory) "last-success.json") -Value ([pscustomobject]@{
        jobId = $JobId
        operation = $Operation
        sourceFingerprint = $SourceFingerprint
        completedAt = [DateTime]::UtcNow.ToString("o")
    })
}

# Compares two source fingerprint objects by their stable hash field.
function Test-SourceFingerprintEqual {
    param(
        [object] $Left,
        [object] $Right
    )
    $leftHash = [string] (Get-ObjectProperty -Object $Left -Name "hash" -DefaultValue "")
    $rightHash = [string] (Get-ObjectProperty -Object $Right -Name "hash" -DefaultValue "")
    return ($leftHash -and $rightHash -and $leftHash -eq $rightHash)
}

# Returns true when a completed job started from the same source state that exists now.
function Test-CompletedJobIncludesSource {
    param(
        [object] $Job,
        [object] $CurrentSourceFingerprint
    )
    if ((Get-ObjectProperty -Object $Job -Name "status") -ne "completed") {
        return $false
    }
    $jobSourceFingerprint = Get-ObjectProperty -Object $Job -Name "sourceFingerprintAtStart"
    return (Test-SourceFingerprintEqual -Left $jobSourceFingerprint -Right $CurrentSourceFingerprint)
}

# Creates a successful skipped response for idempotent reload requests.
function New-SkippedReloadResult {
    param(
        [string] $Operation,
        [string] $Code,
        [string] $Reason,
        [object] $SourceFingerprint,
        [object] $IncludedByJob = $null,
        [object] $LastSuccess = $null,
        [object] $UpToDateCheck = $null
    )
    return [pscustomobject]@{
        success = $true
        accepted = $false
        skipped = $true
        code = $Code
        operation = $Operation
        reason = $Reason
        sourceFingerprint = $SourceFingerprint
        includedByJob = $IncludedByJob
        lastSuccess = $LastSuccess
        upToDateCheck = $UpToDateCheck
    }
}

# Returns a normalized Live Coding status without starting a compile.
function Get-LiveCodingStatus {
    try {
        $bridgeStatus = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 3000)
        $liveCoding = Get-ObjectProperty -Object $bridgeStatus -Name "liveCoding"
        if ($null -ne $liveCoding) {
            return [pscustomobject]@{
                success = $true
                source = "coordination_get_status"
                liveCoding = $liveCoding
                bridgeStatus = $bridgeStatus
            }
        }
        return [pscustomobject]@{
            success = $false
            code = "live_coding_status_unavailable"
            error = "coordination_get_status did not include liveCoding state."
            bridgeStatus = $bridgeStatus
        }
    }
    catch {
        return [pscustomobject]@{
            success = $false
            code = "live_coding_status_unavailable"
            error = $_.Exception.Message
        }
    }
}

# Returns whether a normalized Live Coding status reports an active compile.
function Test-LiveCodingCompiling {
    param([object] $Status)
    $liveCoding = Get-ObjectProperty -Object $Status -Name "liveCoding" -DefaultValue $Status
    return [bool] (Get-ObjectProperty -Object $liveCoding -Name "compiling" -DefaultValue $false)
}

# Waits for an already-started Live Coding compile to finish without starting another one.
function Wait-LiveCodingIdle {
    param([int] $TimeoutMs = 300000)
    $startedAt = [DateTime]::UtcNow
    $deadline = $startedAt.AddMilliseconds($TimeoutMs)
    $lastStatus = $null
    while ($true) {
        $lastStatus = Get-LiveCodingStatus
        if (-not (Get-ObjectProperty -Object $lastStatus -Name "success" -DefaultValue $false)) {
            return $lastStatus
        }
        if (-not (Test-LiveCodingCompiling -Status $lastStatus)) {
            return [pscustomobject]@{
                success = $true
                code = "live_coding_idle"
                waitedMs = [int] ([DateTime]::UtcNow - $startedAt).TotalMilliseconds
                liveCodingStatus = $lastStatus
            }
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            return [pscustomobject]@{
                success = $false
                code = "live_coding_wait_timeout"
                status = "timeout"
                error = "Timed out waiting for the active Live Coding compile after $TimeoutMs ms."
                liveCodingStatus = $lastStatus
            }
        }
        Start-Sleep -Milliseconds 1000
    }
}

# Joins an active Live Coding compile, then reruns the source freshness check.
function Wait-LiveCodingJoinAndCheck {
    param(
        [string] $Operation = "live_coding_join_recheck",
        [int] $TimeoutMs = 300000
    )
    $sourceFingerprintBeforeJoin = Get-SourceStateFingerprint
    $wait = Wait-LiveCodingIdle -TimeoutMs $TimeoutMs
    if (-not (Get-ObjectProperty -Object $wait -Name "success" -DefaultValue $false)) {
        return [pscustomobject]@{
            success = $false
            code = (Get-ObjectProperty -Object $wait -Name "code" -DefaultValue "live_coding_wait_failed")
            wait = $wait
            sourceFingerprintBeforeJoin = $sourceFingerprintBeforeJoin
        }
    }

    $sourceFingerprintAfterJoin = Get-SourceStateFingerprint
    $upToDateCheck = Test-SourceBuildUpToDate -Operation $Operation -RequestId ([guid]::NewGuid().ToString("N")) -TimeoutMs $TimeoutMs
    $sourceFingerprintAfterCheck = Get-SourceStateFingerprint
    $checkSucceeded = Get-ObjectProperty -Object $upToDateCheck -Name "success" -DefaultValue $false
    $checkUpToDate = Get-ObjectProperty -Object $upToDateCheck -Name "upToDate" -DefaultValue $false
    return [pscustomobject]@{
        success = $true
        code = "live_coding_join_checked"
        wait = $wait
        sourceFingerprintBeforeJoin = $sourceFingerprintBeforeJoin
        sourceFingerprintAfterJoin = $sourceFingerprintAfterJoin
        sourceFingerprintAfterCheck = $sourceFingerprintAfterCheck
        upToDateCheck = $upToDateCheck
        upToDate = ($checkSucceeded -and $checkUpToDate -and (Test-SourceFingerprintEqual -Left $sourceFingerprintAfterJoin -Right $sourceFingerprintAfterCheck))
    }
}

# Returns the editor target name from the project file name.
function Get-EditorTargetName {
    return Get-ReloadEditorTargetName -ProjectFile (Get-ReloadProjectFile -ProjectRoot (Get-ProjectRootFromPlugin))
}

# Counts outdated actions from UBT's -WriteOutdatedActions JSON shape.
function Get-OutdatedActionCount {
    param([object] $OutdatedActions)
    if ($null -eq $OutdatedActions) {
        return 0
    }
    if ($OutdatedActions -is [array]) {
        return $OutdatedActions.Count
    }
    $actions = Get-ObjectProperty -Object $OutdatedActions -Name "Actions"
    if ($null -eq $actions) {
        $actions = Get-ObjectProperty -Object $OutdatedActions -Name "actions"
    }
    if ($null -eq $actions) {
        return 0
    }
    return @($actions).Count
}

# Runs UBT in check-only mode and reports whether the editor target has stale actions.
function Test-SourceBuildUpToDate {
    param(
        [string] $Operation,
        [string] $RequestId,
        [int] $TimeoutMs = 60000
    )
    $stateDir = Get-ReloadStateDirectory
    $checksDir = Join-Path $stateDir "checks"
    if (-not (Test-Path -LiteralPath $checksDir -PathType Container)) {
        New-Item -ItemType Directory -Path $checksDir -Force | Out-Null
    }

    if (-not $RequestId) {
        $RequestId = [guid]::NewGuid().ToString("N")
    }

    $projectFile = Get-ReloadProjectFile -ProjectRoot (Get-ProjectRootFromPlugin)
    $buildBat = Resolve-ReloadUnrealBuildBatch -ProjectFile $projectFile
    $outdatedPath = Join-Path $checksDir "$RequestId-outdated-actions.json"
    $stdoutPath = Join-Path $checksDir "$RequestId-check.out.log"
    $stderrPath = Join-Path $checksDir "$RequestId-check.err.log"
    if (Test-Path -LiteralPath $outdatedPath -PathType Leaf) {
        Remove-Item -LiteralPath $outdatedPath -Force
    }

    $arguments = @(
        (Get-EditorTargetName),
        "Win64",
        "Development",
        "-Project=$projectFile",
        "-WaitMutex",
        "-FromMsBuild",
        "-WriteOutdatedActions=$outdatedPath"
    )

    $process = Start-Process `
        -FilePath $buildBat `
        -ArgumentList (Join-ReloadProcessArguments -Arguments $arguments) `
        -WorkingDirectory (Get-ProjectRootFromPlugin) `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    $outdatedActionCount = 0
    $checkSucceeded = $false
    $code = "check_failed"
    $errorMessage = ""
    $exitCode = -1
    $finished = $process.WaitForExit([Math]::Max($TimeoutMs, 1000))
    if ($finished) {
        $process.Refresh()
        $exitCode = [int] (Get-ObjectProperty -Object $process -Name "ExitCode" -DefaultValue 0)
    }
    else {
        try {
            $process.Kill()
        }
        catch {
            Write-ReloadLog "Failed to kill timed-out UBT check process: $($_.Exception.Message)"
        }
        $code = "timeout"
        $errorMessage = "Timed out waiting for UBT check-only after $TimeoutMs ms."
    }

    if ($exitCode -eq 0 -and (Test-Path -LiteralPath $outdatedPath -PathType Leaf)) {
        $outdatedActionCount = Get-OutdatedActionCount -OutdatedActions (Read-JsonFile -Path $outdatedPath)
        $checkSucceeded = $true
        $code = "ok"
    }
    elseif ($code -ne "timeout" -and (Test-Path -LiteralPath $stdoutPath -PathType Leaf)) {
        $stdoutText = Get-Content -LiteralPath $stdoutPath -Raw
        if ($stdoutText -match "Unable to build while Live Coding is active") {
            $code = "live_coding_active"
            $errorMessage = "UBT check-only is blocked while Live Coding is active; use coordinated Live Coding or force rebuild/restart."
        }
    }

    return [pscustomobject]@{
        success = $checkSucceeded
        code = $code
        operation = $Operation
        upToDate = ($checkSucceeded -and $outdatedActionCount -eq 0)
        outdatedActionCount = $outdatedActionCount
        exitCode = $exitCode
        error = $errorMessage
        outdatedActionsPath = $outdatedPath
        stdout = $stdoutPath
        stderr = $stderrPath
    }
}

# Reports source freshness without starting Live Coding or rebuild/restart work.
function Get-ReloadUpToDateStatus {
    param(
        [string] $Operation = "manual_check",
        [int] $TimeoutMs = 60000
    )
    $lock = Enter-ReloadFileLock -Name "state.lock" -TimeoutMs ([Math]::Max($TimeoutMs, 10000))
    try {
        $sourceFingerprint = Get-SourceStateFingerprint
        $lastSuccess = Get-LastSuccessfulReload
        $lastSuccessFingerprint = Get-ObjectProperty -Object $lastSuccess -Name "sourceFingerprint"
        $matchesLastSuccess = Test-SourceFingerprintEqual -Left $lastSuccessFingerprint -Right $sourceFingerprint
        $upToDateCheck = Test-SourceBuildUpToDate -Operation $Operation -RequestId ([guid]::NewGuid().ToString("N")) -TimeoutMs $TimeoutMs
        return [pscustomobject]@{
            success = $true
            operation = $Operation
            sourceFingerprint = $sourceFingerprint
            lastSuccess = $lastSuccess
            matchesLastSuccess = $matchesLastSuccess
            upToDateCheck = $upToDateCheck
            upToDate = ($matchesLastSuccess -or (Get-ObjectProperty -Object $upToDateCheck -Name "upToDate" -DefaultValue $false))
        }
    }
    finally {
        Exit-ReloadFileLock -Lock $lock
    }
}

# Creates a rebuild job and starts the detached helper.
function Start-RebuildAndRestart {
    param(
        [string] $Reason = "rebuild requested",
        [bool] $Wait = $false,
        [int] $TimeoutMs = 60000,
        [bool] $Force = $false,
        [string[]] $EditorArgs = @(),
        [int] $MaxParallelActions = 0,
        [bool] $McpSafeLaunch = $false
    )
    $stateDir = Get-ReloadStateDirectory
    $maintenancePath = Join-Path $stateDir "maintenance.json"
    $lockTimeoutMs = [Math]::Max($TimeoutMs, 10000)
    $pendingCrashReports = @(Get-PendingUnrealCrashReportClients)
    if ($pendingCrashReports.Count -gt 0) {
        $result = [pscustomobject]@{
            success = $false
            accepted = $false
            code = "crash_report_pending"
            error = "Close the Unreal Crash Report window before starting or restarting another editor."
            pendingCrashReports = $pendingCrashReports
            recoveryTool = "editor_reload_recover"
        }
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $true `
            -RecommendedNextTool "editor_reload_recover" `
            -RecommendedArgs @{ reason = "retry after closing Unreal Crash Reporter" } `
            -RecommendedAction "Close or submit the Unreal Crash Reporter dialog, then retry recovery.")
    }

    $preStartRecovery = Invoke-ReloadRecover -Reason "automatic recovery before rebuild/restart"
    if (-not [bool] (Get-ObjectProperty -Object $preStartRecovery -Name "success" -DefaultValue $false)) {
        $result = [pscustomobject]@{
            success = $false
            accepted = $false
            code = [string] (Get-ObjectProperty -Object $preStartRecovery -Name "code" -DefaultValue "recovery_failed")
            error = [string] (Get-ObjectProperty -Object $preStartRecovery -Name "error" -DefaultValue "Reload recovery failed before rebuild/restart.")
            preStartRecovery = $preStartRecovery
        }
        $blockedByCrashReporter = ([string] (Get-ObjectProperty -Object $result -Name "code" -DefaultValue "")) -eq "crash_report_pending"
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $blockedByCrashReporter `
            -RecommendedNextTool "editor_reload_get_status" `
            -RecommendedArgs @{} `
            -RecommendedAction "Inspect editor/reload status before retrying restart.")
    }

    $preStartRecoverySummary = New-ReloadRecoverySummary -Recovery $preStartRecovery
    $preStartRecoveryActions = New-Object System.Collections.Generic.List[string]
    foreach ($action in @((Get-ObjectProperty -Object $preStartRecovery -Name "actions" -DefaultValue @()))) {
        $preStartRecoveryActions.Add([string] $action)
    }
    $initialColdStartEligibility = Get-ColdStartEligibility
    $preferColdStart = [bool] (Get-ObjectProperty -Object $initialColdStartEligibility -Name "allowed" -DefaultValue $false)

    while ($true) {
        $lock = Enter-ReloadFileLock -Name "state.lock" -TimeoutMs $lockTimeoutMs
        $existingJobId = ""
        $existingOperation = "unknown"
        $jobId = ""
        $jobPath = ""
        try {
            $existing = Read-JsonFile -Path $maintenancePath
            if (Test-TerminalMaintenanceState -Maintenance $existing) {
                Remove-Item -LiteralPath $maintenancePath -Force
                $preStartRecoveryActions.Add("cleared_terminal_maintenance")
                $existing = $null
            }
            $existingJobId = [string] (Get-ObjectProperty -Object $existing -Name "jobId" -DefaultValue "")
            if ($null -ne $existing -and $existingJobId) {
                $existingOperation = [string] (Get-ObjectProperty -Object $existing -Name "operation" -DefaultValue "unknown")
                if (-not $Wait) {
                    return [pscustomobject]@{
                        success = $false
                        accepted = $false
                        code = "maintenance_in_progress"
                        jobId = $existingJobId
                        operation = $existingOperation
                        status = "already_running"
                        maintenancePath = $maintenancePath
                    }
                }
            }
            elseif ($null -ne $existing) {
                return [pscustomobject]@{
                    success = $false
                    accepted = $false
                    code = "maintenance_in_progress"
                    operation = (Get-ObjectProperty -Object $existing -Name "operation" -DefaultValue "unknown")
                    status = "already_running"
                    maintenancePath = $maintenancePath
                }
            }

            if ($existingJobId) {
                # Release the state lock before waiting on a long-running owner job.
            }
            else {
                $sourceFingerprintBeforeCheck = Get-SourceStateFingerprint
                $lastSuccess = Get-LastSuccessfulReload
                $lastSuccessOperation = [string] (Get-ObjectProperty -Object $lastSuccess -Name "operation" -DefaultValue "")
                $lastSuccessFingerprint = Get-ObjectProperty -Object $lastSuccess -Name "sourceFingerprint"
                if (-not $Force -and -not $preferColdStart -and $lastSuccessOperation -eq "rebuild_restart" -and (Test-SourceFingerprintEqual -Left $lastSuccessFingerprint -Right $sourceFingerprintBeforeCheck)) {
                    return New-SkippedReloadResult -Operation "rebuild_restart" -Code "already_built" -Reason "Current source fingerprint matches the last successful rebuild/restart." -SourceFingerprint $sourceFingerprintBeforeCheck -LastSuccess $lastSuccess
                }

                $upToDateCheck = $null
                if (-not $Force -and -not $preferColdStart) {
                    $upToDateCheck = Test-SourceBuildUpToDate -Operation "rebuild_restart" -RequestId ([guid]::NewGuid().ToString("N")) -TimeoutMs $TimeoutMs
                    $sourceFingerprintAfterCheck = Get-SourceStateFingerprint
                    if ((Get-ObjectProperty -Object $upToDateCheck -Name "success" -DefaultValue $false) -and
                        (Get-ObjectProperty -Object $upToDateCheck -Name "upToDate" -DefaultValue $false) -and
                        (Test-SourceFingerprintEqual -Left $sourceFingerprintBeforeCheck -Right $sourceFingerprintAfterCheck)) {
                        return New-SkippedReloadResult -Operation "rebuild_restart" -Code "source_up_to_date" -Reason "UBT reported no outdated editor-target actions." -SourceFingerprint $sourceFingerprintAfterCheck -UpToDateCheck $upToDateCheck
                    }
                    $sourceFingerprint = $sourceFingerprintAfterCheck
                }
                else {
                    $sourceFingerprint = $sourceFingerprintBeforeCheck
                }

                $coldStart = $false
                $coldStartEligibility = $initialColdStartEligibility
                if ($preferColdStart) {
                    $coldStartEligibility = Get-ColdStartEligibility
                    if (Get-ObjectProperty -Object $coldStartEligibility -Name "allowed" -DefaultValue $false) {
                        $coldStart = $true
                        $preStartRecoveryActions.Add("selected_cold_start")
                    }
                    else {
                        $preferColdStart = $false
                    }
                }
                if (-not $coldStart) {
                    try {
                        $null = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 5000)
                    }
                    catch {
                        $coldStartEligibility = Get-ColdStartEligibility
                        if (Get-ObjectProperty -Object $coldStartEligibility -Name "allowed" -DefaultValue $false) {
                            $coldStart = $true
                            $preStartRecoveryActions.Add("selected_cold_start_after_coordination_unavailable")
                            Write-ReloadLog "coordination_get_status unavailable; rebuild/restart will use verified cold start: $($_.Exception.Message)"
                        }
                        else {
                            $result = [pscustomobject]@{
                                success = $false
                                accepted = $false
                                code = "coordination_unavailable"
                                error = $_.Exception.Message
                                maintenancePath = $maintenancePath
                                sourceFingerprint = $sourceFingerprint
                                upToDateCheck = $upToDateCheck
                                coldStartEligibility = $coldStartEligibility
                                preStartRecovery = $preStartRecoverySummary
                            }
                            return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
                                -CanAutoRecover $false `
                                -BlockedByCrashReporter $false `
                                -RecommendedNextTool "editor_reload_get_status" `
                                -RecommendedArgs @{} `
                                -RecommendedAction "The editor process appears alive but coordination is unavailable; inspect status before retrying.")
                        }
                    }
                }

                if ($coldStart -and $null -eq $coldStartEligibility) {
                    $coldStartEligibility = Get-ColdStartEligibility
                }

                if ($coldStart -and -not (Get-ObjectProperty -Object $coldStartEligibility -Name "allowed" -DefaultValue $false)) {
                    $result = [pscustomobject]@{
                            success = $false
                            accepted = $false
                            code = "coordination_unavailable"
                            error = "Cold start was selected but a live editor process was detected."
                            maintenancePath = $maintenancePath
                            sourceFingerprint = $sourceFingerprint
                            upToDateCheck = $upToDateCheck
                            coldStartEligibility = $coldStartEligibility
                            preStartRecovery = $preStartRecoverySummary
                        }
                    return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
                        -CanAutoRecover $false `
                        -BlockedByCrashReporter $false `
                        -RecommendedNextTool "editor_reload_get_status" `
                        -RecommendedArgs @{} `
                        -RecommendedAction "A live editor appeared during cold-start selection; inspect status before retrying.")
                }

                $jobId = [guid]::NewGuid().ToString("N")
                $jobsDir = Join-Path $stateDir "jobs"
                $jobPath = Join-Path $jobsDir "$jobId.json"
                $now = [DateTime]::UtcNow.ToString("o")
                $maintenance = [pscustomobject]@{
                    jobId = $jobId
                    reason = $Reason
                    operation = "rebuild_restart"
                    phase = "queued"
                    requestedAt = $now
                    updatedAt = $now
                    source = "EditorReloadMcp"
                }
                $job = [pscustomobject]@{
                    jobId = $jobId
                    status = "running"
                    phase = "queued"
                    operation = "rebuild_restart"
                    reason = $Reason
                    requestedAt = $now
                    updatedAt = $now
                    force = $Force
                    coldStart = $coldStart
                    coldStartEligibility = $coldStartEligibility
                    preStartRecovery = $preStartRecoverySummary
                    preStartRecoveryActions = @($preStartRecoveryActions.ToArray())
                    editorArgs = $EditorArgs
                    mcpSafeLaunch = $McpSafeLaunch
                    maxParallelActions = $MaxParallelActions
                    sourceFingerprintAtStart = $sourceFingerprint
                    upToDateCheck = $upToDateCheck
                }
                Write-JsonFile -Path $maintenancePath -Value $maintenance
                Write-JsonFile -Path $jobPath -Value $job

                if (-not $coldStart) {
                    try {
                        $null = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_prepare_maintenance" -Params @{ jobId = $jobId; reason = $Reason; operation = "rebuild_restart"; phase = "preparing" } -TimeoutMs 5000)
                    }
                    catch {
                        Write-ReloadLog "coordination_prepare_maintenance failed: $($_.Exception.Message)"
                    }
                }

                $helper = Join-Path (Get-PluginRoot) "Resources\Automation\RebuildAndRestart.ps1"
                $editorArgsJson = "[]"
                if ($EditorArgs.Count -gt 0) {
                    $editorArgsJson = ($EditorArgs | ConvertTo-Json -Compress)
                }
                $arguments = @(
                    "-NoProfile",
                    "-ExecutionPolicy", "Bypass",
                    "-File", $helper,
                    "-ProjectRoot", (Get-ProjectRootFromPlugin),
                    "-JobId", $jobId,
                    "-EditorArgsJson", $editorArgsJson
                )
                if ($MaxParallelActions -gt 0) {
                    $arguments += @("-MaxParallelActions", $MaxParallelActions)
                }
                if ($McpSafeLaunch) {
                    $arguments += "-McpSafeLaunch"
                }
                if ($coldStart) {
                    $arguments += "-ColdStart"
                }
                try {
                    Start-Process -FilePath (Get-PowerShellExe) -ArgumentList (Join-ReloadProcessArguments -Arguments $arguments) -WindowStyle Hidden | Out-Null
                }
                catch {
                    $failed = Update-ReloadJobFile -JobId $jobId -Status "failed" -Phase "failed" -Properties @{ error = $_.Exception.Message }
                    Clear-MaintenanceFile -JobId $jobId
                    return $failed
                }
            }
        }
        finally {
            Exit-ReloadFileLock -Lock $lock
        }

        if ($existingJobId) {
            $waitedJob = Wait-ReloadJob -JobId $existingJobId -TimeoutMs $TimeoutMs
            $waitedStatus = [string] (Get-ObjectProperty -Object $waitedJob -Name "status" -DefaultValue "")
            if ($waitedStatus -eq "timeout") {
                return $waitedJob
            }

            $currentFingerprint = Get-SourceStateFingerprint
            $waitedOperation = [string] (Get-ObjectProperty -Object $waitedJob -Name "operation" -DefaultValue "")
            if ($waitedOperation -eq "rebuild_restart" -and (Test-CompletedJobIncludesSource -Job $waitedJob -CurrentSourceFingerprint $currentFingerprint)) {
                return New-SkippedReloadResult -Operation "rebuild_restart" -Code "already_included_by_existing_job" -Reason "The waited reload job completed from the current source fingerprint." -SourceFingerprint $currentFingerprint -IncludedByJob $waitedJob
            }
            if ($waitedStatus -eq "failed" -and (Test-SourceFingerprintEqual -Left (Get-ObjectProperty -Object $waitedJob -Name "sourceFingerprintAtStart") -Right $currentFingerprint)) {
                return $waitedJob
            }
            continue
        }

        if ($Wait) {
            return Wait-ReloadJob -JobId $jobId -TimeoutMs $TimeoutMs
        }
        return [pscustomobject]@{
            success = $true
            accepted = $true
            jobId = $jobId
            status = "running"
            phase = "queued"
            maintenancePath = $maintenancePath
            jobPath = $jobPath
        }
    }
}

# Runs Live Coding through the coordination gate and clears the sentinel when done.
function Start-HotReload {
    param(
        [string] $Reason = "live coding requested",
        [int] $TimeoutMs = 300000,
        [bool] $Force = $false
    )
    $stateDir = Get-ReloadStateDirectory
    $maintenancePath = Join-Path $stateDir "maintenance.json"
    $lockTimeoutMs = [Math]::Max($TimeoutMs, 10000)
    $jobId = ""
    $sourceFingerprint = $null

    while ($true) {
        $lock = Enter-ReloadFileLock -Name "state.lock" -TimeoutMs $lockTimeoutMs
        $existingJobId = ""
        $liveCodingJoin = $false
        $liveCodingJoinReason = ""
        $liveCodingActiveFromCheck = $false
        try {
            $existing = Read-JsonFile -Path $maintenancePath
            if (Test-TerminalMaintenanceState -Maintenance $existing) {
                return [pscustomobject]@{
                    success = $false
                    accepted = $false
                    code = "terminal_maintenance_requires_recover"
                    maintenancePath = $maintenancePath
                    maintenanceState = $existing
                    recoveryTool = "editor_reload_recover"
                }
            }
            $existingJobId = [string] (Get-ObjectProperty -Object $existing -Name "jobId" -DefaultValue "")
            if ($null -ne $existing -and $existingJobId) {
                # Release the state lock before waiting on a long-running owner job.
            }
            elseif ($null -ne $existing) {
                return [pscustomobject]@{
                    success = $false
                    accepted = $false
                    code = "maintenance_in_progress"
                    operation = (Get-ObjectProperty -Object $existing -Name "operation" -DefaultValue "unknown")
                    maintenancePath = $maintenancePath
                }
            }

            if ($existingJobId) {
                # Release the state lock before waiting on a long-running owner job.
            }
            else {
                $sourceFingerprintBeforeCheck = Get-SourceStateFingerprint
                $lastSuccess = Get-LastSuccessfulReload
                $lastSuccessFingerprint = Get-ObjectProperty -Object $lastSuccess -Name "sourceFingerprint"
                if (-not $Force -and (Test-SourceFingerprintEqual -Left $lastSuccessFingerprint -Right $sourceFingerprintBeforeCheck)) {
                    return New-SkippedReloadResult -Operation "live_coding" -Code "already_loaded" -Reason "Current source fingerprint matches the last successful reload/build." -SourceFingerprint $sourceFingerprintBeforeCheck -LastSuccess $lastSuccess
                }

                $upToDateCheck = $null
                if (-not $Force) {
                    $upToDateCheck = Test-SourceBuildUpToDate -Operation "live_coding" -RequestId ([guid]::NewGuid().ToString("N")) -TimeoutMs $TimeoutMs
                    $sourceFingerprintAfterCheck = Get-SourceStateFingerprint
                    if ((Get-ObjectProperty -Object $upToDateCheck -Name "success" -DefaultValue $false) -and
                        (Get-ObjectProperty -Object $upToDateCheck -Name "upToDate" -DefaultValue $false) -and
                        (Test-SourceFingerprintEqual -Left $sourceFingerprintBeforeCheck -Right $sourceFingerprintAfterCheck)) {
                        return New-SkippedReloadResult -Operation "live_coding" -Code "source_up_to_date" -Reason "UBT reported no outdated editor-target actions." -SourceFingerprint $sourceFingerprintAfterCheck -UpToDateCheck $upToDateCheck
                    }
                    $sourceFingerprint = $sourceFingerprintAfterCheck
                    if ((Get-ObjectProperty -Object $upToDateCheck -Name "code" -DefaultValue "") -eq "live_coding_active") {
                        $liveCodingActiveFromCheck = $true
                    }
                }
                else {
                    $sourceFingerprint = $sourceFingerprintBeforeCheck
                }

                if (-not $liveCodingJoin) {
                    try {
                        $coordinationStatus = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 5000)
                        $liveCodingStatus = Get-ObjectProperty -Object $coordinationStatus -Name "liveCoding"
                        if ($null -ne $liveCodingStatus -and (Get-ObjectProperty -Object $liveCodingStatus -Name "compiling" -DefaultValue $false)) {
                            $liveCodingJoin = $true
                            if ($liveCodingActiveFromCheck) {
                                $liveCodingJoinReason = "live_coding_active"
                            }
                            else {
                                $liveCodingJoinReason = "already_compiling"
                            }
                        }
                    }
                    catch {
                        return [pscustomobject]@{
                            success = $false
                            accepted = $false
                            code = "coordination_unavailable"
                            error = $_.Exception.Message
                            maintenancePath = $maintenancePath
                            sourceFingerprint = $sourceFingerprint
                            upToDateCheck = $upToDateCheck
                        }
                    }
                }

                if (-not $liveCodingJoin) {
                    $jobId = [guid]::NewGuid().ToString("N")
                    $jobsDir = Join-Path $stateDir "jobs"
                    $jobPath = Join-Path $jobsDir "$jobId.json"
                    $now = [DateTime]::UtcNow.ToString("o")
                    $maintenance = [pscustomobject]@{
                        jobId = $jobId
                        reason = $Reason
                        operation = "live_coding"
                        phase = "queued"
                        requestedAt = $now
                        updatedAt = $now
                        source = "EditorReloadMcp"
                        singleFlight = $true
                    }
                    $job = [pscustomobject]@{
                        jobId = $jobId
                        status = "running"
                        phase = "queued"
                        operation = "live_coding"
                        reason = $Reason
                        requestedAt = $now
                        updatedAt = $now
                        force = $Force
                        singleFlight = $true
                        sourceFingerprintAtStart = $sourceFingerprint
                        upToDateCheck = $upToDateCheck
                    }
                    Write-JsonFile -Path $maintenancePath -Value $maintenance
                    Write-JsonFile -Path $jobPath -Value $job
                }
            }
        }
        finally {
            Exit-ReloadFileLock -Lock $lock
        }

        if ($existingJobId) {
            $waitedJob = Wait-ReloadJob -JobId $existingJobId -TimeoutMs $TimeoutMs
            $waitedStatus = [string] (Get-ObjectProperty -Object $waitedJob -Name "status" -DefaultValue "")
            if ($waitedStatus -eq "timeout") {
                return $waitedJob
            }

            $currentFingerprint = Get-SourceStateFingerprint
            if (Test-CompletedJobIncludesSource -Job $waitedJob -CurrentSourceFingerprint $currentFingerprint) {
                return New-SkippedReloadResult -Operation "live_coding" -Code "already_included_by_existing_job" -Reason "The waited reload job completed from the current source fingerprint." -SourceFingerprint $currentFingerprint -IncludedByJob $waitedJob
            }
            if ($waitedStatus -eq "failed" -and (Test-SourceFingerprintEqual -Left (Get-ObjectProperty -Object $waitedJob -Name "sourceFingerprintAtStart") -Right $currentFingerprint)) {
                return $waitedJob
            }
            continue
        }

        if ($liveCodingJoin) {
            $joinCheck = Wait-LiveCodingJoinAndCheck -Operation "live_coding_join_recheck" -TimeoutMs $TimeoutMs
            if (-not (Get-ObjectProperty -Object $joinCheck -Name "success" -DefaultValue $false)) {
                return [pscustomobject]@{
                    success = $false
                    accepted = $false
                    code = (Get-ObjectProperty -Object $joinCheck -Name "code" -DefaultValue "live_coding_wait_failed")
                    status = (Get-ObjectProperty -Object $joinCheck -Name "status" -DefaultValue "failed")
                    reason = "Timed out or failed while joining an existing Live Coding compile."
                    joinReason = $liveCodingJoinReason
                    joinCheck = $joinCheck
                }
            }

            $currentFingerprint = Get-ObjectProperty -Object $joinCheck -Name "sourceFingerprintAfterCheck"
            if (Get-ObjectProperty -Object $joinCheck -Name "upToDate" -DefaultValue $false) {
                return New-SkippedReloadResult -Operation "live_coding" -Code "already_included_by_existing_compile" -Reason "The active Live Coding compile finished and UBT reported the current source up to date." -SourceFingerprint $currentFingerprint -UpToDateCheck (Get-ObjectProperty -Object $joinCheck -Name "upToDateCheck") -IncludedByJob $joinCheck
            }
            continue
        }

        break
    }

    try {
        $null = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_prepare_maintenance" -Params @{ jobId = $jobId; reason = $Reason; operation = "live_coding"; phase = "live_coding" } -TimeoutMs 5000)
        Update-MaintenanceFile -JobId $jobId -Reason $Reason -Operation "live_coding" -Phase "live_coding"
        $null = Update-ReloadJobFile -JobId $jobId -Status "running" -Phase "live_coding"

        $compileJoinAttempts = 0
        while ($true) {
            if ($compileJoinAttempts -gt 0) {
                Update-MaintenanceFile -JobId $jobId -Reason $Reason -Operation "live_coding" -Phase "live_coding"
                $null = Update-ReloadJobFile -JobId $jobId -Status "running" -Phase "live_coding"
            }
            $compile = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_live_coding_compile" -Params @{ wait = $true } -TimeoutMs $TimeoutMs)
            $compileResult = [string] (Get-ObjectProperty -Object $compile -Name "result" -DefaultValue "unknown")
            if ($compileResult -eq "success" -or $compileResult -eq "no_changes") {
                $completed = Update-ReloadJobFile -JobId $jobId -Status "completed" -Phase "completed" -Properties @{ compile = $compile; sourceFingerprintAtCompletion = (Get-SourceStateFingerprint) }
                Set-LastSuccessfulReload -JobId $jobId -Operation "live_coding" -SourceFingerprint $sourceFingerprint
                Clear-MaintenanceFile -JobId $jobId
                return $completed
            }

            if ($compileResult -eq "already_compiling" -or $compileResult -eq "in_progress") {
                $compileJoinAttempts++
                if ($compileJoinAttempts -gt 3) {
                    throw "Live Coding remained active after $compileJoinAttempts join attempts."
                }

                Update-MaintenanceFile -JobId $jobId -Reason $Reason -Operation "live_coding" -Phase "joining_live_coding"
                $null = Update-ReloadJobFile -JobId $jobId -Status "running" -Phase "joining_live_coding" -Properties @{
                    compile = $compile
                    joinReason = $compileResult
                    sourceFingerprintBeforeJoin = (Get-SourceStateFingerprint)
                }
                $joinCheck = Wait-LiveCodingJoinAndCheck -Operation "live_coding_compile_join_recheck" -TimeoutMs $TimeoutMs
                $null = Update-ReloadJobFile -JobId $jobId -Status "running" -Phase "joining_live_coding" -Properties @{
                    compile = $compile
                    joinReason = $compileResult
                    sourceFingerprintBeforeJoin = (Get-ObjectProperty -Object $joinCheck -Name "sourceFingerprintBeforeJoin")
                    sourceFingerprintAfterJoin = (Get-ObjectProperty -Object $joinCheck -Name "sourceFingerprintAfterJoin")
                    upToDateCheckAfterJoin = (Get-ObjectProperty -Object $joinCheck -Name "upToDateCheck")
                }
                if (-not (Get-ObjectProperty -Object $joinCheck -Name "success" -DefaultValue $false)) {
                    throw "Live Coding join failed with $([string] (Get-ObjectProperty -Object $joinCheck -Name "code" -DefaultValue "unknown"))."
                }
                if (Get-ObjectProperty -Object $joinCheck -Name "upToDate" -DefaultValue $false) {
                    $sourceFingerprintAfterCheck = Get-ObjectProperty -Object $joinCheck -Name "sourceFingerprintAfterCheck"
                    $completed = Update-ReloadJobFile -JobId $jobId -Status "completed" -Phase "completed" -Properties @{
                        compile = $compile
                        joinCheck = $joinCheck
                        joinReason = $compileResult
                        sourceFingerprintAtCompletion = $sourceFingerprintAfterCheck
                    }
                    Set-LastSuccessfulReload -JobId $jobId -Operation "live_coding" -SourceFingerprint $sourceFingerprintAfterCheck
                    Clear-MaintenanceFile -JobId $jobId
                    return $completed
                }
                continue
            }

            throw "Live Coding result was $compileResult."
        }
    }
    catch {
        if ($jobId) {
            $failed = Update-ReloadJobFile -JobId $jobId -Status "failed" -Phase "failed" -Properties @{ error = $_.Exception.Message; sourceFingerprintAtFailure = (Get-SourceStateFingerprint) }
            Clear-MaintenanceFile -JobId $jobId
            return $failed
        }
        Clear-MaintenanceFile
        return [pscustomobject]@{
            success = $false
            accepted = $false
            code = "hot_reload_failed"
            error = $_.Exception.Message
        }
    }
}

# Returns the PowerShell executable used to start helper scripts.
function Get-PowerShellExe {
    if ($PSVersionTable.PSEdition -eq "Core") {
        return [System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
    }
    return "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
}

# Waits for a reload job to reach a terminal state.
function Wait-ReloadJob {
    param(
        [string] $JobId,
        [int] $TimeoutMs = 60000
    )
    $jobPath = Join-Path (Join-Path (Get-ReloadStateDirectory) "jobs") "$JobId.json"
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ($true) {
        $job = Read-JsonFile -Path $jobPath
        $status = Get-ObjectProperty -Object $job -Name "status"
        if ($null -ne $job -and ($status -eq "completed" -or $status -eq "failed")) {
            return $job
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            return [pscustomobject]@{
                success = $false
                jobId = $JobId
                status = "timeout"
                jobPath = $jobPath
            }
        }
        Start-Sleep -Milliseconds 1000
    }
}

# Performs conservative recovery checks without unsafe editor termination.
function Invoke-ReloadRecover {
    param(
        [string] $Reason = "recover requested",
        [bool] $AllowUnsafeRestart = $false
    )
    $status = Get-ReloadStatus
    $actions = New-Object System.Collections.Generic.List[string]
    $pendingCrashReports = @(Get-ObjectProperty -Object $status -Name "pendingCrashReports" -DefaultValue @())
    if ($pendingCrashReports.Count -gt 0) {
        $result = [pscustomobject]@{
            success = $false
            code = "crash_report_pending"
            reason = $Reason
            error = "Close the Unreal Crash Report window before launching another editor."
            allowUnsafeRestart = $AllowUnsafeRestart
            manualAction = "Close CrashReportClientEditor or submit/dismiss the Crash Reporter dialog, then retry editor_reload_recover."
            status = $status
        }
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $true `
            -RecommendedNextTool "editor_reload_recover" `
            -RecommendedArgs @{ reason = "retry after closing Unreal Crash Reporter" } `
            -RecommendedAction "Close or submit the Unreal Crash Reporter dialog, then retry recovery.")
    }

    $maintenancePath = Get-ObjectProperty -Object $status -Name "maintenancePath"
    $maintenanceState = Get-ObjectProperty -Object $status -Name "maintenanceState"
    if (Get-ObjectProperty -Object $status -Name "maintenance" -DefaultValue $false) {
        if (Test-TerminalMaintenanceState -Maintenance $maintenanceState) {
            Remove-Item -LiteralPath $maintenancePath -Force
            $actions.Add("cleared_terminal_maintenance")
        }
        else {
            $result = [pscustomobject]@{
                success = $false
                code = "maintenance_in_progress"
                reason = $Reason
                status = $status
            }
            return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
                -CanAutoRecover $false `
                -BlockedByCrashReporter $false `
                -RecommendedNextTool "editor_reload_wait_for_job" `
                -RecommendedArgs @{ jobId = [string] (Get-ObjectProperty -Object $maintenanceState -Name "jobId" -DefaultValue "") } `
                -RecommendedAction "Wait for the active reload job instead of starting another recovery.")
        }
    }
    $portState = Get-ObjectProperty -Object $status -Name "portState"
    $portPath = Get-ObjectProperty -Object $portState -Name "path"
    if ((Get-ObjectProperty -Object $portState -Name "exists" -DefaultValue $false) -and
        -not (Get-ObjectProperty -Object $portState -Name "processAlive" -DefaultValue $false)) {
        Remove-Item -LiteralPath $portPath -Force
        $actions.Add("cleared_stale_port")
    }

    if ($actions.Count -gt 0) {
        $result = [pscustomobject]@{
            success = $true
            code = "recovered"
            reason = $Reason
            actions = @($actions)
            before = $status
            after = (Get-ReloadStatus)
        }
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $true `
            -BlockedByCrashReporter $false `
            -RecommendedNextTool "editor_reload_recover_and_restart" `
            -RecommendedArgs @{ wait = $true; reason = "restart after reload recovery" } `
            -RecommendedAction "Run recover-and-restart to rebuild and relaunch the editor.")
    }

    $bridgeError = Get-ObjectProperty -Object $status -Name "bridgeError"
    if ((Get-ObjectProperty -Object $portState -Name "exists" -DefaultValue $false) -and
        (Get-ObjectProperty -Object $portState -Name "processAlive" -DefaultValue $false) -and
        $bridgeError) {
        $result = [pscustomobject]@{
            success = $false
            code = "bridge_unreachable_editor_alive"
            error = "Editor process is alive but bridge is unreachable; unsafe restart is disabled."
            allowUnsafeRestart = $AllowUnsafeRestart
            status = $status
        }
        return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice `
            -CanAutoRecover $false `
            -BlockedByCrashReporter $false `
            -RecommendedNextTool "editor_reload_get_status" `
            -RecommendedArgs @{} `
            -RecommendedAction "The editor process is alive; inspect status or close the editor before retrying recovery.")
    }
    $result = [pscustomobject]@{
        success = $true
        code = "no_recovery_needed"
        reason = $Reason
        status = $status
    }
    return Add-ReloadActionAdvice -Result $result -Advice (New-ReloadActionAdvice)
}

# Runs recovery first, then starts the normal rebuild/restart flow.
function Invoke-ReloadRecoverAndRestart {
    param(
        [string] $Reason = "recover and restart requested",
        [bool] $Wait = $false,
        [int] $TimeoutMs = 60000,
        [bool] $Force = $false,
        [string[]] $EditorArgs = @(),
        [int] $MaxParallelActions = 0,
        [bool] $McpSafeLaunch = $false
    )
    $recovery = Invoke-ReloadRecover -Reason "explicit recovery before restart"
    if (-not [bool] (Get-ObjectProperty -Object $recovery -Name "success" -DefaultValue $false)) {
        return $recovery
    }

    $result = Start-RebuildAndRestart `
        -Reason $Reason `
        -Wait $Wait `
        -TimeoutMs $TimeoutMs `
        -Force $Force `
        -EditorArgs $EditorArgs `
        -MaxParallelActions $MaxParallelActions `
        -McpSafeLaunch $McpSafeLaunch

    if ($null -ne $result) {
        $result | Add-Member -NotePropertyName explicitRecovery -NotePropertyValue (New-ReloadRecoverySummary -Recovery $recovery) -Force
    }
    return $result
}

# Converts internal data into an MCP tool result.
function New-ToolResult {
    param(
        [object] $Data,
        [bool] $IsError = $false
    )
    return @{
        content = @(
            @{
                type = "text"
                text = ($Data | ConvertTo-Json -Depth 50)
            }
        )
        isError = $IsError
    }
}

# Returns the tool metadata exposed by this MCP server.
function Get-ToolList {
    return @(
        @{
            name = "editor_reload_get_status"
            description = "Report Unreal Editor reload coordination status."
            inputSchema = @{ type = "object"; properties = @{} }
        },
        @{
            name = "editor_reload_check_up_to_date"
            description = "Check whether editor C++ build inputs are already up to date without compiling."
            inputSchema = @{
                type = "object"
                properties = @{
                    operation = @{ type = "string" }
                    timeoutMs = @{ type = "number" }
                }
            }
        },
        @{
            name = "editor_reload_rebuild_and_restart"
            description = "Safely recover stale crash state when possible, drain MCP work, close the editor, rebuild, and restart it."
            inputSchema = @{
                type = "object"
                properties = @{
                    reason = @{ type = "string" }
                    wait = @{ type = "boolean" }
                    timeoutMs = @{ type = "number" }
                    force = @{ type = "boolean" }
                    editorArgs = @{ type = "array"; items = @{ type = "string" } }
                    maxParallelActions = @{ type = "number" }
                    mcpSafeLaunch = @{ type = "boolean" }
                }
            }
        },
        @{
            name = "editor_reload_recover_and_restart"
            description = "Run reload recovery first, then rebuild and restart the editor using cold start when the editor crashed or is closed."
            inputSchema = @{
                type = "object"
                properties = @{
                    reason = @{ type = "string" }
                    wait = @{ type = "boolean" }
                    timeoutMs = @{ type = "number" }
                    force = @{ type = "boolean" }
                    editorArgs = @{ type = "array"; items = @{ type = "string" } }
                    maxParallelActions = @{ type = "number" }
                    mcpSafeLaunch = @{ type = "boolean" }
                }
            }
        },
        @{
            name = "editor_reload_hot_reload"
            description = "Run coordinated Live Coding while blocking other editor-backed MCP calls."
            inputSchema = @{
                type = "object"
                properties = @{
                    reason = @{ type = "string" }
                    timeoutMs = @{ type = "number" }
                    force = @{ type = "boolean" }
                }
            }
        },
        @{
            name = "editor_reload_wait_for_job"
            description = "Wait for an editor reload job to complete or fail."
            inputSchema = @{
                type = "object"
                required = @("jobId")
                properties = @{
                    jobId = @{ type = "string" }
                    timeoutMs = @{ type = "number" }
                }
            }
        },
        @{
            name = "editor_reload_recover"
            description = "Inspect reload recovery state without unsafe editor termination."
            inputSchema = @{
                type = "object"
                properties = @{
                    reason = @{ type = "string" }
                    allowUnsafeRestart = @{ type = "boolean" }
                }
            }
        }
    )
}

# Dispatches one MCP tools/call request.
function Invoke-ToolCall {
    param(
        [string] $Name,
        [object] $Arguments
    )
    if ($null -eq $Arguments) {
        $Arguments = [pscustomobject]@{}
    }

    switch ($Name) {
        "editor_reload_get_status" {
            return New-ToolResult -Data (Get-ReloadStatus)
        }
        "editor_reload_check_up_to_date" {
            return New-ToolResult -Data (Get-ReloadUpToDateStatus `
                -Operation ([string] (Get-ObjectProperty -Object $Arguments -Name "operation" -DefaultValue "manual_check")) `
                -TimeoutMs ([int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue 60000)))
        }
        "editor_reload_rebuild_and_restart" {
            return New-ToolResult -Data (Start-RebuildAndRestart `
                -Reason ([string] (Get-ObjectProperty -Object $Arguments -Name "reason" -DefaultValue "rebuild requested")) `
                -Wait ([bool] (Get-ObjectProperty -Object $Arguments -Name "wait" -DefaultValue $false)) `
                -TimeoutMs ([int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue 60000)) `
                -Force ([bool] (Get-ObjectProperty -Object $Arguments -Name "force" -DefaultValue $false)) `
                -EditorArgs @((Get-ObjectProperty -Object $Arguments -Name "editorArgs" -DefaultValue @())) `
                -MaxParallelActions ([int] (Get-ObjectProperty -Object $Arguments -Name "maxParallelActions" -DefaultValue 0)) `
                -McpSafeLaunch ([bool] (Get-ObjectProperty -Object $Arguments -Name "mcpSafeLaunch" -DefaultValue $false)))
        }
        "editor_reload_recover_and_restart" {
            return New-ToolResult -Data (Invoke-ReloadRecoverAndRestart `
                -Reason ([string] (Get-ObjectProperty -Object $Arguments -Name "reason" -DefaultValue "recover and restart requested")) `
                -Wait ([bool] (Get-ObjectProperty -Object $Arguments -Name "wait" -DefaultValue $false)) `
                -TimeoutMs ([int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue 60000)) `
                -Force ([bool] (Get-ObjectProperty -Object $Arguments -Name "force" -DefaultValue $false)) `
                -EditorArgs @((Get-ObjectProperty -Object $Arguments -Name "editorArgs" -DefaultValue @())) `
                -MaxParallelActions ([int] (Get-ObjectProperty -Object $Arguments -Name "maxParallelActions" -DefaultValue 0)) `
                -McpSafeLaunch ([bool] (Get-ObjectProperty -Object $Arguments -Name "mcpSafeLaunch" -DefaultValue $false)))
        }
        "editor_reload_hot_reload" {
            return New-ToolResult -Data (Start-HotReload `
                -Reason ([string] (Get-ObjectProperty -Object $Arguments -Name "reason" -DefaultValue "live coding requested")) `
                -TimeoutMs ([int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue 300000)) `
                -Force ([bool] (Get-ObjectProperty -Object $Arguments -Name "force" -DefaultValue $false)))
        }
        "editor_reload_wait_for_job" {
            $jobId = Get-ObjectProperty -Object $Arguments -Name "jobId"
            if (-not $jobId) {
                return New-ToolResult -IsError $true -Data @{ success = $false; error = "jobId is required" }
            }
            return New-ToolResult -Data (Wait-ReloadJob -JobId ([string] $jobId) -TimeoutMs ([int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue 60000)))
        }
        "editor_reload_recover" {
            return New-ToolResult -Data (Invoke-ReloadRecover `
                -Reason ([string] (Get-ObjectProperty -Object $Arguments -Name "reason" -DefaultValue "recover requested")) `
                -AllowUnsafeRestart ([bool] (Get-ObjectProperty -Object $Arguments -Name "allowUnsafeRestart" -DefaultValue $false)))
        }
        default {
            return New-ToolResult -IsError $true -Data @{ success = $false; error = "Unknown tool: $Name" }
        }
    }
}

# Writes one JSON-RPC response to stdout.
function Write-McpResponse {
    param([object] $Response)
    $script:Stdout.WriteLine(($Response | ConvertTo-Json -Depth 50 -Compress))
}

# Creates a JSON-RPC error response object.
function New-McpError {
    param($Id, [int] $Code, [string] $Message)
    return @{ jsonrpc = "2.0"; id = $Id; error = @{ code = $Code; message = $Message } }
}

# Processes one JSON-RPC request from the MCP client.
function Invoke-McpRequest {
    param($Request)
    $id = Get-ObjectProperty -Object $Request -Name "id"
    $method = [string] (Get-ObjectProperty -Object $Request -Name "method")
    switch ($method) {
        "initialize" {
            return @{
                jsonrpc = "2.0"
                id = $id
                result = @{
                    protocolVersion = "2025-03-26"
                    capabilities = @{ tools = @{} }
                    serverInfo = @{ name = "odiro-editor-reload"; version = "0.1.0" }
                }
            }
        }
        "notifications/initialized" {
            return $null
        }
        "tools/list" {
            return @{ jsonrpc = "2.0"; id = $id; result = @{ tools = Get-ToolList } }
        }
        "tools/call" {
            $params = Get-ObjectProperty -Object $Request -Name "params"
            $result = Invoke-ToolCall `
                -Name ([string] (Get-ObjectProperty -Object $params -Name "name")) `
                -Arguments (Get-ObjectProperty -Object $params -Name "arguments")
            return @{ jsonrpc = "2.0"; id = $id; result = $result }
        }
        default {
            if ($null -eq $id) {
                return $null
            }
            return New-McpError -Id $id -Code -32601 -Message "Unknown method: $method"
        }
    }
}

while (($line = [Console]::In.ReadLine()) -ne $null) {
    try {
        $request = $line | ConvertFrom-Json
        $response = Invoke-McpRequest -Request $request
        if ($null -ne $response) {
            Write-McpResponse -Response $response
        }
    }
    catch {
        Write-ReloadLog $_.Exception.Message
        Write-McpResponse -Response (New-McpError -Id $null -Code -32603 -Message $_.Exception.Message)
    }
}
