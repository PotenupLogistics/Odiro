# Configures repository Git, GitHub identity, Git LFS locking, and Unreal Editor source-control settings.
param(
    [string] $Hostname = "github.com",
    [string] $Email = "",
    [switch] $SkipCredentialSetup,
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/git-config"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Assert-Command "git"

# Stops before changing anything when GitHub CLI is not installed.
function Assert-GitHubCli {
    if (Get-Command "gh" -ErrorAction SilentlyContinue) {
        return
    }

    throw @"
GitHub CLI was not found on PATH.
Install it, open a new shell, then authenticate:

  winget install --id GitHub.cli -e
  gh auth login -h $Hostname
  .\tools\set-git-config.ps1

If winget is unavailable, install GitHub CLI from:
  https://cli.github.com/
"@
}

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) {
    throw "Not inside a Git repository."
}

$repoRoot = $repoRoot.Trim()
if ($repoRoot.Length -eq 0) {
    throw "Not inside a Git repository."
}

# Reads a local Git config value without treating a missing key as fatal.
function Get-LocalGitConfig {
    param([string] $Name)

    $value = @(git -C $repoRoot config --local --get $Name)
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return (($value -join "`n").Trim())
}

# Reads a config value from a repository config file.
function Get-GitConfigFromFile {
    param(
        [string] $File,
        [string] $Name
    )

    $value = @(git -C $repoRoot config --file $File --get $Name)
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return (($value -join "`n").Trim())
}

# Reads an effective Git config value from local, global, or system config.
function Get-EffectiveGitConfig {
    param([string] $Name)

    $value = @(git -C $repoRoot config --get $Name)
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return (($value -join "`n").Trim())
}

# Formats command output for user-facing diagnostics without dumping full payloads.
function Get-OutputPreview {
    param([string[]] $Lines)

    $preview = @($Lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -First 3)
    if ($preview.Count -eq 0) {
        return "<empty>"
    }

    return (($preview -join " | ") -replace "\s+", " ").Trim()
}

# Quotes one native process argument for the Windows command line.
function ConvertTo-NativeArgument {
    param([string] $Value)

    if ($null -eq $Value) {
        return '""'
    }

    if ($Value -notmatch '[\s"]') {
        return $Value
    }

    return '"' + ($Value -replace '"', '\"') + '"'
}

