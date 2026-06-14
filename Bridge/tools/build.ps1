# Builds the Bridge host executable.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot "common.ps1")
Set-ToolPrefix "build/bridge"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$bridgeRoot = Get-BridgeRoot
$outputDir = Join-Path $bridgeRoot "bin"
$outputName = "OdiroHost"
if ([System.IO.Path]::DirectorySeparatorChar -eq "\") {
    $outputName = "$outputName.exe"
}

Assert-Command "go"
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Write-Step "Run Bridge tests"
Push-Location $bridgeRoot
try {
    & go test ./...
    if ($LASTEXITCODE -ne 0) {
        throw "go test failed with exit code $LASTEXITCODE."
    }

    Write-Step "Build Bridge host"
    & go build -o (Join-Path $outputDir $outputName) ./cmd/odirohost
    if ($LASTEXITCODE -ne 0) {
        throw "go build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Success "Bridge build complete."
