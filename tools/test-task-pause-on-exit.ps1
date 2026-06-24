# Decides whether a task batch file was launched as a transient Explorer/Terminal command.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-ProcessSnapshotById {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable] $ProcessById,

        [Parameter(Mandatory = $true)]
        [int] $ProcessId
    )

    if (-not $ProcessById.ContainsKey($ProcessId)) {
        return $null
    }

    $ProcessById[$ProcessId]
}

function Get-ProcessNameWithoutExtension {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ProcessName
    )

    [System.IO.Path]::GetFileNameWithoutExtension($ProcessName).ToLowerInvariant()
}

try {
    $interactiveShells = @("cmd", "powershell", "pwsh")
    $transientLaunchers = @("explorer", "windowsterminal", "windowsterminalpreview", "wt", "openconsole")
    $processById = @{}

    foreach ($process in Get-CimInstance -ClassName Win32_Process -Property ProcessId, ParentProcessId, Name -ErrorAction Stop) {
        $processById[[int] $process.ProcessId] = $process
    }

    $helperProcess = Get-ProcessSnapshotById -ProcessById $processById -ProcessId $PID
    if ($null -eq $helperProcess) {
        exit 0
    }

    $helperCmdProcess = Get-ProcessSnapshotById -ProcessById $processById -ProcessId ([int] $helperProcess.ParentProcessId)
    if ($null -eq $helperCmdProcess) {
        exit 0
    }

    if ((Get-ProcessNameWithoutExtension -ProcessName $helperCmdProcess.Name) -ne "cmd") {
        exit 0
    }

    $taskCmdProcess = Get-ProcessSnapshotById -ProcessById $processById -ProcessId ([int] $helperCmdProcess.ParentProcessId)
    if ($null -eq $taskCmdProcess) {
        exit 0
    }

    if ((Get-ProcessNameWithoutExtension -ProcessName $taskCmdProcess.Name) -ne "cmd") {
        exit 0
    }

    $currentProcess = $taskCmdProcess
    for ($depth = 0; $depth -lt 6; $depth++) {
        $parentProcess = Get-ProcessSnapshotById -ProcessById $processById -ProcessId ([int] $currentProcess.ParentProcessId)
        if ($null -eq $parentProcess) {
            exit 0
        }

        $parentName = Get-ProcessNameWithoutExtension -ProcessName $parentProcess.Name
        if ($interactiveShells -contains $parentName) {
            exit 0
        }

        if ($transientLaunchers -contains $parentName) {
            "1"
            exit 0
        }

        $currentProcess = $parentProcess
    }
}
catch {
    exit 0
}
