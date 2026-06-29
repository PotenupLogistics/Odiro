# Installs repository MCP server entries for supported local coding agents.
param(
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "agent-mcp"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$repoRoot = Get-RepoRoot
$bridgeMcp = Join-Path $repoRoot "Client\Plugins\UE_MCP_Bridge\Resources\MCP\BridgeMcp.ps1"
$reloadMcp = Join-Path $repoRoot "Client\Plugins\UE_MCP_Bridge\Resources\MCP\EditorReloadMcp.ps1"
$codexHome = if ($env:CODEX_HOME) { $env:CODEX_HOME } else { Join-Path $env:USERPROFILE ".codex" }

# Returns whether a command can be found on PATH.
function Test-CommandAvailable {
    param([string] $Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

# Returns whether a known config or install path exists.
function Test-AnyPath {
    param([string[]] $Paths)
    foreach ($path in $Paths) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            return $true
        }
    }
    return $false
}

# Returns whether a JSON value is an object that can own named properties.
function Test-JsonObject {
    param([object] $Value)
    return $null -ne $Value -and
        -not ($Value -is [array]) -and
        -not ($Value -is [string]) -and
        -not ($Value -is [ValueType])
}

# Returns a JSON object from disk, or an empty object when the config is missing.
function Read-JsonConfig {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{}
    }
    try {
        $config = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        if (-not (Test-JsonObject -Value $config)) {
            throw "JSON config root must be an object: $Path"
        }
        return $config
    }
    catch {
        throw "Unable to read JSON config '$Path': $($_.Exception.Message)"
    }
}

# Writes a UTF-8 JSON file after ensuring the parent directory exists.
function Write-JsonConfig {
    param(
        [string] $Path,
        [object] $Value
    )
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    $json = $Value | ConvertTo-Json -Depth 20
    if ($DryRun) {
        Write-Step "Would write $Path"
        return
    }
    [System.IO.File]::WriteAllText($Path, ($json + "`n"), [System.Text.UTF8Encoding]::new($false))
}

