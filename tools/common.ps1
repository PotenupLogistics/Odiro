# Shared helper functions for Odiro development scripts.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$script:OdiroRepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$script:ToolPrefix = "tools"

function Get-RepoRoot {
    return $script:OdiroRepoRoot
}

function Get-ClientDir {
    param([string] $RepoRoot)
    return Join-Path $RepoRoot "Client"
}

function Get-AgentsDir {
    param([string] $RepoRoot)
    return Join-Path $RepoRoot "Agents"
}

function Write-ToolMessage {
    param(
        [string] $Message,
        [string] $Prefix = "",
        [ConsoleColor] $Color = [ConsoleColor]::White
    )

    if ([string]::IsNullOrWhiteSpace($Prefix)) {
        $Prefix = $script:ToolPrefix
    }

    Write-Host "[$Prefix] $Message" -ForegroundColor $Color
}

function Set-ToolPrefix {
    param([string] $Prefix)
    $script:ToolPrefix = $Prefix
}

function Write-Step {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Cyan
}

function Write-Success {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Green
}

function Write-WarningMessage {
    param([string] $Message)
    Write-ToolMessage -Message $Message -Color Yellow
}

function Write-ErrorMessage {
    param([string] $Message)
    Write-ToolMessage -Message "ERROR: $Message" -Color Red
}

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

function Complete-Prerequisites {
    param(
        [object[]] $Issues,
        [switch] $AllowMissing,
        [string] $SuccessMessage = "Setup prerequisites OK.",
        [string] $ErrorMessage = "Setup prerequisites are missing. Install the missing tools, or rerun setup with -AllowMissingPrerequisites to continue anyway."
    )

    if ($Issues.Count -eq 0) {
        Write-Success $SuccessMessage
        return
    }

    Write-WarningMessage "Missing setup prerequisites:"
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

        $missingItems = @(Get-IssueValue -Issue $issue -PropertyName "MissingItems" -DefaultValue @())
        if ($missingItems.Count -gt 0) {
            Write-WarningMessage "  Missing items:"
            foreach ($item in $missingItems) {
                Write-WarningMessage "    - $item"
            }
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
        Write-WarningMessage "Continuing because -AllowMissing was set."
        return
    }

    throw $ErrorMessage
}

function Assert-Command {
    param([string] $Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found on PATH."
    }
}

function Stop-ProcessTree {
    param([int] $ProcessId)

    $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $ProcessId" -ErrorAction SilentlyContinue
    foreach ($child in $children) {
        Stop-ProcessTree -ProcessId ([int] $child.ProcessId)
    }

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($process) {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    }
}
