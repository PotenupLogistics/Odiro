# Shared Crash Report Client diagnostics for editor lifecycle scripts.

# Returns whether a process id is currently alive.
function Test-UnrealProcessAlive {
    param([int] $ProcessId)
    if ($ProcessId -le 0) {
        return $false
    }
    return $null -ne (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)
}

# Returns CrashReportClient processes and whether they are safe editor monitors or blocking reports.
function Get-UnrealCrashReportClientStates {
    $states = New-Object System.Collections.Generic.List[object]
    $processes = @(Get-CimInstance Win32_Process -Filter "Name = 'CrashReportClient.exe' OR Name = 'CrashReportClientEditor.exe'" -ErrorAction SilentlyContinue)
    foreach ($process in $processes) {
        $processId = [int] $process.ProcessId
        $commandLine = [string] $process.CommandLine
        $monitorPid = 0
        if ($commandLine -match "(?:^|\s)-MONITOR=(\d+)") {
            $monitorPid = [int] $Matches[1]
        }

        $monitorAlive = Test-UnrealProcessAlive -ProcessId $monitorPid
        $mainWindowTitle = ""
        $responding = $null
        $processObject = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($processObject) {
            $mainWindowTitle = [string] $processObject.MainWindowTitle
            $responding = $processObject.Responding
        }

        $classification = "crash_report_pending"
        $blocksEditorLaunch = $true
        if ($monitorPid -gt 0 -and $monitorAlive) {
            $classification = "active_monitor"
            $blocksEditorLaunch = $false
        }
        elseif ($monitorPid -gt 0) {
            $classification = "orphaned_report"
        }
        elseif ($mainWindowTitle) {
            $classification = "report_dialog"
        }
        else {
            $classification = "unknown_report_client"
        }

        $states.Add([pscustomobject]@{
            pid = $processId
            name = [string] $process.Name
            parentPid = [int] $process.ParentProcessId
            monitorPid = $monitorPid
            monitorAlive = $monitorAlive
            classification = $classification
            blocksEditorLaunch = $blocksEditorLaunch
            mainWindowTitle = $mainWindowTitle
            responding = $responding
            commandLine = $commandLine
        })
    }
    return @($states.ToArray())
}

# Returns CrashReportClient entries that should block starting another editor.
function Get-PendingUnrealCrashReportClients {
    return @(Get-UnrealCrashReportClientStates | Where-Object { $_.blocksEditorLaunch })
}

# Stops editor launch while an unresolved crash reporter can hold crash state.
function Assert-NoPendingUnrealCrashReportClients {
    param([string] $Message = "crash_report_pending: Close the Unreal Crash Report window before launching another editor.")
    $pendingCrashReports = @(Get-PendingUnrealCrashReportClients)
    if ($pendingCrashReports.Count -eq 0) {
        return
    }
    $details = ($pendingCrashReports | ForEach-Object { "$($_.name) pid=$($_.pid) monitor=$($_.monitorPid) $($_.classification)" }) -join "; "
    throw "$Message $details"
}
