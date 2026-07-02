# Renders icon SVG files into the repository PNG icon size set.
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $IconName = @(),
    [switch] $SkipInstall
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Returns ImageMagick's magick executable path, installing it with winget when requested.
function Resolve-ImageMagickPath {
    param([switch] $SkipInstall)

    $command = Get-Command "magick" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    if ($SkipInstall) {
        throw "ImageMagick 'magick' command was not found on PATH."
    }

    $winget = Get-Command "winget" -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "ImageMagick is missing and winget is not available. Install ImageMagick or rerun after adding magick to PATH."
    }

    Write-Host "ImageMagick not found. Attempting winget install: ImageMagick.ImageMagick"
    & $winget.Source install --id ImageMagick.ImageMagick --exact --source winget --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) {
        throw "winget failed to install ImageMagick. Exit code: $LASTEXITCODE"
    }

    $command = Get-Command "magick" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidateRoots = @(
        Join-Path $env:ProgramFiles "ImageMagick-*",
        Join-Path ${env:ProgramFiles(x86)} "ImageMagick-*",
        Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\ImageMagick.ImageMagick_*"
    )

    foreach ($root in $candidateRoots) {
        $candidate = @(Get-ChildItem -Path $root -Filter "magick.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1)
        if ($candidate.Count -gt 0) {
            return $candidate[0].FullName
        }
    }

    throw "ImageMagick installed, but magick.exe was not found. Restart the shell or add ImageMagick to PATH."
}

# Returns the SVG files selected for rendering.
function Get-IconSvgFiles {
    param(
        [string] $SvgDirectory,
        [string[]] $IconName
    )

    $normalizedNames = @(
        foreach ($name in $IconName) {
            foreach ($part in ([string] $name).Split(",")) {
                $trimmed = $part.Trim()
                if ($trimmed) {
                    $trimmed
                }
            }
        }
    )

    if ($normalizedNames.Count -eq 0) {
        return @(Get-ChildItem -LiteralPath $SvgDirectory -Filter "*.svg" -File | Sort-Object Name)
    }

    $files = @()
    foreach ($name in $normalizedNames) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($name)
        $path = Join-Path $SvgDirectory "$baseName.svg"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Icon SVG not found: $path"
        }
        $files += Get-Item -LiteralPath $path
    }
    return $files
}

# Renders one SVG to 24px, 48px, 96px, and 84px-crop PNG outputs.
function Invoke-IconPngRender {
    param(
        [string] $MagickPath,
        [System.IO.FileInfo] $SvgFile,
        [string] $PngDirectory
    )

    $name = [System.IO.Path]::GetFileNameWithoutExtension($SvgFile.Name)
    $sizeDirectories = @{
        "24px" = 24
        "48px" = 48
        "96px" = 96
    }

    foreach ($entry in $sizeDirectories.GetEnumerator()) {
        $directory = Join-Path $PngDirectory $entry.Key
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $output = Join-Path $directory "$name.png"
        & $MagickPath -background none -size "$($entry.Value)x$($entry.Value)" $SvgFile.FullName "PNG32:$output"
        if ($LASTEXITCODE -ne 0) {
            throw "ImageMagick failed to render $($SvgFile.FullName) to $output."
        }
    }

    $cropDirectory = Join-Path $PngDirectory "84px-crop"
    New-Item -ItemType Directory -Path $cropDirectory -Force | Out-Null
    $source96 = Join-Path (Join-Path $PngDirectory "96px") "$name.png"
    $output84 = Join-Path $cropDirectory "$name.png"
    & $MagickPath $source96 -crop "84x84+6+6" +repage "PNG32:$output84"
    if ($LASTEXITCODE -ne 0) {
        throw "ImageMagick failed to crop $source96 to $output84."
    }
}

$iconRoot = $PSScriptRoot
$svgDirectory = Join-Path $iconRoot "svg"
$pngDirectory = Join-Path $iconRoot "png"
$magickPath = Resolve-ImageMagickPath -SkipInstall:$SkipInstall
$svgFiles = @(Get-IconSvgFiles -SvgDirectory $svgDirectory -IconName $IconName)

if ($svgFiles.Count -eq 0) {
    throw "No SVG files found in $svgDirectory."
}

foreach ($svgFile in $svgFiles) {
    Invoke-IconPngRender -MagickPath $magickPath -SvgFile $svgFile -PngDirectory $pngDirectory
    Write-Host "Rendered $($svgFile.BaseName)"
}