# Splits captured process output into trimmed non-empty lines.
function ConvertFrom-ProcessOutput {
    param([string] $Output)

    if ([string]::IsNullOrWhiteSpace($Output)) {
        return @()
    }

    return @($Output -split "\r?\n" | ForEach-Object { ([string] $_).Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

# Runs gh while capturing stdout and stderr separately for clear diagnostics.
function Invoke-GitHubCli {
    param(
        [string[]] $Arguments,
        [string] $HostOverride = ""
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = "gh"
    $startInfo.Arguments = (($Arguments | ForEach-Object { ConvertTo-NativeArgument -Value $_ }) -join " ")
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true
    if (-not [string]::IsNullOrWhiteSpace($HostOverride)) {
        $startInfo.EnvironmentVariables["GH_HOST"] = $HostOverride
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void] $process.Start()
        $stdoutText = $process.StandardOutput.ReadToEnd()
        $stderrText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        $exitCode = $process.ExitCode
    }
    finally {
        $process.Dispose()
    }

    $stdoutLines = @(ConvertFrom-ProcessOutput -Output $stdoutText)
    $stderrLines = @(ConvertFrom-ProcessOutput -Output $stderrText)

    return [pscustomobject]@{
        ExitCode = $exitCode
        StdOut = $stdoutLines
        StdErr = $stderrLines
        Lines = @($stdoutLines + $stderrLines)
    }
}

# Reads one field from the authenticated GitHub user without parsing the full profile payload.
function Get-GitHubUserField {
    param(
        [string] $Query,
        [string] $Name,
        [switch] $Optional
    )

    $apiResult = Invoke-GitHubCli -Arguments @("api", "user", "--jq", $Query) -HostOverride $Hostname
    if ($apiResult.ExitCode -ne 0) {
        throw "Failed to read the authenticated GitHub user $Name with 'gh api user --jq $Query'. gh output: $(Get-OutputPreview -Lines $apiResult.Lines). Run: gh auth status -h $Hostname; if it is logged in, run: gh auth refresh -h $Hostname"
    }

    $value = (($apiResult.StdOut | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join "`n").Trim()
    if ([string]::Equals($value, "null", [System.StringComparison]::OrdinalIgnoreCase)) {
        $value = ""
    }

    if (-not $Optional -and [string]::IsNullOrWhiteSpace($value)) {
        throw "GitHub CLI returned an empty GitHub user $Name."
    }

    return $value
}

# Loads the authenticated GitHub account from gh.
function Get-GitHubUser {
    $authStatus = Invoke-GitHubCli -Arguments @("auth", "status", "-h", $Hostname)
    if ($authStatus.ExitCode -ne 0) {
        throw "GitHub CLI is not authenticated for $Hostname. Run: gh auth login -h $Hostname. gh output: $(Get-OutputPreview -Lines $authStatus.Lines)"
    }

    $login = Get-GitHubUserField -Query ".login" -Name "login"
    $id = Get-GitHubUserField -Query ".id" -Name "id"
    $email = Get-GitHubUserField -Query ".email" -Name "email" -Optional

    return [pscustomobject]@{
        Login = $login
        Id = $id
        Email = $email
    }
}

# Chooses the commit email GitHub will recognize for this account.
function Resolve-CommitEmail {
    param(
        [object] $User,
        [string] $OverrideEmail
    )

    if (-not [string]::IsNullOrWhiteSpace($OverrideEmail)) {
        return $OverrideEmail.Trim()
    }

    if (-not [string]::IsNullOrWhiteSpace($User.Email)) {
        return $User.Email.Trim()
    }

    return "$($User.Id)+$($User.Login)@users.noreply.github.com"
}

# Configures GitHub CLI as Git's GitHub credential helper for the current Windows user.
function Set-GitHubCredentialHelper {
    if ($SkipCredentialSetup) {
        return
    }

    if ($DryRun) {
        Write-Step "Would run: gh auth setup-git --hostname $Hostname"
        return
    }

    gh auth setup-git --hostname $Hostname
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure Git credential helper from GitHub CLI auth."
    }

    Write-Step "Configured Git credential helper from GitHub CLI auth."
}

# Reads a JSON property as an array even when Git LFS omits it.
function Get-JsonArrayProperty {
    param(
        [object] $Object,
        [string] $Name
    )

    if ($null -eq $Object) {
        return @()
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return @()
    }

    return @($property.Value)
}

# Extracts the displayed owner name from one Git LFS lock object.
function Get-LfsLockOwnerName {
    param([object] $Lock)

    $ownerProperty = $Lock.PSObject.Properties["owner"]
    if ($null -eq $ownerProperty -or $null -eq $ownerProperty.Value) {
        return ""
    }

    $nameProperty = $ownerProperty.Value.PSObject.Properties["name"]
    if ($null -eq $nameProperty -or [string]::IsNullOrWhiteSpace([string] $nameProperty.Value)) {
        return ""
    }

    return ([string] $nameProperty.Value).Trim()
}

# Queries Git LFS for lock owners that belong to the current credentials.
function Get-CurrentLfsLockOwnerNames {
    if ($DryRun) {
        Write-Step "Would run: git lfs locks --verify --json"
        return @()
    }

    $locksJson = @(git -C $repoRoot lfs locks --verify --json 2>$null)
    if ($LASTEXITCODE -ne 0) {
        Write-WarningMessage "Git LFS lock identity check failed. Run 'git lfs locks --verify' and check GitHub credentials."
        Write-GitHubIdentityRepairHint
        return @()
    }

    if ($locksJson.Count -eq 0) {
        return @()
    }

    try {
        $locksPayload = ($locksJson -join "`n") | ConvertFrom-Json
    }
    catch {
        Write-WarningMessage "Git LFS lock identity check returned invalid JSON."
        return @()
    }

    $ownerNames = @()
    foreach ($lock in Get-JsonArrayProperty -Object $locksPayload -Name "ours") {
        $ownerName = Get-LfsLockOwnerName -Lock $lock
        if (-not [string]::IsNullOrWhiteSpace($ownerName)) {
            $ownerNames += $ownerName
        }
    }

    return @($ownerNames | Sort-Object -Unique)
}

# Points identity-related warnings to the single repair script.
function Write-GitHubIdentityRepairHint {
    Write-WarningMessage "Run: gh auth login -h $Hostname"
    Write-WarningMessage "Then run: .\tools\set-git-config.ps1"
}

# Sets one local Git config key only when it is missing.
function Set-MissingLocalGitConfig {
    param(
        [string] $Name,
        [string] $Value,
        [string] $Source
    )

    if (-not [string]::IsNullOrWhiteSpace((Get-LocalGitConfig -Name $Name))) {
        return
    }

    git -C $repoRoot config --local $Name $Value
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure $Name."
    }

    Write-Step "Configured: $Name=$Value ($Source)"
}

# Keeps repository-local Git identity aligned with the current LFS lock owner when possible.
function Set-GitIdentityFromLfsLocks {
    param([string[]] $OwnerNames)

    $localUserName = Get-LocalGitConfig -Name "user.name"
    $localUserEmail = Get-LocalGitConfig -Name "user.email"
    $effectiveUserName = Get-EffectiveGitConfig -Name "user.name"
    $effectiveUserEmail = Get-EffectiveGitConfig -Name "user.email"
    $ownerNames = @($OwnerNames)

    if ($ownerNames.Count -eq 1) {
        $lfsOwnerName = $ownerNames[0]
        if ([string]::IsNullOrWhiteSpace($localUserName)) {
            Set-MissingLocalGitConfig -Name "user.name" -Value $lfsOwnerName -Source "Git LFS lock owner"
        }
        elseif (-not [string]::Equals($localUserName, $lfsOwnerName, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-WarningMessage "Repository-local user.name '$localUserName' differs from Git LFS lock owner '$lfsOwnerName'."
            Write-GitHubIdentityRepairHint
        }
    }
    elseif ($ownerNames.Count -gt 1) {
        Write-WarningMessage "Multiple Git LFS lock owners were returned for current credentials: $($ownerNames -join ', ')."
        Write-WarningMessage "Run 'git lfs locks --verify' and check GitHub credentials."
        Write-GitHubIdentityRepairHint
    }
    elseif ([string]::IsNullOrWhiteSpace($localUserName)) {
        if ([string]::IsNullOrWhiteSpace($effectiveUserName)) {
            Write-WarningMessage "Repository-local user.name is not set, and no Git LFS lock owner is available."
        }
        else {
            Write-WarningMessage "Repository-local user.name is not set; inherited value is '$effectiveUserName'."
        }
        Write-GitHubIdentityRepairHint
    }

    if ([string]::IsNullOrWhiteSpace($localUserEmail)) {
        if ([string]::IsNullOrWhiteSpace($effectiveUserEmail)) {
            Write-WarningMessage "Repository-local user.email is not set."
        }
        else {
            Write-WarningMessage "Repository-local user.email is not set; inherited value is '$effectiveUserEmail'."
        }
        Write-WarningMessage "Git LFS lock data has no commit email."
        Write-GitHubIdentityRepairHint
    }
}

# Reads one INI key without treating a missing section or key as fatal.
function Get-IniValue {
    param(
        [string] $File,
        [string] $Section,
        [string] $Name
    )

    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        return ""
    }

    $lines = @(Get-Content -LiteralPath $File)
    $sectionLine = "[$Section]"
    $sectionStart = -1
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i].Trim() -eq $sectionLine) {
            $sectionStart = $i
            break
        }
    }

    if ($sectionStart -lt 0) {
        return ""
    }

    $keyPattern = "^\s*" + [regex]::Escape($Name) + "\s*="
    for ($i = $sectionStart + 1; $i -lt $lines.Count; ++$i) {
        $trimmed = $lines[$i].Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            return ""
        }
        if ($lines[$i] -match $keyPattern) {
            return (($lines[$i] -split "=", 2)[1]).Trim()
        }
    }

    return ""
}

