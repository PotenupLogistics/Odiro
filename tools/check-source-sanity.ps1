# Checks source code sanity for staged local changes and pull request diffs.
param(
    [ValidateSet("manual", "pre-commit", "pre-merge-commit", "pr-check")]
    [string] $Hook = "manual",
    [switch] $Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "check/source-sanity"

$script:ReadStagedFilesFromWorktree = $false

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

# Runs Git and returns output lines, failing loudly on non-zero exit codes.
function Invoke-GitLines {
    param([Parameter(Mandatory = $true)][string[]] $Arguments)

    $output = @(& git @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join " ") failed with exit code $LASTEXITCODE."
    }

    return @($output | ForEach-Object { ([string] $_).TrimEnd("`r") })
}

# Returns the current checked-out branch name, or an empty string for detached HEAD.
function Get-CurrentBranchName {
    $branch = @(Invoke-GitLines -Arguments @("branch", "--show-current"))
    if ($branch.Count -eq 0) {
        return ""
    }

    return ([string] $branch[0]).Trim()
}

# Returns staged paths that may be part of the next commit.
function Get-StagedChangedPaths {
    return @(Invoke-GitLines -Arguments @("diff", "--cached", "--name-only", "--diff-filter=ACMRT", "--"))
}

# Returns whether a path belongs to Bridge code that needs Go build validation.
function Test-BridgeBuildPath {
    param([Parameter(Mandatory = $true)][string] $Path)

    return $Path -match '^Bridge/.*\.go$' -or $Path -eq 'Bridge/go.mod' -or $Path -eq 'Bridge/go.sum'
}

# Returns whether a path belongs to Client C++ implementation code.
function Test-ClientImplementationPath {
    param([Parameter(Mandatory = $true)][string] $Path)

    return $Path -match '^Client/.*\.(c|cc|cpp|cxx)$'
}

# Returns whether a path belongs to Client C++ header-like code.
function Test-ClientHeaderPath {
    param([Parameter(Mandatory = $true)][string] $Path)

    return $Path -match '^Client/.*\.(h|hh|hpp|inl)$'
}

# Exports an index path prefix to a temporary directory for staged-only checks.
function Export-IndexPathPrefix {
    param(
        [Parameter(Mandatory = $true)][string] $PrefixPath,
        [Parameter(Mandatory = $true)][string] $Pathspec
    )

    $trackedPaths = @(Invoke-GitLines -Arguments @("ls-files", "--", $Pathspec))
    if ($trackedPaths.Count -eq 0) {
        throw "No tracked index paths found for $Pathspec."
    }

    $prefix = ($PrefixPath -replace '\\', '/')
    if (-not $prefix.EndsWith("/")) {
        $prefix = "$prefix/"
    }

    $checkoutArguments = @("checkout-index", "--force", "--prefix=$prefix", "--") + $trackedPaths
    & git @checkoutArguments
    if ($LASTEXITCODE -ne 0) {
        throw "git checkout-index failed for $Pathspec."
    }
}

