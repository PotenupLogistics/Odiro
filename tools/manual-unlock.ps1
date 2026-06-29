# Human-only helper for unlocking Git LFS locks by exact path or per-lock prompt.
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Path = "",

    [switch] $All,

    [switch] $Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
. "$PSScriptRoot\list-lfs-locks.ps1"

# Returns the supported command-line forms for manual Git LFS unlocks.
function Get-ManualUnlockUsage {
    return @"
Usage:
  tools/manual-unlock.ps1 <Path>
  tools/manual-unlock.ps1 -All
  tools/manual-unlock.ps1 -Force <Path>
  tools/manual-unlock.ps1 -Force -All
"@.Trim()
}

# Normalizes user input to repository-relative Git paths.
function ConvertTo-GitPath {
    param([string] $InputPath)

    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw "Path is required."
    }
    if ([System.IO.Path]::IsPathRooted($InputPath)) {
        throw "Use a repository-relative path, not an absolute path."
    }

    $gitPath = ($InputPath -replace '\\', '/').Trim()
    $gitPath = $gitPath.TrimStart('./')
    if ([string]::IsNullOrWhiteSpace($gitPath) -or $gitPath -eq "." -or $gitPath -eq "/") {
        throw "Repository root is not a valid unlock target."
    }
    if ($gitPath -match '(^|/)\.\.($|/)') {
        throw "Parent directory segments are not allowed."
    }
    if ($gitPath.StartsWith("-", [System.StringComparison]::Ordinal)) {
        throw "Path must not start with '-'."
    }

    return $gitPath
}

# Normalizes a Git LFS lock row path without treating literal path characters as user wildcards.
function ConvertLockPathToGitPath {
    param([string] $InputPath)

    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw "Lock path is empty."
    }

    $gitPath = ($InputPath -replace '\\', '/').Trim()
    $gitPath = $gitPath.TrimStart('./')
    if ([string]::IsNullOrWhiteSpace($gitPath) -or $gitPath -eq "." -or $gitPath -eq "/") {
        throw "Git LFS returned an invalid lock path."
    }
    if ($gitPath -match '(^|/)\.\.($|/)') {
        throw "Git LFS returned a lock path with parent directory segments."
    }

    return $gitPath
}

# Verifies that an unlocked Git LFS lock id is no longer listed.
function Assert-LfsLockRemovedById {
    param(
        [string] $RepoRoot,
        [string] $Id
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        return
    }

    foreach ($attempt in 1..5) {
        if ($attempt -gt 1) {
            Start-Sleep -Seconds 2
        }

        $remainingLocks = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
        $remainingLock = @($remainingLocks | Where-Object { [string] $_.Id -eq $Id })
        if ($remainingLock.Count -eq 0) {
            return
        }
    }

    throw "Git LFS lock id $Id is still listed after unlock."
}

# Runs a native command and throws with its stderr/stdout when it fails.
function Invoke-CheckedNativeCommand {
    param(
        [string] $FailureMessage,
        [scriptblock] $Command
    )

    $output = @(& $Command 2>&1)
    if ($LASTEXITCODE -eq 0) {
        return $output
    }

    $detail = (($output | ForEach-Object { [string] $_ }) -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($detail)) {
        throw $FailureMessage
    }

    throw "$FailureMessage`n$detail"
}

# Reads the active GitHub CLI login and token for force unlock authentication.
function Get-GhAuthContext {
    Assert-Command "gh"

    $loginOutput = @(gh api user --jq ".login" 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $detail = (($loginOutput | ForEach-Object { [string] $_ }) -join "`n").Trim()
        throw "Failed to read active GitHub login from gh.`n$detail"
    }

    $login = (($loginOutput | Select-Object -First 1) -as [string]).Trim()
    if ([string]::IsNullOrWhiteSpace($login)) {
        throw "Active GitHub login from gh is empty."
    }
    if ($login -match '[\s:@/]') {
        throw "Active GitHub login contains unsupported characters: $login"
    }

    $tokenOutput = @(gh auth token 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $detail = (($tokenOutput | ForEach-Object { [string] $_ }) -join "`n").Trim()
        throw "Failed to read active GitHub token from gh.`n$detail"
    }

    $token = (($tokenOutput | Select-Object -First 1) -as [string]).Trim()
    if ([string]::IsNullOrWhiteSpace($token)) {
        throw "Active GitHub token from gh is empty."
    }

    return [pscustomobject]@{
        Login = $login
        Token = $token
    }
}