# Sets one local Git config key and logs only when it changes.
function Set-ExpectedLocalGitConfig {
    param(
        [string] $Name,
        [string] $Expected
    )

    $actual = Get-LocalGitConfig -Name $Name
    if ($actual -eq $Expected) {
        return
    }

    if ($DryRun) {
        if ([string]::IsNullOrWhiteSpace($actual)) {
            Write-Step "Would configure: $Name=$Expected"
        }
        else {
            Write-Step "Would update: $Name '$actual' -> '$Expected'"
        }
        return
    }

    git -C $repoRoot config --local $Name $Expected
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure $Name."
    }

    if ([string]::IsNullOrWhiteSpace($actual)) {
        Write-Step "Configured: $Name=$Expected"
    }
    else {
        Write-Step "Updated: $Name '$actual' -> '$Expected'"
    }
}

# Warns when Unreal's Git LFS user setting differs from the lock server owner.
function Test-UnrealEditorLfsUserNameSettings {
    param([string[]] $OwnerNames)

    $ownerNames = @($OwnerNames)
    if ($ownerNames.Count -gt 1) {
        return
    }

    $expectedUserName = ""
    $expectedSource = ""
    if ($ownerNames.Count -eq 1) {
        $expectedUserName = $ownerNames[0]
        $expectedSource = "Git LFS lock owner"
    }
    else {
        $expectedUserName = Get-LocalGitConfig -Name "user.name"
        $expectedSource = "repository-local user.name"
    }

    if ([string]::IsNullOrWhiteSpace($expectedUserName)) {
        return
    }

    $clientDir = Join-Path $repoRoot "Client"
    if (-not (Test-Path -LiteralPath $clientDir -PathType Container)) {
        return
    }

    $savedConfigRoot = Join-Path $clientDir "Saved\Config"
    if (-not (Test-Path -LiteralPath $savedConfigRoot -PathType Container)) {
        return
    }

    $sourceControlSettingsFiles = @(Get-ChildItem -LiteralPath $savedConfigRoot -Recurse -File -Filter "SourceControlSettings.ini" | Select-Object -ExpandProperty FullName)
    foreach ($settingsFile in $sourceControlSettingsFiles) {
        $usingLfsLocking = Get-IniValue -File $settingsFile -Section "GitSourceControl.GitSourceControlSettings" -Name "UsingGitLfsLocking"
        if (-not [string]::IsNullOrWhiteSpace($usingLfsLocking) -and -not [string]::Equals($usingLfsLocking, "True", [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $editorUserName = Get-IniValue -File $settingsFile -Section "GitSourceControl.GitSourceControlSettings" -Name "LfsUserName"
        if ([string]::IsNullOrWhiteSpace($editorUserName)) {
            Write-WarningMessage "Unreal Editor Git LFS user name is empty in $settingsFile."
            Write-WarningMessage "Set Revision Control LFS user name to '$expectedUserName' ($expectedSource)."
            Write-GitHubIdentityRepairHint
        }
        elseif (-not [string]::Equals($editorUserName, $expectedUserName, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-WarningMessage "Unreal Editor LfsUserName '$editorUserName' differs from $expectedSource '$expectedUserName'."
            Write-WarningMessage "Set Revision Control LFS user name to '$expectedUserName'."
            Write-GitHubIdentityRepairHint
        }
    }
}

# Sets one INI key while preserving unrelated user settings.
function Set-IniValue {
    param(
        [string] $File,
        [string] $Section,
        [string] $Name,
        [string] $Expected
    )

    $actual = Get-IniValue -File $File -Section $Section -Name $Name
    if ($actual -eq $Expected) {
        return
    }

    if ($DryRun) {
        if ([string]::IsNullOrWhiteSpace($actual)) {
            Write-Step "Would configure Editor setting: $Name=$Expected"
        }
        else {
            Write-Step "Would update Editor setting: $Name '$actual' -> '$Expected'"
        }
        return
    }

    $directory = Split-Path -Parent $File
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory | Out-Null
    }

    $lines = @()
    if (Test-Path -LiteralPath $File -PathType Leaf) {
        $lines = @(Get-Content -LiteralPath $File)
    }

    $list = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $lines) {
        $list.Add($line)
    }

    $sectionLine = "[$Section]"
    $sectionStart = -1
    for ($i = 0; $i -lt $list.Count; ++$i) {
        if ($list[$i].Trim() -eq $sectionLine) {
            $sectionStart = $i
            break
        }
    }

    if ($sectionStart -lt 0) {
        if ($list.Count -gt 0 -and -not [string]::IsNullOrWhiteSpace($list[$list.Count - 1])) {
            $list.Add("")
        }
        $list.Add($sectionLine)
        $list.Add("$Name=$Expected")
        Set-Content -LiteralPath $File -Value $list
        Write-Step "Configured Editor setting: $Name=$Expected"
        return
    }

    $sectionEnd = $list.Count
    for ($i = $sectionStart + 1; $i -lt $list.Count; ++$i) {
        $trimmed = $list[$i].Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $sectionEnd = $i
            break
        }
    }

    $keyPattern = "^\s*" + [regex]::Escape($Name) + "\s*="
    for ($i = $sectionStart + 1; $i -lt $sectionEnd; ++$i) {
        if ($list[$i] -match $keyPattern) {
            $list[$i] = "$Name=$Expected"
            Set-Content -LiteralPath $File -Value $list
            Write-Step "Updated Editor setting: $Name '$actual' -> '$Expected'"
            return
        }
    }

    $list.Insert($sectionEnd, "$Name=$Expected")
    Set-Content -LiteralPath $File -Value $list
    Write-Step "Configured Editor setting: $Name=$Expected"
}

# Updates Unreal Editor's local Git LFS provider settings.
function Set-UnrealSourceControlIdentity {
    param([string] $GitHubLogin)

    $clientDir = Join-Path $repoRoot "Client"
    if (-not (Test-Path -LiteralPath $clientDir -PathType Container)) {
        return
    }

    $savedConfigRoot = Join-Path $clientDir "Saved\Config"
    $editorConfigDirs = @()
    if (Test-Path -LiteralPath $savedConfigRoot -PathType Container) {
        $editorConfigDirs = @(Get-ChildItem -LiteralPath $savedConfigRoot -Directory -Filter "*Editor" | Select-Object -ExpandProperty FullName)
    }
    if ($editorConfigDirs.Count -eq 0) {
        $editorConfigDirs = @(Join-Path $savedConfigRoot "WindowsEditor")
    }

    $gitCommand = Get-Command git -ErrorAction Stop
    $gitBinaryPath = $gitCommand.Source
    foreach ($editorConfigDir in $editorConfigDirs) {
        $settingsFile = Join-Path $editorConfigDir "SourceControlSettings.ini"
        Set-IniValue -File $settingsFile -Section "SourceControl.SourceControlSettings" -Name "Provider" -Expected "Git LFS 2"
        Set-IniValue -File $settingsFile -Section "GitSourceControl.GitSourceControlSettings" -Name "BinaryPath" -Expected $gitBinaryPath
        Set-IniValue -File $settingsFile -Section "GitSourceControl.GitSourceControlSettings" -Name "UsingGitLfsLocking" -Expected "True"
        Set-IniValue -File $settingsFile -Section "GitSourceControl.GitSourceControlSettings" -Name "LfsUserName" -Expected $GitHubLogin
    }
}

# Ensures current local Editor preferences prompt before modifying unlocked assets.
function Set-UnrealEditorCheckoutPromptSettings {
    $clientDir = Join-Path $repoRoot "Client"
    if (-not (Test-Path -LiteralPath $clientDir -PathType Container)) {
        return
    }

    $savedConfigRoot = Join-Path $clientDir "Saved\Config"
    $editorConfigDirs = @()
    if (Test-Path -LiteralPath $savedConfigRoot -PathType Container) {
        $editorConfigDirs = @(Get-ChildItem -LiteralPath $savedConfigRoot -Directory -Filter "*Editor" | Select-Object -ExpandProperty FullName)
    }
    if ($editorConfigDirs.Count -eq 0) {
        $editorConfigDirs = @(Join-Path $savedConfigRoot "WindowsEditor")
    }

    foreach ($editorConfigDir in $editorConfigDirs) {
        $settingsFile = Join-Path $editorConfigDir "EditorPerProjectUserSettings.ini"
        Set-IniValue -File $settingsFile -Section "/Script/UnrealEd.EditorLoadingSavingSettings" -Name "bAutomaticallyCheckoutOnAssetModification" -Expected "False"
        Set-IniValue -File $settingsFile -Section "/Script/UnrealEd.EditorLoadingSavingSettings" -Name "bPromptForCheckoutOnAssetModification" -Expected "True"
    }
}

# Checks one expected Git attribute set for Unreal binary assets.
function Assert-UnrealAssetAttributes {
    param([string] $Path)

    $output = @(git -C $repoRoot check-attr filter lockable text eol diff merge -- $Path)
    if ($LASTEXITCODE -ne 0) {
        throw "git check-attr failed for $Path."
    }

    $actualByName = @{}
    foreach ($line in $output) {
        $parts = $line -split ":", 3
        if ($parts.Count -ge 3) {
            $actualByName[$parts[1].Trim()] = $parts[2].Trim()
        }
    }

    $expected = @{
        filter = "unspecified"
        lockable = "set"
        text = "unset"
        eol = "unset"
        diff = "unset"
        merge = "unset"
    }

    foreach ($name in $expected.Keys) {
        $actual = $actualByName[$name]
        if ($actual -ne $expected[$name]) {
            throw "Expected $Path $name=$($expected[$name]), got '$actual'."
        }
    }

}

Assert-GitHubCli

$githubUser = Get-GitHubUser
$commitEmail = Resolve-CommitEmail -User $githubUser -OverrideEmail $Email
Write-Step "GitHub user: $($githubUser.Login)"
Set-GitHubCredentialHelper

git -C $repoRoot lfs version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Git LFS was not found. Install git-lfs and rerun setup."
}

