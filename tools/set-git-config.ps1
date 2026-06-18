# Configures and checks repository-local Git, hook, and Git LFS locking settings.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/git-config"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

Assert-Command "git"

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
    $locksJson = @(git -C $repoRoot lfs locks --verify --json 2>$null)
    if ($LASTEXITCODE -ne 0) {
        Write-WarningMessage "Git LFS lock identity check failed. Run 'git lfs locks --verify' and check GitHub credentials."
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
    $localUserName = Get-LocalGitConfig -Name "user.name"
    $localUserEmail = Get-LocalGitConfig -Name "user.email"
    $effectiveUserName = Get-EffectiveGitConfig -Name "user.name"
    $effectiveUserEmail = Get-EffectiveGitConfig -Name "user.email"
    $ownerNames = @(Get-CurrentLfsLockOwnerNames)

    if ($ownerNames.Count -eq 1) {
        $lfsOwnerName = $ownerNames[0]
        if ([string]::IsNullOrWhiteSpace($localUserName)) {
            Set-MissingLocalGitConfig -Name "user.name" -Value $lfsOwnerName -Source "Git LFS lock owner"
        }
        elseif (-not [string]::Equals($localUserName, $lfsOwnerName, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-WarningMessage "Repository-local user.name '$localUserName' differs from Git LFS lock owner '$lfsOwnerName'."
            Write-WarningMessage "Run: git config --local user.name `"$lfsOwnerName`""
        }
    }
    elseif ($ownerNames.Count -gt 1) {
        Write-WarningMessage "Multiple Git LFS lock owners were returned for current credentials: $($ownerNames -join ', ')."
        Write-WarningMessage "Run 'git lfs locks --verify' and check GitHub credentials."
    }
    elseif ([string]::IsNullOrWhiteSpace($localUserName)) {
        if ([string]::IsNullOrWhiteSpace($effectiveUserName)) {
            Write-WarningMessage "Repository-local user.name is not set, and no Git LFS lock owner is available."
        }
        else {
            Write-WarningMessage "Repository-local user.name is not set; inherited value is '$effectiveUserName'."
        }
        Write-WarningMessage "Lock an Unreal asset or run: git config --local user.name <GitHub login>"
    }

    if ([string]::IsNullOrWhiteSpace($localUserEmail)) {
        if ([string]::IsNullOrWhiteSpace($effectiveUserEmail)) {
            Write-WarningMessage "Repository-local user.email is not set."
        }
        else {
            Write-WarningMessage "Repository-local user.email is not set; inherited value is '$effectiveUserEmail'."
        }
        Write-WarningMessage "Git LFS lock data has no commit email. Run: git config --local user.email <GitHub commit email>"
    }
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

# Sets one INI key while preserving unrelated user settings.
function Set-IniValue {
    param(
        [string] $File,
        [string] $Section,
        [string] $Name,
        [string] $Expected
    )

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
            $actual = (($list[$i] -split "=", 2)[1]).Trim()
            if ($actual -eq $Expected) {
                return
            }

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
Set-ExpectedLocalGitConfig -Name "lfs.locksverify" -Expected "true"
Set-ExpectedLocalGitConfig -Name "lfs.setlockablereadonly" -Expected "true"

$lfsConfig = Join-Path $repoRoot ".lfsconfig"
if (-not (Test-Path -LiteralPath $lfsConfig -PathType Leaf)) {
    throw ".lfsconfig is missing."
}
if ((Get-GitConfigFromFile -File $lfsConfig -Name "lfs.locksverify") -ne "true") {
    throw ".lfsconfig must set lfs.locksverify=true."
}

Set-GitIdentityFromLfsLocks

Assert-UnrealAssetAttributes -Path "*.uasset"
Assert-UnrealAssetAttributes -Path "*.umap"

Set-UnrealEditorCheckoutPromptSettings

$head = git -C $repoRoot rev-parse --verify HEAD
if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($head)) {
    $headOid = $head.Trim()
    git -C $repoRoot lfs post-checkout 0000000000000000000000000000000000000000 $headOid 1
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply read-only state to current lockable files."
    }
}

Write-Success "Git config complete."