# Removes only the temporary directories created by this script.
function Remove-StagedVerificationTempDirectory {
    param([Parameter(Mandatory = $true)][string] $Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $resolved) {
        return
    }

    $resolvedPath = $resolved.Path
    $tempRoot = [System.IO.Path]::GetTempPath()
    $leaf = Split-Path -Leaf $resolvedPath
    if (-not $resolvedPath.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not $leaf.StartsWith("odiro-staged-", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to delete unexpected temporary directory: $resolvedPath"
    }

    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
}

# Runs go build against a staged Bridge snapshot without writing build output into the repo.
function Invoke-BridgeBuildCheck {
    Assert-Command "go"

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "odiro-staged-$([guid]::NewGuid().ToString("N"))"
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

    try {
        Export-IndexPathPrefix -PrefixPath $tempRoot -Pathspec "Bridge"
        $bridgeRoot = Join-Path $tempRoot "Bridge"
        $outputPath = Join-Path $tempRoot "OdiroHost.commitcheck.exe"

        Write-Step "Run staged Bridge go build"
        Push-Location $bridgeRoot
        try {
            & go build -o $outputPath ./cmd/odirohost
            if ($LASTEXITCODE -ne 0) {
                throw "go build failed with exit code $LASTEXITCODE."
            }
        }
        finally {
            Pop-Location
        }
    }
    finally {
        Remove-StagedVerificationTempDirectory -Path $tempRoot
    }
}

# Returns tracked Client C++ implementation files from the index.
function Get-ClientIndexCppFiles {
    $paths = @(Invoke-GitLines -Arguments @("ls-files", "--", "Client"))
    return @($paths | Where-Object { $_ -match '^Client/.*\.(c|cc|cpp|cxx)$' })
}

# Reads text lines for one staged file, using the worktree only after CI has staged the PR merge result.
function Read-IndexFileLines {
    param([Parameter(Mandatory = $true)][string] $Path)

    if ($script:ReadStagedFilesFromWorktree -and (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return @(Get-Content -LiteralPath $Path)
    }

    $content = @(& git show ":$Path")
    if ($LASTEXITCODE -ne 0) {
        throw "git show :$Path failed with exit code $LASTEXITCODE."
    }

    return $content
}

# Removes simple line comments before the brace and declaration heuristics run.
function Remove-LineComment {
    param([AllowNull()][AllowEmptyString()][string] $Line)

    if ($null -eq $Line) {
        return ""
    }

    return ($Line -replace '//.*$', '')
}

# Returns whether a stripped line is a merge conflict marker.
function Test-ConflictMarkerLine {
    param([AllowNull()][AllowEmptyString()][string] $Line)

    if ($null -eq $Line) {
        return $false
    }

    return $Line -match '^\s*(<<<<<<<|=======|>>>>>>>)'
}

# Counts one character in a line after comments have been stripped.
function Get-CharacterCount {
    param(
        [AllowNull()][AllowEmptyString()][string] $Line,
        [Parameter(Mandatory = $true)][char] $Character
    )

    if ([string]::IsNullOrEmpty($Line)) {
        return 0
    }

    return ([regex]::Matches($Line, [regex]::Escape([string] $Character))).Count
}

# Returns a coarse namespace-scope function match for common Unreal helper definitions.
function Match-NamespaceFunctionDefinition {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string] $Line,
        [AllowNull()][AllowEmptyString()][string] $NextLine
    )

    $trimmed = (Remove-LineComment -Line $Line).Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return $null
    }

    if ($trimmed -match '^(#|if\b|for\b|while\b|switch\b|catch\b|else\b|return\b|using\b|namespace\b|class\b|struct\b|enum\b|UE_LOG\b|IMPLEMENT_|DEFINE_)') {
        return $null
    }

    if ($trimmed.Contains("::") -or $trimmed.Contains(";") -or $trimmed.Contains("=")) {
        return $null
    }

    $candidate = $trimmed
    if (-not $candidate.Contains("{") -and -not [string]::IsNullOrWhiteSpace($NextLine)) {
        $nextTrimmed = (Remove-LineComment -Line $NextLine).Trim()
        if ($nextTrimmed -eq "{") {
            $candidate = "$candidate {"
        }
    }

    $pattern = '^(?<prefix>(?:(?:static|inline|constexpr|FORCEINLINE|FORCEINLINE_DEBUGGABLE)\s+)*)(?<return>[~A-Za-z_][\w:<>,\s*&]*?)\s+(?<name>[A-Za-z_]\w*)\s*\((?<params>[^;{}]*)\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[^{]+)?\s*\{'
    $match = [regex]::Match($candidate, $pattern)
    if (-not $match.Success) {
        return $null
    }

    return $match
}

# Returns whether a function definition prefix is explicitly header-local or inline.
function Test-InlineOrInternalFunctionPrefix {
    param([AllowNull()][AllowEmptyString()][string] $Prefix)

    if ([string]::IsNullOrWhiteSpace($Prefix)) {
        return $false
    }

    return $Prefix -match '(^|\s)(static|inline|constexpr|FORCEINLINE|FORCEINLINE_DEBUGGABLE)(\s|$)'
}

# Returns whether a header line should be ignored by function declaration heuristics.
function Test-IgnoredHeaderDeclarationLine {
    param([AllowNull()][AllowEmptyString()][string] $Line)

    if ([string]::IsNullOrWhiteSpace($Line)) {
        return $true
    }

    return $Line -match '^(#|//|/\*|\*|public:|protected:|private:|U[A-Z_]*\b|GENERATED_|DECLARE_|IMPLEMENT_|DEFINE_|using\b|typedef\b|enum\b|class\b|struct\b|namespace\b)'
}