# Adds or replaces a note property on a PowerShell JSON object.
function Set-JsonProperty {
    param(
        [object] $Object,
        [string] $Name,
        [object] $Value
    )
    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

# Returns a JSON object property value without tripping StrictMode on missing fields.
function Get-JsonPropertyValue {
    param(
        [object] $Object,
        [string] $Name
    )
    if ($null -eq $Object) {
        return $null
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($property) {
        return $property.Value
    }
    return $null
}

# Returns a child JSON object, creating it when missing.
function Get-OrCreateJsonObjectProperty {
    param(
        [object] $Object,
        [string] $Name
    )
    $value = Get-JsonPropertyValue -Object $Object -Name $Name
    if ($null -eq $value) {
        $value = [pscustomobject]@{}
        Set-JsonProperty -Object $Object -Name $Name -Value $value
        return $value
    }
    if (-not (Test-JsonObject -Value $value)) {
        throw "JSON property '$Name' must be an object."
    }
    return $value
}

# Returns a stdio MCP server object for mcpServers-style configs.
function New-McpServersEntry {
    param([string] $ScriptPath)
    return [ordered]@{
        command = "powershell.exe"
        args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    }
}

# Returns a stdio MCP server object for VS Code/Copilot workspace configs.
function New-VsCodeServerEntry {
    param([string] $ScriptPath)
    return [ordered]@{
        type = "stdio"
        command = "powershell.exe"
        args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    }
}

# Installs .mcp.json for Claude Code project scope.
function Install-ClaudeCodeMcp {
    $path = Join-Path $repoRoot ".mcp.json"
    $config = Read-JsonConfig -Path $path
    $servers = Get-OrCreateJsonObjectProperty -Object $config -Name "mcpServers"
    Set-JsonProperty -Object $servers -Name "odiro_ue_bridge" -Value (New-McpServersEntry -ScriptPath $bridgeMcp)
    Set-JsonProperty -Object $servers -Name "odiro_editor_reload" -Value (New-McpServersEntry -ScriptPath $reloadMcp)
    Write-JsonConfig -Path $path -Value $config
    if (-not $DryRun) {
        Write-Success "Claude Code MCP config installed."
    }
}

# Installs .cursor/mcp.json for Cursor project scope.
function Install-CursorMcp {
    $path = Join-Path $repoRoot ".cursor\mcp.json"
    $config = Read-JsonConfig -Path $path
    $servers = Get-OrCreateJsonObjectProperty -Object $config -Name "mcpServers"
    Set-JsonProperty -Object $servers -Name "odiro_ue_bridge" -Value (New-McpServersEntry -ScriptPath $bridgeMcp)
    Set-JsonProperty -Object $servers -Name "odiro_editor_reload" -Value (New-McpServersEntry -ScriptPath $reloadMcp)
    Write-JsonConfig -Path $path -Value $config
    if (-not $DryRun) {
        Write-Success "Cursor MCP config installed."
    }
}

# Installs .vscode/mcp.json for VS Code Copilot workspace scope.
function Install-VsCodeCopilotMcp {
    $path = Join-Path $repoRoot ".vscode\mcp.json"
    $config = Read-JsonConfig -Path $path
    $servers = Get-OrCreateJsonObjectProperty -Object $config -Name "servers"
    Set-JsonProperty -Object $servers -Name "odiro_ue_bridge" -Value (New-VsCodeServerEntry -ScriptPath $bridgeMcp)
    Set-JsonProperty -Object $servers -Name "odiro_editor_reload" -Value (New-VsCodeServerEntry -ScriptPath $reloadMcp)
    Write-JsonConfig -Path $path -Value $config
    if (-not $DryRun) {
        Write-Success "VS Code Copilot MCP config installed."
    }
}

# Converts a string to a TOML basic string.
function ConvertTo-TomlString {
    param([string] $Value)
    return '"' + $Value.Replace('\', '\\').Replace('"', '\"') + '"'
}

# Removes generated Codex MCP server blocks from an existing config.
function Remove-CodexMcpBlocks {
    param([string] $Content)
    $lines = @($Content -split "`r?`n")
    $output = New-Object System.Collections.Generic.List[string]
    $skip = $false
    foreach ($line in $lines) {
        if ($line -match '^\[mcp_servers\.odiro_(ue_bridge|editor_reload)(\..*)?\]\s*$') {
            $skip = $true
            continue
        }
        if ($skip -and $line -match '^\[') {
            $skip = $false
        }
        if (-not $skip) {
            $output.Add($line)
        }
    }
    return (($output -join "`n").TrimEnd() + "`n")
}

# Installs the user-scope Codex config because Codex loads MCP servers from CODEX_HOME.
function Install-CodexMcp {
    $path = Join-Path $codexHome "config.toml"
    $content = ""
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        $content = Get-Content -LiteralPath $path -Raw
    }
    $content = Remove-CodexMcpBlocks -Content $content
    $append = @"
[mcp_servers.odiro_ue_bridge]
command = "powershell.exe"
args = ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $(ConvertTo-TomlString $bridgeMcp)]
startup_timeout_sec = 120

[mcp_servers.odiro_editor_reload]
command = "powershell.exe"
args = ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $(ConvertTo-TomlString $reloadMcp)]
startup_timeout_sec = 120
"@
    $parent = Split-Path -Parent $path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    if ($DryRun) {
        Write-Step "Would write $path"
        return
    }
    $baseContent = $content.TrimEnd()
    $newContent = if ($baseContent) { $baseContent + "`n`n" + $append + "`n" } else { $append + "`n" }
    [System.IO.File]::WriteAllText($path, $newContent, [System.Text.UTF8Encoding]::new($false))
    Write-Success "Codex MCP config installed."
}

if (-not (Test-Path -LiteralPath $bridgeMcp -PathType Leaf)) {
    throw "Bridge MCP script not found: $bridgeMcp"
}
if (-not (Test-Path -LiteralPath $reloadMcp -PathType Leaf)) {
    throw "Reload MCP script not found: $reloadMcp"
}

$installed = 0
if ((Test-CommandAvailable "codex") -or (Test-AnyPath @($codexHome))) {
    Install-CodexMcp
    $installed += 1
}
else {
    Write-WarningMessage "Codex not detected; skipped."
}

if ((Test-CommandAvailable "claude") -or (Test-AnyPath @((Join-Path $env:USERPROFILE ".claude")))) {
    Install-ClaudeCodeMcp
    $installed += 1
}
else {
    Write-WarningMessage "Claude Code not detected; skipped."
}

if ((Test-CommandAvailable "cursor") -or (Test-AnyPath @((Join-Path $env:APPDATA "Cursor"), (Join-Path $env:USERPROFILE ".cursor"), (Join-Path $env:LOCALAPPDATA "Programs\cursor")))) {
    Install-CursorMcp
    $installed += 1
}
else {
    Write-WarningMessage "Cursor not detected; skipped."
}

if ((Test-CommandAvailable "code") -or (Test-AnyPath @((Join-Path $env:APPDATA "Code"), (Join-Path $env:LOCALAPPDATA "Programs\Microsoft VS Code")))) {
    Install-VsCodeCopilotMcp
    $installed += 1
}
else {
    Write-WarningMessage "VS Code Copilot not detected; skipped."
}

if ($installed -eq 0) {
    Write-WarningMessage "No supported local agents detected."
}
elseif ($DryRun) {
    Write-Success "MCP setup dry-run complete for $installed agent target(s)."
}
else {
    Write-Success "MCP setup complete for $installed agent target(s)."
}
