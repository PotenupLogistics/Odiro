# Stdio MCP proxy for the editor-owned UE_MCP_Bridge WebSocket server.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:Stdout = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $script:Utf8NoBom)
$script:Stdout.AutoFlush = $true
$script:DefaultBridgeTimeoutMs = 120000
[Console]::InputEncoding = $script:Utf8NoBom
[Console]::OutputEncoding = $script:Utf8NoBom

# Writes diagnostics away from the MCP stdout protocol stream.
function Write-BridgeMcpLog {
    param([string] $Message)
    [Console]::Error.WriteLine("[ue-bridge-mcp] $Message")
}

# Returns the UE_MCP_Bridge plugin root from this script location.
function Get-PluginRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
}

# Returns the Unreal project root that owns the plugin and .uproject file.
function Get-ProjectRootFromPlugin {
    return (Resolve-Path -LiteralPath (Join-Path (Get-PluginRoot) "..\..")).Path
}

. (Join-Path (Get-PluginRoot) "Resources\Automation\CrashReportTools.ps1")

# Returns the shared runtime state directory written by the editor bridge.
function Get-BridgeStateDirectory {
    return (Join-Path (Get-ProjectRootFromPlugin) "Saved\UE_MCP_Bridge")
}

# Reads a JSON file into a PowerShell object, returning null on missing files.
function Read-JsonFile {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
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

# Returns whether a maintenance phase is terminal and should not gate bridge calls.
function Test-TerminalPhase {
    param([string] $Phase)
    return @("completed", "failed", "restart_failed", "port_timeout", "editor_crashed", "modal_blocked", "crash_report_pending") -contains $Phase
}

# Returns the current bridge port state advertised by the editor.
function Get-PortState {
    $path = Join-Path (Get-BridgeStateDirectory) "port.json"
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

# Sends one JSON-RPC request to the editor's WebSocket bridge.
function Invoke-BridgeMethod {
    param(
        [string] $Method,
        [object] $Params = @{},
        [int] $TimeoutMs = $script:DefaultBridgeTimeoutMs
    )
    $portState = Get-PortState
    $port = Get-ObjectProperty -Object $portState -Name "port"
    if (-not (Get-ObjectProperty -Object $portState -Name "exists" -DefaultValue $false) -or -not $port) {
        $pendingCrashReports = @(Get-PendingUnrealCrashReportClients)
        if ($pendingCrashReports.Count -gt 0) {
            throw "UE_MCP_Bridge port.json is missing and Unreal Crash Report is pending; close the Crash Report window before launching another editor."
        }
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

        $buffer = [byte[]]::new(16384)
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
            Write-BridgeMcpLog "Failed to abort bridge WebSocket: $($_.Exception.Message)"
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
    return (Get-ObjectProperty -Object $Response -Name "result")
}

# Discovers registered editor handler names from plugin source for stable tools/list output.
function Get-SourceHandlerNames {
    $sourceRoot = Join-Path (Get-PluginRoot) "Source"
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
        return @()
    }

    $names = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::Ordinal)
    Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include "*.cpp", "*.h" |
        Select-String -Pattern 'Register(?:External)?Handler(?:WithTimeout)?\(\s*TEXT\("([^"]+)"\)' |
        ForEach-Object {
            foreach ($match in $_.Matches) {
                [void] $names.Add($match.Groups[1].Value)
            }
        }

    return @($names | Sort-Object)
}

# Returns MCP tool metadata for generic bridge utilities and each discovered handler.
function Get-ToolList {
    $tools = New-Object System.Collections.Generic.List[object]
    $tools.Add(@{
        name = "ue_bridge_get_status"
        description = "Report UE_MCP_Bridge port, process, and editor coordination status."
        inputSchema = @{ type = "object"; properties = @{} }
    })
    $tools.Add(@{
        name = "ue_bridge_call"
        description = "Call a UE_MCP_Bridge editor method by name with arbitrary JSON parameters."
        inputSchema = @{
            type = "object"
            required = @("method")
            properties = @{
                method = @{ type = "string" }
                params = @{ type = "object" }
                timeoutMs = @{ type = "number" }
            }
        }
    })

    foreach ($name in Get-SourceHandlerNames) {
        if ($name -eq "ue_bridge_call" -or $name -eq "ue_bridge_get_status") {
            continue
        }
        $tools.Add(@{
            name = $name
            description = "UE_MCP_Bridge editor handler '$name'. Parameters are forwarded as JSON."
            inputSchema = @{ type = "object"; properties = @{}; additionalProperties = $true }
        })
    }
    return $tools
}

# Returns bridge status without mutating editor state.
function Get-BridgeStatus {
    $stateDir = Get-BridgeStateDirectory
    $maintenancePath = Join-Path $stateDir "maintenance.json"
    $maintenance = Read-JsonFile -Path $maintenancePath
    $maintenancePhase = [string] (Get-ObjectProperty -Object $maintenance -Name "phase" -DefaultValue "")
    $portState = Get-PortState
    $crashReportClients = @(Get-UnrealCrashReportClientStates)
    $pendingCrashReports = @($crashReportClients | Where-Object { $_.blocksEditorLaunch })
    $bridgeStatus = $null
    $bridgeError = $null
    try {
        $bridgeStatus = Get-BridgeResult -Response (Invoke-BridgeMethod -Method "coordination_get_status" -TimeoutMs 3000)
    }
    catch {
        $bridgeError = $_.Exception.Message
    }
    if ($bridgeStatus) {
        $portState | Add-Member -NotePropertyName classification -NotePropertyValue "ready" -Force
    }
    elseif ((Get-ObjectProperty -Object $portState -Name "exists" -DefaultValue $false) -and
        (Get-ObjectProperty -Object $portState -Name "processAlive" -DefaultValue $false)) {
        $portState | Add-Member -NotePropertyName classification -NotePropertyValue "process_alive_but_bridge_down" -Force
    }

    return [pscustomobject]@{
        success = $true
        stateDirectory = $stateDir
        maintenancePath = $maintenancePath
        maintenanceState = $maintenance
        maintenanceTerminal = (Test-TerminalPhase -Phase $maintenancePhase)
        portState = $portState
        bridgeStatus = $bridgeStatus
        bridgeError = $bridgeError
        crashReportClients = $crashReportClients
        pendingCrashReports = $pendingCrashReports
        toolCount = (Get-SourceHandlerNames).Count + 2
    }
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

# Dispatches one MCP tools/call request.
function Invoke-ToolCall {
    param(
        [string] $Name,
        [object] $Arguments
    )
    if ($null -eq $Arguments) {
        $Arguments = [pscustomobject]@{}
    }

    try {
        if ($Name -eq "ue_bridge_get_status") {
            return New-ToolResult -Data (Get-BridgeStatus)
        }
        if ($Name -eq "ue_bridge_call") {
            $method = [string] (Get-ObjectProperty -Object $Arguments -Name "method")
            if (-not $method) {
                return New-ToolResult -IsError $true -Data @{ success = $false; error = "method is required" }
            }
            $params = Get-ObjectProperty -Object $Arguments -Name "params" -DefaultValue @{}
            $timeoutMs = [int] (Get-ObjectProperty -Object $Arguments -Name "timeoutMs" -DefaultValue $script:DefaultBridgeTimeoutMs)
            return New-ToolResult -Data (Get-BridgeResult -Response (Invoke-BridgeMethod -Method $method -Params $params -TimeoutMs $timeoutMs))
        }

        return New-ToolResult -Data (Get-BridgeResult -Response (Invoke-BridgeMethod -Method $Name -Params $Arguments))
    }
    catch {
        return New-ToolResult -IsError $true -Data @{ success = $false; error = $_.Exception.Message; tool = $Name }
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
                    serverInfo = @{ name = "odiro-ue-bridge"; version = "0.1.0" }
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
        Write-BridgeMcpLog $_.Exception.Message
        Write-McpResponse -Response (New-McpError -Id $null -Code -32603 -Message $_.Exception.Message)
    }
}