# Normalizes a one-line or joined multiline function declaration for duplicate checks.
function Get-HeaderFunctionDeclarationKey {
    param([AllowNull()][AllowEmptyString()][string] $Declaration)

    if ([string]::IsNullOrWhiteSpace($Declaration)) {
        return $null
    }

    $normalized = $Declaration.Trim()
    $normalized = $normalized -replace '\s+', ' '
    $normalized = $normalized -replace '\s*([(),;&*<>:=])\s*', '$1'

    if (-not $normalized.EndsWith(";")) {
        return $null
    }

    if ($normalized -match '^(DECLARE_|IMPLEMENT_|DEFINE_|UE_|UPROPERTY|UFUNCTION|UCLASS|USTRUCT|UENUM)\b') {
        return $null
    }

    if ($normalized -notmatch '\(' -or $normalized -notmatch '\)') {
        return $null
    }

    if ($normalized -match '^(if|for|while|switch|return|sizeof|static_assert)\b') {
        return $null
    }

    $functionPattern = '^(?:(?:virtual|static|inline|constexpr|explicit|friend|FORCEINLINE|FORCEINLINE_DEBUGGABLE)\s+)*(?:[~A-Za-z_][\w:<>,\s*&]+\s+)?~?[A-Za-z_]\w*\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?(?:final\s*)?(?:noexcept\s*)?(?:=\s*(?:0|default|delete)\s*)?;$'
    if ($normalized -notmatch $functionPattern) {
        return $null
    }

    return $normalized
}

