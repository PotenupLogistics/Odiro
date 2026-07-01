[CmdletBinding()]
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [string[]]$ScanPath = @(
        'Client/Source/OdiroSim/Public/Platform/Widget',
        'Client/Source/OdiroSim/Private/Platform/Widget'
    ),
    [string[]]$ExcludePath = @('*/Tests/*'),
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

$files = @(
    foreach ($file in $files) {
        $relativePath = [System.IO.Path]::GetRelativePath($Root, $file.FullName).Replace('\', '/')
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
                $relativePath = [System.IO.Path]::GetRelativePath($Root, $file.FullName).Replace('\', '/')
                $findings.Add([pscustomobject]@{
                    Rule = $rule.Id
                    File = $relativePath
                    Line = $lineIndex + 1
                    Text = $trimmed
                    Note = $rule.Message
                })
            }
        }
    }
}

Write-Host 'UE5 widget boundary scan'
Write-Host "Root: $Root"
Write-Host "Scanned files: $($files.Count)"
Write-Host "Findings: $($findings.Count)"
Write-Host ''

if ($findings.Count -eq 0) {
    Write-Host 'No suspicious widget boundary patterns found.'
    exit 0
}

foreach ($finding in ($findings | Sort-Object File, Line, Rule)) {
    Write-Host ("{0}:{1}: [{2}] {3}" -f $finding.File, $finding.Line, $finding.Rule, $finding.Text)
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

exit 0