Set-ExpectedLocalGitConfig -Name "core.hooksPath" -Expected ".githooks"

git -C $repoRoot lfs install --local --manual | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to initialize Git LFS local config."
}

git -C $repoRoot lfs update --manual | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to verify Git LFS hook snippets."
}

Set-ExpectedLocalGitConfig -Name "merge.ff" -Expected "false"
Set-ExpectedLocalGitConfig -Name "pull.ff" -Expected "true"
Set-ExpectedLocalGitConfig -Name "pull.rebase" -Expected "true"
Set-ExpectedLocalGitConfig -Name "rebase.autoStash" -Expected "true"
Set-ExpectedLocalGitConfig -Name "branch.autoSetupRebase" -Expected "always"
Set-ExpectedLocalGitConfig -Name "user.name" -Expected $githubUser.Login
Set-ExpectedLocalGitConfig -Name "user.email" -Expected $commitEmail
Set-ExpectedLocalGitConfig -Name "lfs.locksverify" -Expected "true"
Set-ExpectedLocalGitConfig -Name "lfs.setlockablereadonly" -Expected "true"

$lfsConfig = Join-Path $repoRoot ".lfsconfig"
if (-not (Test-Path -LiteralPath $lfsConfig -PathType Leaf)) {
    throw ".lfsconfig is missing."
}
if ((Get-GitConfigFromFile -File $lfsConfig -Name "lfs.locksverify") -ne "true") {
    throw ".lfsconfig must set lfs.locksverify=true."
}