# Returns the current C++ declaration scope name for diagnostics and duplicate keys.
function Get-ScopeKey {
    param([Parameter(Mandatory = $true)] $ScopeStack)

    $names = @($ScopeStack | ForEach-Object { $_.Name } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($names.Count -eq 0) {
        return "<global>"
    }

    return ($names -join "::")
}

# Adds a pending class or struct scope when its opening brace appears on the next line.
function Add-PendingScopeIfOpened {
    param(
        [Parameter(Mandatory = $true)] $ScopeStack,
        [AllowNull()] $PendingScope,
        [Parameter(Mandatory = $true)][int] $BraceDepth,
        [AllowNull()][AllowEmptyString()][string] $Line
    )

    if ($null -eq $PendingScope) {
        return $null
    }

    if ($Line.Trim() -eq "{") {
        $ScopeStack.Add([pscustomobject]@{
            Depth = $BraceDepth + 1
            Name = $PendingScope.Name
            Kind = $PendingScope.Kind
        }) | Out-Null
        return $null
    }

    return $PendingScope
}

# Validates staged Client headers for obvious merge and declaration accidents.
function Test-ClientHeaderSanity {
    param([Parameter(Mandatory = $true)][string[]] $ChangedHeaderPaths)

    $issues = New-Object System.Collections.Generic.List[object]

    foreach ($path in $ChangedHeaderPaths) {
        $lines = @(Read-IndexFileLines -Path $path)
        $braceDepth = 0
        $scopeStack = New-Object System.Collections.Generic.List[object]
        $seenDeclarations = @{}
        $pendingScope = $null
        $declarationBuffer = New-Object System.Collections.Generic.List[string]
        $declarationStartLine = 0

        for ($index = 0; $index -lt $lines.Count; $index++) {
            while ($scopeStack.Count -gt 0 -and $scopeStack[$scopeStack.Count - 1].Depth -gt $braceDepth) {
                $scopeStack.RemoveAt($scopeStack.Count - 1)
            }

            $line = [string] $lines[$index]
            $stripped = Remove-LineComment -Line $line
            $trimmed = $stripped.Trim()
            $lineNumber = $index + 1

            if (Test-ConflictMarkerLine -Line $line) {
                $issues.Add([pscustomobject]@{
                    Path = $path
                    Line = $lineNumber
                    Message = "merge conflict marker remains"
                }) | Out-Null
            }

            $pendingScope = Add-PendingScopeIfOpened -ScopeStack $scopeStack -PendingScope $pendingScope -BraceDepth $braceDepth -Line $trimmed

            $insideTypeScope = @($scopeStack | Where-Object { $_.Kind -eq "class" -or $_.Kind -eq "struct" }).Count -gt 0
            $namespaceDepths = @($scopeStack | Where-Object { $_.Kind -eq "namespace" } | ForEach-Object { $_.Depth })
            $isNamespaceScope = $braceDepth -eq 0 -or $namespaceDepths -contains $braceDepth

            if ($isNamespaceScope) {
                $match = Match-NamespaceFunctionDefinition -Line $line -NextLine $(if ($index + 1 -lt $lines.Count) { [string] $lines[$index + 1] } else { $null })
                if ($null -ne $match -and -not (Test-InlineOrInternalFunctionPrefix -Prefix ([string] $match.Groups["prefix"].Value))) {
                    $issues.Add([pscustomobject]@{
                        Path = $path
                        Line = $lineNumber
                        Message = "non-inline namespace-scope function body in header"
                    }) | Out-Null
                }
            }

            if ($insideTypeScope -and -not (Test-IgnoredHeaderDeclarationLine -Line $trimmed)) {
                if ($declarationBuffer.Count -eq 0 -and $trimmed.Contains("(")) {
                    $declarationStartLine = $lineNumber
                }

                if ($declarationBuffer.Count -gt 0 -or ($trimmed.Contains("(") -and -not $trimmed.EndsWith("{"))) {
                    $declarationBuffer.Add($trimmed) | Out-Null
                }

                if ($declarationBuffer.Count -gt 0 -and $trimmed.EndsWith(";")) {
                    $declarationText = ($declarationBuffer -join " ")
                    $declarationKey = Get-HeaderFunctionDeclarationKey -Declaration $declarationText
                    if ($declarationKey) {
                        $scopeKey = Get-ScopeKey -ScopeStack $scopeStack
                        $key = "$scopeKey|$declarationKey"
                        if ($seenDeclarations.ContainsKey($key)) {
                            $issues.Add([pscustomobject]@{
                                Path = $path
                                Line = $declarationStartLine
                                Message = "duplicate function declaration in $scopeKey"
                                Detail = $declarationKey
                                FirstLine = $seenDeclarations[$key]
                            }) | Out-Null
                        }
                        else {
                            $seenDeclarations[$key] = $declarationStartLine
                        }
                    }

                    $declarationBuffer.Clear()
                    $declarationStartLine = 0
                }
                elseif ($declarationBuffer.Count -gt 0 -and ($trimmed.EndsWith("{") -or $trimmed.EndsWith("}"))) {
                    $declarationBuffer.Clear()
                    $declarationStartLine = 0
                }
            }
            else {
                $declarationBuffer.Clear()
                $declarationStartLine = 0
            }

            if ($trimmed -match '^namespace(?:\s+(?<name>[A-Za-z_][A-Za-z0-9_:]*))?\s*\{') {
                $name = [string] $Matches["name"]
                $scopeStack.Add([pscustomobject]@{
                    Depth = $braceDepth + 1
                    Name = $name
                    Kind = "namespace"
                }) | Out-Null
            }
            elseif ($trimmed -match '^(?<kind>class|struct)\s+(?:(?:[A-Za-z_][A-Za-z0-9_]*_API)\s+)?(?<name>[A-Za-z_][A-Za-z0-9_]*)\b') {
                $scope = [pscustomobject]@{
                    Name = [string] $Matches["name"]
                    Kind = [string] $Matches["kind"]
                }

                if ($trimmed.Contains("{")) {
                    $scopeStack.Add([pscustomobject]@{
                        Depth = $braceDepth + 1
                        Name = $scope.Name
                        Kind = $scope.Kind
                    }) | Out-Null
                    $pendingScope = $null
                }
                else {
                    $pendingScope = $scope
                }
            }

            $braceDepth += Get-CharacterCount -Line $stripped -Character '{'
            $braceDepth -= Get-CharacterCount -Line $stripped -Character '}'
            if ($braceDepth -lt 0) {
                $braceDepth = 0
            }
        }
    }

    if ($issues.Count -eq 0) {
        Write-Success "Client header sanity check passed."
        return
    }

    Write-ErrorMessage "Client header sanity check found risky staged content."
    foreach ($issue in $issues) {
        $detail = if ($issue.PSObject.Properties["Detail"]) { ": $($issue.Detail)" } else { "" }
        $firstLine = if ($issue.PSObject.Properties["FirstLine"]) { " first seen at line $($issue.FirstLine)" } else { "" }
        Write-ErrorMessage "- $($issue.Path):$($issue.Line) $($issue.Message)$detail$firstLine"
    }

    throw "Fix the staged Client header issue before committing."
}

# Produces a best-effort signature key to avoid blocking valid overloads unnecessarily.
function Get-FunctionSignatureKey {
    param(
        [Parameter(Mandatory = $true)][string] $Name,
        [AllowNull()][AllowEmptyString()][string] $Parameters
    )

    $params = ""
    if (-not [string]::IsNullOrWhiteSpace($Parameters)) {
        $params = $Parameters.Trim()
        $params = $params -replace '\s+', ' '
        $params = $params -replace '\s*=\s*[^,]+', ''
        $params = $params -replace '\s+[A-Za-z_][A-Za-z0-9_]*\s*(?=,|$)', ''
    }

    return "$Name($params)"
}

# Reads risky namespace-scope helpers from a C++ file in the index.
function Get-RiskyClientCppFunctions {
    param([Parameter(Mandatory = $true)][string] $Path)

    $lines = @(Read-IndexFileLines -Path $Path)
    $braceDepth = 0
    $namespaceStack = New-Object System.Collections.Generic.List[object]
    $definitions = New-Object System.Collections.Generic.List[object]

    for ($index = 0; $index -lt $lines.Count; $index++) {
        while ($namespaceStack.Count -gt 0 -and $namespaceStack[$namespaceStack.Count - 1].Depth -gt $braceDepth) {
            $namespaceStack.RemoveAt($namespaceStack.Count - 1)
        }

        $line = [string] $lines[$index]
        $stripped = Remove-LineComment -Line $line
        $trimmed = $stripped.Trim()
        $nextLine = $null
        if ($index + 1 -lt $lines.Count) {
            $nextLine = [string] $lines[$index + 1]
        }

        $insideAnonymousNamespace = @($namespaceStack | Where-Object { $_.Anonymous }).Count -gt 0
        $namedNamespacePath = (@($namespaceStack | Where-Object { -not $_.Anonymous } | ForEach-Object { $_.Name }) -join "::")
        $namespaceDepths = @($namespaceStack | ForEach-Object { $_.Depth })
        $isNamespaceScope = $braceDepth -eq 0 -or $namespaceDepths -contains $braceDepth

        if ($isNamespaceScope) {
            $match = Match-NamespaceFunctionDefinition -Line $line -NextLine $nextLine
            if ($null -ne $match) {
                $prefix = [string] $match.Groups["prefix"].Value
                $isStatic = $prefix -match '(^|\s)static(\s|$)'
                if ($insideAnonymousNamespace -or $isStatic) {
                    $name = [string] $match.Groups["name"].Value
                    $params = [string] $match.Groups["params"].Value
                    $signature = Get-FunctionSignatureKey -Name $name -Parameters $params
                    $scopeKey = $namedNamespacePath
                    $definitions.Add([pscustomobject]@{
                        Path = $Path
                        Line = $index + 1
                        Name = $name
                        Signature = $signature
                        ScopeKey = $scopeKey
                        Kind = if ($insideAnonymousNamespace) { "anonymous namespace" } else { "static namespace-scope" }
                        Key = "$scopeKey|$signature"
                    }) | Out-Null
                }
            }
        }

        if ($trimmed -match '^namespace(?:\s+(?<name>[A-Za-z_][A-Za-z0-9_:]*))?\s*\{') {
            $name = [string] $Matches["name"]
            $namespaceStack.Add([pscustomobject]@{
                Depth = $braceDepth + 1
                Name = $name
                Anonymous = [string]::IsNullOrWhiteSpace($name)
            }) | Out-Null
        }

        $braceDepth += Get-CharacterCount -Line $stripped -Character '{'
        $braceDepth -= Get-CharacterCount -Line $stripped -Character '}'
        if ($braceDepth -lt 0) {
            $braceDepth = 0
        }
    }

    return $definitions
}

# Returns staged Client C++ files containing any changed risky helper name.
function Get-ClientCppFilesContainingNames {
    param([Parameter(Mandatory = $true)][string[]] $Names)

    $uniqueNames = @(
        $Names |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    if ($uniqueNames.Count -eq 0) {
        return @()
    }

    $matches = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $chunkSize = 50
    for ($offset = 0; $offset -lt $uniqueNames.Count; $offset += $chunkSize) {
        $chunk = @($uniqueNames[$offset..([Math]::Min($offset + $chunkSize - 1, $uniqueNames.Count - 1))])
        $arguments = @("grep", "--cached", "-l", "-F")
        foreach ($name in $chunk) {
            $arguments += @("-e", $name)
        }
        $arguments += @("--", "Client")

        $output = @(& git @arguments)
        if ($LASTEXITCODE -eq 1) {
            continue
        }
        if ($LASTEXITCODE -ne 0) {
            throw "git $($arguments -join " ") failed with exit code $LASTEXITCODE."
        }

        foreach ($path in $output) {
            $normalized = ([string] $path).TrimEnd("`r")
            if (Test-ClientImplementationPath -Path $normalized) {
                [void] $matches.Add($normalized)
            }
        }
    }

    return @($matches | Sort-Object)
}

# Blocks UnityBuild-prone duplicate file-private helper definitions touched by staged Client C++ changes.
function Test-ClientUnityFunctionNames {
    param([Parameter(Mandatory = $true)][string[]] $ChangedCppPaths)

    $changedSet = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $ChangedCppPaths) {
        [void] $changedSet.Add($path)
    }

    $changedDefinitions = New-Object System.Collections.Generic.List[object]
    foreach ($path in $ChangedCppPaths) {
        foreach ($definition in Get-RiskyClientCppFunctions -Path $path) {
            $changedDefinitions.Add($definition) | Out-Null
        }
    }

    if ($changedDefinitions.Count -eq 0) {
        Write-Success "Client UnityBuild helper-name check passed."
        return
    }

    $changedKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $changedNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($definition in $changedDefinitions) {
        [void] $changedKeys.Add([string] $definition.Key)
        [void] $changedNames.Add([string] $definition.Name)
    }

    $definitions = New-Object System.Collections.Generic.List[object]
    foreach ($definition in $changedDefinitions) {
        $definitions.Add($definition) | Out-Null
    }

    $candidatePaths = @(
        Get-ClientCppFilesContainingNames -Names @($changedNames) |
            Where-Object { -not $changedSet.Contains($_) }
    )
    foreach ($path in $candidatePaths) {
        foreach ($definition in Get-RiskyClientCppFunctions -Path $path) {
            if ($changedKeys.Contains([string] $definition.Key)) {
                $definitions.Add($definition) | Out-Null
            }
        }
    }

    $duplicates = @(
        $definitions |
            Group-Object -Property Key |
            Where-Object {
                $_.Count -gt 1 -and
                @($_.Group | Where-Object { $changedSet.Contains($_.Path) }).Count -gt 0
            }
    )

    if ($duplicates.Count -eq 0) {
        Write-Success "Client UnityBuild helper-name check passed."
        return
    }

    Write-ErrorMessage "Client UnityBuild-prone duplicate helper definitions were found."
    foreach ($duplicate in $duplicates) {
        $first = $duplicate.Group[0]
        $scope = if ([string]::IsNullOrWhiteSpace($first.ScopeKey)) { "<global>" } else { $first.ScopeKey }
        Write-ErrorMessage "- $($first.Signature) in $scope"
        foreach ($item in $duplicate.Group) {
            Write-ErrorMessage "  $($item.Path):$($item.Line) ($($item.Kind))"
        }
    }

    throw "Rename the helper or move it into a file-specific named namespace before committing."
}

Assert-Command "git"

$script:ReadStagedFilesFromWorktree = $Hook -eq "pr-check" -and $env:GITHUB_ACTIONS -eq "true"

$branch = Get-CurrentBranchName
if (-not $Force -and $branch -ne "main") {
    Write-Step "Skipping source sanity check on branch '$branch'. Main branch commits and merge commits are gated."
    exit 0
}

$stagedPaths = @(Get-StagedChangedPaths)
if ($stagedPaths.Count -eq 0) {
    Write-Step "No staged source code changes to check."
    exit 0
}

$bridgePaths = @($stagedPaths | Where-Object { Test-BridgeBuildPath -Path $_ })
$clientImplementationPaths = @($stagedPaths | Where-Object { Test-ClientImplementationPath -Path $_ })
$clientHeaderPaths = @($stagedPaths | Where-Object { Test-ClientHeaderPath -Path $_ })

if ($clientHeaderPaths.Count -gt 0) {
    Write-Step "Check staged Client headers for merge accidents"
    Test-ClientHeaderSanity -ChangedHeaderPaths $clientHeaderPaths
}

if ($clientImplementationPaths.Count -gt 0) {
    Write-Step "Check staged Client C++ for UnityBuild helper collisions"
    Test-ClientUnityFunctionNames -ChangedCppPaths $clientImplementationPaths
}

if ($bridgePaths.Count -gt 0) {
    Invoke-BridgeBuildCheck
    Write-Success "Bridge staged go build passed."
}

Write-Success "Source code sanity check complete for $Hook."