# Creates a temporary Git askpass helper that returns the supplied GitHub login and token.
function New-GitAskPassHelper {
    param(
        [string] $Login,
        [string] $Token
    )

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("odiro-lfs-askpass-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tempDir | Out-Null

    $isWindowsRuntime = [System.IO.Path]::DirectorySeparatorChar -eq [char] '\'
    if ($isWindowsRuntime) {
        $askPassPath = Join-Path $tempDir "askpass.cmd"
        $askPassScript = @"
@echo off
setlocal EnableExtensions
set PROMPT_ARG=%~1
echo %PROMPT_ARG% | findstr /I "Username" >nul
if %ERRORLEVEL%==0 (
  echo %LFS_LOCK_BOT_LOGIN%
) else (
  echo %LFS_LOCK_BOT_TOKEN%
)
"@
    }
    else {
        $askPassPath = Join-Path $tempDir "askpass.sh"
        $askPassScript = (@(
            '#!/usr/bin/env sh',
            'case "$1" in',
            '  *Username*) printf ''%s\n'' "${LFS_LOCK_BOT_LOGIN}" ;;',
            '  *) printf ''%s\n'' "${LFS_LOCK_BOT_TOKEN}" ;;',
            'esac'
        ) -join "`n") + "`n"
    }

    [System.IO.File]::WriteAllText($askPassPath, $askPassScript, [System.Text.UTF8Encoding]::new($false))
    if (-not $isWindowsRuntime) {
        chmod 700 $askPassPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to make Git askpass helper executable."
        }
    }

    return [pscustomobject]@{
        Path = $askPassPath
        TempDir = $tempDir
        Login = $Login
        Token = $Token
    }
}

# Returns the repository's effective Git LFS API endpoint without a trailing slash.
function Get-LfsApiEndpoint {
    param([string] $RepoRoot)

    $lines = @(git -C $RepoRoot lfs env 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $detail = (($lines | ForEach-Object { [string] $_ }) -join "`n").Trim()
        throw "Failed to read Git LFS environment.`n$detail"
    }

    foreach ($line in $lines) {
        if ([string] $line -match '^Endpoint=(?<Url>\S+)(?:\s+\(auth=[^)]+\))?') {
            return ([string] $Matches["Url"]).TrimEnd("/")
        }
    }

    throw "Git LFS endpoint was not found."
}

# Builds a Git LFS Basic auth header from a GitHub login and token.
function New-LfsBasicAuthHeader {
    param(
        [string] $Login,
        [string] $Token
    )

    if ([string]::IsNullOrWhiteSpace($Login)) {
        throw "GitHub login is required for Git LFS API auth."
    }
    if ([string]::IsNullOrWhiteSpace($Token)) {
        throw "GitHub token is required for Git LFS API auth."
    }

    $credential = "${Login}:${Token}"
    $encodedCredential = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($credential))
    return "Authorization: Basic $encodedCredential"
}

# Calls the Git LFS HTTP API with explicit Basic auth.
function Invoke-LfsApi {
    param(
        [string] $Method,
        [string] $Url,
        [string] $Login,
        [string] $Token,
        [string] $Body = ""
    )

    $curlCommand = Get-Command "curl.exe" -ErrorAction SilentlyContinue
    if (-not $curlCommand) {
        $curlCommand = Get-Command "curl" -ErrorAction SilentlyContinue
    }
    if (-not $curlCommand) {
        throw "curl was not found on PATH."
    }

    $responsePath = [System.IO.Path]::GetTempFileName()
    $requestPath = $null
    try {
        $curlArgs = @(
            "--silent",
            "--show-error",
            "--location",
            "--request",
            $Method,
            "--header",
            (New-LfsBasicAuthHeader -Login $Login -Token $Token),
            "--header",
            "Accept: application/vnd.git-lfs+json",
            "--header",
            "Content-Type: application/vnd.git-lfs+json",
            "--output",
            $responsePath,
            "--write-out",
            "%{http_code}",
            $Url
        )

        if (-not [string]::IsNullOrWhiteSpace($Body)) {
            $requestPath = [System.IO.Path]::GetTempFileName()
            [System.IO.File]::WriteAllText($requestPath, $Body, [System.Text.UTF8Encoding]::new($false))
            $curlArgs = @("--data-binary", "@$requestPath") + $curlArgs
        }

        $statusOutput = @(& $curlCommand.Source @curlArgs 2>&1)
        $curlExitCode = $LASTEXITCODE
        $statusText = (($statusOutput | ForEach-Object { [string] $_ }) -join "`n").Trim()
        $responseText = [System.IO.File]::ReadAllText($responsePath)

        if ($curlExitCode -ne 0) {
            throw "curl failed for Git LFS API request.`n$statusText`n$responseText"
        }
        if ($statusText -notmatch "(?<Status>\d{3})$") {
            throw "curl did not return an HTTP status code for Git LFS API request.`n$statusText`n$responseText"
        }

        return [pscustomobject]@{
            Status = [int] $Matches["Status"]
            Body = $responseText.Trim()
        }
    }
    finally {
        Remove-Item -LiteralPath $responsePath -Force -ErrorAction SilentlyContinue
        if ($null -ne $requestPath) {
            Remove-Item -LiteralPath $requestPath -Force -ErrorAction SilentlyContinue
        }
    }
}