$lfsOwnerNames = @(Get-CurrentLfsLockOwnerNames)
if ($lfsOwnerNames.Count -eq 1 -and -not [string]::Equals($lfsOwnerNames[0], $githubUser.Login, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-WarningMessage "Git LFS lock owner '$($lfsOwnerNames[0])' differs from GitHub CLI user '$($githubUser.Login)'."
    Write-GitHubIdentityRepairHint
}
elseif ($lfsOwnerNames.Count -gt 1) {
    Write-WarningMessage "Multiple Git LFS lock owners were returned for current credentials: $($lfsOwnerNames -join ', ')."
    Write-GitHubIdentityRepairHint
}

Set-UnrealSourceControlIdentity -GitHubLogin $githubUser.Login
Test-UnrealEditorLfsUserNameSettings -OwnerNames $lfsOwnerNames

Assert-UnrealAssetAttributes -Path "*.uasset"
Assert-UnrealAssetAttributes -Path "*.umap"

Set-UnrealEditorCheckoutPromptSettings

$head = git -C $repoRoot rev-parse --verify HEAD
if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($head)) {
    $headOid = $head.Trim()
    if ($DryRun) {
        Write-Step "Would run: git lfs post-checkout 0000000000000000000000000000000000000000 $headOid 1"
    }
    else {
        git -C $repoRoot lfs post-checkout 0000000000000000000000000000000000000000 $headOid 1
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to apply read-only state to current lockable files."
        }
    }
}

Write-Success "Git, GitHub, Git LFS, and Editor source-control config complete."
