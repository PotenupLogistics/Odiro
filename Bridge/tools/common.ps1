# Shared helpers for Bridge task scripts.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Resolved absolute Bridge project root.
$script:BridgeRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
# Current console message prefix.
$script:ToolPrefix = "bridge"

# Returns the Bridge root used by callers as their working base.
function Get-BridgeRoot {
    return $script:BridgeRoot
}

# Sets the message prefix for the current script.
function Set-ToolPrefix {
    param([string] $Prefix)
    $script:ToolPrefix = $Prefix
}

# Writes a Bridge-formatted console message.
function Write-ToolMessage {
    param(
        [string] $Message,
        [ConsoleColor] $Color = [ConsoleColor]::White
    )

    Write-Host "[$script:ToolPrefix] $Message" -ForegroundColor $Color
}

# Writes an in-progress step.
function Write-Step {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Cyan
}

# Writes a successful completion message.
function Write-Success {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Green
}

# Writes a warning that may still allow the caller to continue.
function Write-WarningMessage {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Yellow
}

# Writes an error in the standard Bridge tool format.
function Write-ErrorMessage {
    param([string] $Message)
    Write-ToolMessage -Message "ERROR: $Message" -Color Red
}

# Verifies that a required command is available on PATH.
function Assert-Command {
    param([string] $Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found on PATH."
    }
}

# Creates the shared prerequisite issue shape used by setup scripts.
function New-PrerequisiteIssue {
    param(
        [string] $Name,
        [string] $Detail,
        [string] $Reason = "",
        [string[]] $MissingItems = @(),
        [string] $Install,
        [string] $Verify = "",
        [string[]] $Docs = @()
    )

    return [pscustomobject]@{
        Name = $Name
        Detail = $Detail
        Reason = $Reason
        MissingItems = $MissingItems
        Install = $Install
        Verify = $Verify
        Docs = $Docs
    }
}

# Reads an optional issue property with a default value.
function Get-IssueValue {
    param(
        [object] $Issue,
        [string] $PropertyName,
        [object] $DefaultValue = ""
    )

    $property = $Issue.PSObject.Properties[$PropertyName]
    if ($property) {
        return $property.Value
    }
    return $DefaultValue
}

# Prints prerequisite results and applies the failure policy.
function Complete-Prerequisites {
    param(
        [object[]] $Issues,
        [switch] $AllowMissing,
        [string] $SuccessMessage,
        [string] $ErrorMessage
    )

    if ($Issues.Count -eq 0) {
        Write-Success $SuccessMessage
        return
    }

    Write-WarningMessage "Missing Bridge prerequisites:"
    foreach ($issue in $Issues) {
        Write-WarningMessage "- $(Get-IssueValue -Issue $issue -PropertyName "Name")"

        $detail = Get-IssueValue -Issue $issue -PropertyName "Detail"
        if (-not [string]::IsNullOrWhiteSpace($detail)) {
            Write-WarningMessage "  Status: $detail"
        }

        $reason = Get-IssueValue -Issue $issue -PropertyName "Reason"
        if (-not [string]::IsNullOrWhiteSpace($reason)) {
            Write-WarningMessage "  Required for: $reason"
        }

        $install = Get-IssueValue -Issue $issue -PropertyName "Install"
        if (-not [string]::IsNullOrWhiteSpace($install)) {
            Write-Step "  Install: $install"
        }

        $verify = Get-IssueValue -Issue $issue -PropertyName "Verify"
        if (-not [string]::IsNullOrWhiteSpace($verify)) {
            Write-Step "  Verify: $verify"
        }

        $docs = @(Get-IssueValue -Issue $issue -PropertyName "Docs" -DefaultValue @()) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
        foreach ($doc in $docs) {
            Write-Step "  Docs: $doc"
        }
    }

    if ($AllowMissing) {
        Write-WarningMessage "Continuing with missing prerequisites allowed."
        return
    }

    throw $ErrorMessage
}
