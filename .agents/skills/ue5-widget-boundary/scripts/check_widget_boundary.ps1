[CmdletBinding()]
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [string[]]$ScanPath = @(
        'Client/Source/OdiroSim/Public/Platform/Widget',
        'Client/Source/OdiroSim/Private/Platform/Widget'
    ),
    [string[]]$ExcludePath = @('*/Tests/*'),
    [switch]$ChangedOnly,
    [string[]]$FailOnRules = @(),
    [string]$BaselineFile = '',
    [switch]$FailOnFindings
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Review-oriented rules for likely WBP/C++ ownership drift.
$rules = @(
    [pscustomobject]@{
        Id = 'static-copy-in-cpp'
        Pattern = '\b(SetLabel|SetText|SetToolTipText)\s*\([^;]*(NSLOCTEXT|LOCTEXT|FText::FromString\s*\(\s*(TEXT\s*\()?["'']|FText::FromName\s*\()'
        Message = 'Hardcoded widget copy in C++ may belong in WBP defaults.'
    },
    [pscustomobject]@{
        Id = 'visual-default-in-cpp'
        Pattern = '(?:->|\.)\s*(SetIcon|SetBrush|SetBrushFromTexture|SetColorAndOpacity)\s*\('
        Message = 'Hardcoded visual defaults in C++ may belong in WBP assets or bindings.'
    },
    [pscustomobject]@{
        Id = 'widget-tick'
        Pattern = '\b(NativeTick|Tick)\s*\('
        Message = 'Widget Tick usage should be reviewed for event, delegate, or timer alternatives.'
    },
    [pscustomobject]@{
        Id = 'widget-tree-construction'
        Pattern = 'WidgetTree\s*->\s*ConstructWidget|\bConstructWidget\s*<'
        Message = 'C++ widget tree construction may duplicate WBP-authored layout.'
    },
    [pscustomobject]@{
        Id = 'cpp-size-override'
        Pattern = '\b(SetWidthOverride|SetHeightOverride|WidthOverride|HeightOverride|SetMinDesiredWidth|SetMinDesiredHeight)\b'
        Message = 'C++ sizing overrides may belong in WBP layout defaults.'
    },
    [pscustomobject]@{
        Id = 'viewport-breakpoint'
        Pattern = '\b(GetViewportSize|GetViewportScale)\s*\(|\b(ViewportSize|ScreenWidth|ScreenHeight|Breakpoint)\b'
        Message = 'Viewport-size breakpoint logic in widget C++ may belong in WBP/layout policy.'
    }
)

$highRiskRules = @(
    'widget-tree-construction',
    'viewport-breakpoint',
    'cpp-size-override'
)

function Get-RelativePath {
    param([string]$Path)
    return [System.IO.Path]::GetRelativePath($Root, $Path).Replace('\', '/')
}

function Get-FindingFingerprint {
    param([object]$Finding)
    return "$($Finding.Rule)|$($Finding.File)|$($Finding.Text)"
}

function ConvertTo-RuleSet {
    param([string[]]$RuleIds)

    $set = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($ruleId in $RuleIds) {
        if ([string]::IsNullOrWhiteSpace($ruleId)) {
            continue
        }
        if ($ruleId -eq 'high-risk') {
            foreach ($highRiskRule in $highRiskRules) {
                [void]$set.Add($highRiskRule)
            }
            continue
        }
        if ($ruleId -eq '*') {
            foreach ($rule in $rules) {
                [void]$set.Add($rule.Id)
            }
            continue
        }
        [void]$set.Add($ruleId)
    }
    return ,$set
}

function Read-BaselineFindings {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return @()
    }

    $fullPath = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $Root $Path }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        Write-Warning "Baseline file not found; treating all gate findings as new: $Path"
        return @()
    }

    $json = Get-Content -Raw -LiteralPath $fullPath | ConvertFrom-Json
    if ($null -eq $json) {
        return @()
    }
    if ($json.PSObject.Properties['findings']) {
        return @($json.findings)
    }
    return @($json)
}

function Get-ChangedRelativePaths {
    $changed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $gitTracked = @(git -C $Root diff --name-only --diff-filter=ACMRT HEAD -- 2>$null)
    $gitUntracked = @(git -C $Root ls-files --others --exclude-standard 2>$null)
    foreach ($path in @($gitTracked + $gitUntracked)) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }
        [void]$changed.Add($path.Replace('\', '/'))
    }
    return $changed
}

$scanRoots = @(
    foreach ($path in $ScanPath) {
        $fullPath = Join-Path $Root $path
        if (Test-Path -LiteralPath $fullPath) {
            Get-Item -LiteralPath $fullPath
        }
    }
)

