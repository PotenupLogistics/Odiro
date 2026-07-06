# Syncs Agents Python development dependencies with uv.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\common.ps1"
Set-ToolPrefix "install/agents"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$agentsDir = Get-AgentsRoot

# Returns whether a setup environment flag is enabled.
function Test-SetupFlag {
    param([string] $Name)
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $false
    }
    return $value -match '^(1|true|yes|on)$'
}

# Loads local Agents .env values into the current setup process without overriding existing environment values.
function Import-AgentsDotEnv {
    param([string] $AgentsDir)
    $envPath = Join-Path $AgentsDir ".env"
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        return
    }

    foreach ($line in Get-Content -LiteralPath $envPath) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        $match = [regex]::Match($line, '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$')
        if (-not $match.Success) {
            continue
        }

        $name = $match.Groups[1].Value
        $value = $match.Groups[2].Value
        if (
            ($value.Length -ge 2) -and
            (($value.StartsWith('"') -and $value.EndsWith('"')) -or ($value.StartsWith("'") -and $value.EndsWith("'")))
        ) {
            $value = $value.Substring(1, $value.Length - 2)
        }

        if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name, "Process"))) {
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }
}

# Clears handled native-command failures before returning to root setup wrappers.
function Clear-NonfatalLastExitCode {
    $global:LASTEXITCODE = 0
}

# Runs the PDF RAG index freshness check and returns the script exit code.
function Invoke-PdfRagIndexCheck {
    param([string] $AgentsDir)
    Push-Location $AgentsDir
    try {
        $output = & uv run python scripts/build_pdf_rag_index.py --check-only 2>&1
        $exitCode = $LASTEXITCODE
        foreach ($line in $output) {
            Write-Host $line
        }
        return $exitCode
    }
    finally {
        Pop-Location
    }
}

# Builds the local PDF RAG Chroma index and returns the script exit code.
function Invoke-PdfRagIndexBuild {
    param([string] $AgentsDir)
    Push-Location $AgentsDir
    try {
        $output = & uv run python scripts/build_pdf_rag_index.py 2>&1
        $exitCode = $LASTEXITCODE
        foreach ($line in $output) {
            Write-Host $line
        }
        return $exitCode
    }
    finally {
        Pop-Location
    }
}

# Prepares the local PDF RAG Chroma index without blocking default setup on missing keys.
function Invoke-PdfRagIndexSetup {
    param([string] $AgentsDir)
    if (Test-SetupFlag -Name "ODIRO_SKIP_PDF_RAG_INDEX") {
        Write-WarningMessage "[PDF RAG] Skipping PDF RAG Chroma index setup because ODIRO_SKIP_PDF_RAG_INDEX is set."
        Clear-NonfatalLastExitCode
        return
    }

    $strict = Test-SetupFlag -Name "ODIRO_REQUIRE_PDF_RAG_INDEX"
    $checkExitCode = Invoke-PdfRagIndexCheck -AgentsDir $AgentsDir
    if ($checkExitCode -eq 0) {
        return
    }

    $buildRequiredExitCodes = @(10, 11, 12, 13)
    if ($buildRequiredExitCodes -notcontains $checkExitCode) {
        $message = "[PDF RAG] Chroma index check failed with exit code $checkExitCode."
        if ($strict) {
            throw $message
        }
        Write-WarningMessage $message
        Write-WarningMessage "[PDF RAG] Setup will continue. Vector RAG will be unavailable until the index is built."
        Clear-NonfatalLastExitCode
        return
    }

    if ([string]::IsNullOrWhiteSpace($env:OPENAI_API_KEY)) {
        Write-WarningMessage "[PDF RAG] Chroma index missing, but OPENAI_API_KEY is not set."
        Write-WarningMessage "[PDF RAG] Setup will continue. Vector RAG will be unavailable until the index is built."
        Write-Step "[PDF RAG] To build later: cd Agents; uv run python scripts/build_pdf_rag_index.py"
        if ($strict) {
            throw "[PDF RAG] PDF RAG Chroma index is required, but OPENAI_API_KEY is not set."
        }
        Clear-NonfatalLastExitCode
        return
    }

    Write-Step "[PDF RAG] Chroma index missing. Building local Chroma index..."
    $buildExitCode = Invoke-PdfRagIndexBuild -AgentsDir $AgentsDir
    if ($buildExitCode -eq 0) {
        Write-Success "[PDF RAG] Chroma index build completed."
        return
    }

    $message = "[PDF RAG] Chroma index build failed with exit code $buildExitCode."
    if ($strict) {
        throw $message
    }
    Write-WarningMessage $message
    Write-WarningMessage "[PDF RAG] Setup will continue. Vector RAG will be unavailable until the index is built."
    Write-Step "[PDF RAG] To build later: cd Agents; uv run python scripts/build_pdf_rag_index.py"
    Clear-NonfatalLastExitCode
}

if (-not (Test-Path -LiteralPath (Join-Path $agentsDir "pyproject.toml") -PathType Leaf)) {
    throw "Agents pyproject.toml not found: $agentsDir"
}

Assert-Command "uv"

$dependenciesCurrent = $false
Push-Location $agentsDir
try {
    & uv sync --check --dev --quiet
    if ($LASTEXITCODE -eq 0) {
        $dependenciesCurrent = $true
    }
}
finally {
    Pop-Location
}

if (-not $dependenciesCurrent) {
    Write-Step "Sync Agents development dependencies"
    Invoke-External -WorkingDirectory $agentsDir -FilePath "uv" -Arguments @("sync", "--dev")
}

Import-AgentsDotEnv -AgentsDir $agentsDir
Invoke-PdfRagIndexSetup -AgentsDir $agentsDir

Write-Success "Agents install phase complete."