# Unlocks one Git LFS lock by id through the HTTP API, bypassing local file path checks.
function Invoke-ApiUnlockById {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id,
        [switch] $Force
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        throw "Git LFS lock id is required."
    }

    $auth = Get-GhAuthContext
    $endpoint = Get-LfsApiEndpoint -RepoRoot $RepoRoot
    $escapedId = [Uri]::EscapeDataString($Id)
    $body = @{ force = [bool] $Force } | ConvertTo-Json -Compress
    $response = Invoke-LfsApi `
        -Method "POST" `
        -Url "$endpoint/locks/$escapedId/unlock" `
        -Login $auth.Login `
        -Token $auth.Token `
        -Body $body

    if ($response.Status -lt 200 -or $response.Status -ge 300) {
        $mode = if ($Force) { "force unlock" } else { "unlock" }
        throw "Failed to $mode $Path by Git LFS lock id ${Id}.`nHTTP $($response.Status)`n$($response.Body)"
    }
}

# Forces Git LFS to use the active GitHub CLI token while a script block runs.
function Invoke-WithForceUnlockAuth {
    param([scriptblock] $Command)

    $auth = Get-GhAuthContext
    $helper = New-GitAskPassHelper -Login $auth.Login -Token $auth.Token

    $oldAskPass = $env:GIT_ASKPASS
    $oldPrompt = $env:GIT_TERMINAL_PROMPT
    $oldToken = $env:LFS_LOCK_BOT_TOKEN
    $oldLogin = $env:LFS_LOCK_BOT_LOGIN

    try {
        $env:GIT_ASKPASS = $helper.Path
        $env:GIT_TERMINAL_PROMPT = "0"
        $env:LFS_LOCK_BOT_TOKEN = $auth.Token
        $env:LFS_LOCK_BOT_LOGIN = $auth.Login

        $usernameProbe = @(& $helper.Path "Username for 'https://github.com':" 2>&1)
        if ($LASTEXITCODE -ne 0 -or (($usernameProbe | Select-Object -First 1) -as [string]).Trim() -ne $auth.Login) {
            throw "Git askpass helper username probe failed."
        }

        $passwordProbe = @(& $helper.Path "Password for 'https://$($auth.Login)@github.com':" 2>&1)
        if ($LASTEXITCODE -ne 0 -or (($passwordProbe | Select-Object -First 1) -as [string]).Trim() -ne $auth.Token) {
            throw "Git askpass helper token probe failed."
        }

        Write-Step "Force unlock auth: gh token as $($auth.Login)"
        & $Command
    }
    finally {
        $env:GIT_ASKPASS = $oldAskPass
        $env:GIT_TERMINAL_PROMPT = $oldPrompt
        $env:LFS_LOCK_BOT_TOKEN = $oldToken
        $env:LFS_LOCK_BOT_LOGIN = $oldLogin
        Remove-Item -LiteralPath $helper.TempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Unlocks a lock through the normal Git LFS operation, preferring lock id for dangling paths.
function Invoke-NormalUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id
    )

    if (-not [string]::IsNullOrWhiteSpace($Id)) {
        Invoke-ApiUnlockById -RepoRoot $RepoRoot -Path $Path -Id $Id
        return
    }

    Invoke-CheckedNativeCommand `
        -FailureMessage "Failed to unlock $Path." `
        -Command { git -C $RepoRoot lfs unlock $Path } |
        Out-Null
}

# Force-unlocks a lock through Git LFS, preferring the lock id to avoid path ambiguity.
function Invoke-ForceUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [string] $Id
    )

    if ([string]::IsNullOrWhiteSpace($Id)) {
        Invoke-WithForceUnlockAuth {
            Invoke-CheckedNativeCommand `
                -FailureMessage "Failed to force unlock $Path." `
                -Command { git -C $RepoRoot lfs unlock --force $Path } |
                Out-Null
        }
        return
    }

    Invoke-ApiUnlockById -RepoRoot $RepoRoot -Path $Path -Id $Id -Force
}

# Unlocks one path using either normal or force mode and verifies removed lock ids.
function Invoke-LfsUnlock {
    param(
        [string] $RepoRoot,
        [string] $Path,
        [object[]] $Locks,
        [switch] $Force
    )

    if ($Force) {
        foreach ($lock in $Locks) {
            Invoke-ForceUnlock -RepoRoot $RepoRoot -Path $Path -Id ([string] $lock.Id)
            Assert-LfsLockRemovedById -RepoRoot $RepoRoot -Id ([string] $lock.Id)
        }
        return
    }

    foreach ($lock in $Locks) {
        Invoke-NormalUnlock -RepoRoot $RepoRoot -Path $Path -Id ([string] $lock.Id)
        Assert-LfsLockRemovedById -RepoRoot $RepoRoot -Id ([string] $lock.Id)
    }
}

# Prompts for explicit per-lock consent, defaulting every response to no.
function Read-UnlockConfirmation {
    param(
        [object] $LockRow,
        [string] $GitPath,
        [switch] $Force
    )

    Write-Host ""
    Write-Host "Path     : $GitPath"
    Write-Host "Owner    : $($LockRow.Owner)"
    Write-Host "LockedAt : $($LockRow.LockedAt)"
    Write-Host "Id       : $($LockRow.Id)"

    if ($Force) {
        $answer = Read-Host "Force unlock this path? Type y to unlock [y/N]"
    }
    else {
        $answer = Read-Host "Unlock this path? Type y to unlock [y/N]"
    }

    if ($null -eq $answer) {
        return $false
    }

    $answer = $answer.Trim()
    return [string]::Equals($answer, "y", [System.StringComparison]::OrdinalIgnoreCase)
}

# Prompts through every current Git LFS lock and unlocks only selected rows.
function Invoke-UnlockAllLocks {
    param(
        [string] $RepoRoot,
        [switch] $Force
    )

    $rows = @(Get-LfsLockRows -RepoRoot $RepoRoot -NormalizedPrefix "")
    if ($rows.Count -eq 0) {
        Write-Success "No active Git LFS locks."
        return
    }

    Write-Step "Active Git LFS locks: $($rows.Count)"
    $unlockedCount = 0
    $skippedCount = 0
    $failedCount = 0

    foreach ($row in $rows) {
        $gitPath = ConvertLockPathToGitPath -InputPath ([string] $row.Path)
        if (-not (Read-UnlockConfirmation -LockRow $row -GitPath $gitPath -Force:$Force)) {
            Write-Step "Skipped: $gitPath"
            $skippedCount += 1
            continue
        }

        try {
            Invoke-LfsUnlock -RepoRoot $RepoRoot -Path $gitPath -Locks @($row) -Force:$Force
            Write-Success "Unlocked: $gitPath"
            $unlockedCount += 1
        }
        catch {
            Write-WarningMessage "Failed: $gitPath"
            Write-WarningMessage $_.Exception.Message
            $failedCount += 1
        }
    }

    if ($failedCount -gt 0) {
        throw "Unlock prompts complete with $failedCount failure(s). Unlocked $unlockedCount, skipped $skippedCount."
    }

    Write-Success "Unlock prompts complete. Unlocked $unlockedCount, skipped $skippedCount."
}

# Runs the command-line entry point for manual Git LFS unlocks.
function Invoke-ManualUnlockScript {
    Set-ToolPrefix "manual-unlock"

    try {
        Assert-Command "git"

        if ($All -and -not [string]::IsNullOrWhiteSpace($Path)) {
            throw "-All cannot be combined with Path.`n$(Get-ManualUnlockUsage)"
        }
        if (-not $All -and [string]::IsNullOrWhiteSpace($Path)) {
            throw "Path is required unless -All is set.`n$(Get-ManualUnlockUsage)"
        }

        $repoRoot = Get-RepoRoot
        Write-WarningMessage "Human-only script. Coding agents must not run this unless the user explicitly requested unlock in the current turn."

        if ($All) {
            Invoke-UnlockAllLocks -RepoRoot $repoRoot -Force:$Force
            return
        }

        $repoPath = ConvertTo-GitPath -InputPath $Path
        $locks = @(Get-LfsLockRows -RepoRoot $repoRoot -NormalizedPrefix "")
        $matchingLocks = @(
            $locks | Where-Object {
                (ConvertLockPathToGitPath -InputPath ([string] $_.Path)) -eq $repoPath
            }
        )

        if ($matchingLocks.Count -eq 0) {
            Write-Step "No active lock: $repoPath"
            return
        }

        foreach ($lock in $matchingLocks) {
            Write-Step "Lock id=$($lock.Id) path=$repoPath owner=$($lock.Owner) locked_at=$($lock.LockedAt)"
        }

        Invoke-LfsUnlock -RepoRoot $repoRoot -Path $repoPath -Locks $matchingLocks -Force:$Force
        Write-Success "Unlocked: $repoPath"
    }
    catch {
        Write-ErrorMessage $_.Exception.Message
        exit 1
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    Invoke-ManualUnlockScript
}
