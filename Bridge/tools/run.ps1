# Runs the Bridge host.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot "common.ps1")
Set-ToolPrefix "run/bridge"

trap {
    Write-ErrorMessage $_.Exception.Message
    exit 1
}

$bridgeRoot = Get-BridgeRoot

Assert-Command "go"
Write-Step "Run Bridge host"
Push-Location $bridgeRoot
try {
    & go run ./cmd/odirohost @args
    if ($LASTEXITCODE -ne 0) {
        throw "Bridge host failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