$files = @(
    foreach ($scanRoot in $scanRoots) {
        Get-ChildItem -LiteralPath $scanRoot.FullName -Recurse -File |
            Where-Object { $_.Extension -in @('.cpp', '.h', '.hpp', '.inl') }
    }
) | Sort-Object FullName -Unique

if ($ChangedOnly) {
    $changedRelativePaths = Get-ChangedRelativePaths
    $files = @(
        foreach ($file in $files) {
            if ($changedRelativePaths.Contains((Get-RelativePath -Path $file.FullName))) {
                $file
            }
        }
    )
}

$files = @(
    foreach ($file in $files) {
        $relativePath = Get-RelativePath -Path $file.FullName
        $skipFile = $false
        foreach ($excludedPath in $ExcludePath) {
            if ($relativePath -like $excludedPath) {
                $skipFile = $true
                break
            }
        }
        if (-not $skipFile) {
            $file
        }
    }
)

$findings = [System.Collections.Generic.List[object]]::new()

foreach ($file in $files) {
    $lines = @(Get-Content -LiteralPath $file.FullName)
    for ($lineIndex = 0; $lineIndex -lt $lines.Count; $lineIndex++) {
        $line = $lines[$lineIndex]
        $trimmed = $line.Trim()
        if ($trimmed -match '^(//|/\*|\*)') {
            continue
        }

        foreach ($rule in $rules) {
            if ($line -match $rule.Pattern) {
                $relativePath = Get-RelativePath -Path $file.FullName
                $risk = if ($highRiskRules -contains $rule.Id -and $relativePath -like 'Client/Source/OdiroSim/*/Platform/Widget/*') {
                    'high'
                }
                else {
                    'review'
                }
                $findings.Add([pscustomobject]@{
                    Rule = $rule.Id
                    Risk = $risk
                    File = $relativePath
                    Line = $lineIndex + 1
                    Text = $trimmed
                    Note = $rule.Message
                })
            }
        }
    }
}

$baselineCounts = @{}
foreach ($finding in (Read-BaselineFindings -Path $BaselineFile)) {
    $fingerprint = Get-FindingFingerprint -Finding $finding
    if (-not $baselineCounts.ContainsKey($fingerprint)) {
        $baselineCounts[$fingerprint] = 0
    }
    $baselineCounts[$fingerprint]++
}

$newFindings = [System.Collections.Generic.List[object]]::new()
foreach ($finding in $findings) {
    $fingerprint = Get-FindingFingerprint -Finding $finding
    if ($baselineCounts.ContainsKey($fingerprint) -and $baselineCounts[$fingerprint] -gt 0) {
        $baselineCounts[$fingerprint]--
        continue
    }
    $newFindings.Add($finding)
}

$failRuleSet = ConvertTo-RuleSet -RuleIds $FailOnRules
$gateFindings = @(
    if ($failRuleSet.Count -gt 0) {
        foreach ($finding in $newFindings) {
            if ($failRuleSet.Contains($finding.Rule)) {
                $finding
            }
        }
    }
)

Write-Host 'UE5 widget boundary scan'
Write-Host "Root: $Root"
Write-Host "Scanned files: $($files.Count)"
Write-Host "Findings: $($findings.Count)"
if ($ChangedOnly) {
    Write-Host 'Scope: changed files only'
}
if (-not [string]::IsNullOrWhiteSpace($BaselineFile)) {
    Write-Host "New findings after baseline: $($newFindings.Count)"
}
if ($failRuleSet.Count -gt 0) {
    Write-Host "Gate rules: $(@($failRuleSet) -join ', ')"
    Write-Host "Gate findings: $($gateFindings.Count)"
}
Write-Host ''

if ($findings.Count -eq 0) {
    Write-Host 'No suspicious widget boundary patterns found.'
    exit 0
}

foreach ($finding in ($findings | Sort-Object File, Line, Rule)) {
    Write-Host ("{0}:{1}: [{2}/{3}] {4}" -f $finding.File, $finding.Line, $finding.Rule, $finding.Risk, $finding.Text)
}

Write-Host ''
Write-Host 'Notes:'
foreach ($rule in $rules) {
    Write-Host "- $($rule.Id): $($rule.Message)"
}
Write-Host ''
Write-Host 'Findings are review prompts and may be false positives. No files were modified.'

if ($FailOnFindings) {
    exit 1
}

if ($gateFindings.Count -gt 0) {
    Write-Host ''
    Write-Host 'Gate failure: new or unbaselined findings matched -FailOnRules.'
    exit 1
}

exit 0
