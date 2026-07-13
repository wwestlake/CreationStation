param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$sdkRoot = $env:WindowsSdkDir
if (-not $sdkRoot) {
    $sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
}

function Get-LatestVersionDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    if (-not (Test-Path $Root)) {
        return $null
    }

    Get-ChildItem $Root -Directory |
        Where-Object {
            $name = $_.Name
            $version = $null
            [version]::TryParse($name, [ref]$version)
        } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
}

function Get-BestSdkVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SdkRoot
    )

    $includeRoot = Join-Path $SdkRoot "Include"
    $libRoot = Join-Path $SdkRoot "Lib"

    if (-not (Test-Path $includeRoot) -or -not (Test-Path $libRoot)) {
        return $null
    }

    $candidates = Get-ChildItem $includeRoot -Directory |
        Where-Object {
            $version = $null
            [version]::TryParse($_.Name, [ref]$version)
        } |
        Sort-Object { [version]$_.Name } -Descending

    foreach ($candidate in $candidates) {
        $versionName = $candidate.Name
        $requiredPaths = @(
            (Join-Path $includeRoot "$versionName\km"),
            (Join-Path $includeRoot "$versionName\km\crt"),
            (Join-Path $includeRoot "$versionName\shared"),
            (Join-Path $libRoot "$versionName\km\x64")
        )

        if (($requiredPaths | Where-Object { -not (Test-Path $_) }).Count -eq 0) {
            return $versionName
        }
    }

    return $null
}

$sdkVersion = $env:WindowsSdkVersion
if ($sdkVersion) {
    $sdkVersion = $sdkVersion.TrimEnd('\')
}
else {
    $bestSdkVersion = Get-BestSdkVersion -SdkRoot $sdkRoot
    if ($bestSdkVersion) {
        $sdkVersion = $bestSdkVersion
    }
    else {
        $sdkVersion = "10.0.19041.0"
    }
}

$wdfVersion = $null
$latestWdf = Get-LatestVersionDirectory (Join-Path $sdkRoot "Include\wdf\kmdf")
if ($latestWdf) {
    $wdfVersion = $latestWdf.Name
}
else {
    $wdfVersion = "1.31"
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceRoot = Join-Path $repoRoot "Driver\Source"

$msvcRoot = $null
if ($env:VCToolsInstallDir -and (Test-Path $env:VCToolsInstallDir)) {
    $msvcRoot = Split-Path (Resolve-Path $env:VCToolsInstallDir).Path -Parent
}
elseif ($env:VSINSTALLDIR) {
    $candidate = Join-Path $env:VSINSTALLDIR "VC\Tools\MSVC"
    if (Test-Path $candidate) {
        $msvcRoot = $candidate
    }
}
else {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstallDir = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstallDir) {
            $candidate = Join-Path $vsInstallDir "VC\Tools\MSVC"
            if (Test-Path $candidate) {
                $msvcRoot = $candidate
            }
        }
    }
}

if (-not $msvcRoot) {
    $fallbackRoots = @(
        "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
    )

    foreach ($fallbackRoot in $fallbackRoots) {
        if (Test-Path $fallbackRoot) {
            $msvcRoot = $fallbackRoot
            break
        }
    }
}

if (-not $msvcRoot) {
    throw "Could not find an installed MSVC toolset."
}

$latestMsvc = Get-ChildItem $msvcRoot -Directory |
    Where-Object {
        $version = $null
        [version]::TryParse($_.Name, [ref]$version)
    } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1
if (-not $latestMsvc) {
    throw "Could not find an installed MSVC toolset under $msvcRoot."
}

$cl = Join-Path $latestMsvc.FullName "bin\Hostx64\x64\cl.exe"
$link = Join-Path $latestMsvc.FullName "bin\Hostx64\x64\link.exe"
if (-not (Test-Path $cl)) { throw "cl.exe not found at $cl" }
if (-not (Test-Path $link)) { throw "link.exe not found at $link" }

$includePaths = @(
    "$sdkRoot\Include\$sdkVersion\km\crt",
    "$sdkRoot\Include\$sdkVersion\km",
    "$sdkRoot\Include\$sdkVersion\shared",
    "$sdkRoot\Include\wdf\kmdf\$wdfVersion"
)

$libRoot = "$sdkRoot\Lib\$sdkVersion\km\x64"
$wdfLibRoot = "$sdkRoot\Lib\wdf\kmdf\x64\$wdfVersion"

foreach ($path in @($sdkRoot, $libRoot, $wdfLibRoot)) {
    if (-not (Test-Path $path)) {
        throw "Missing required Windows SDK/WDK path: $path"
    }
}

$outDir = Join-Path $PSScriptRoot "out\$Configuration\$Platform"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$clRsp = Join-Path $outDir "cl.rsp"
$linkRsp = Join-Path $outDir "link.rsp"

$compileLines = @(
    "/nologo"
    "/c"
    "/TC"
    "/EHsc-"
    "/GR-"
    "/GS"
    "/Zl"
    "/W4"
    "/kernel"
    "/Fo""$outDir\\"""
    "/Fd""$outDir\DjehutiRouterDriver.pdb"""
    "/D_AMD64_"
    "/D_WIN64"
    "/DWIN64"
    "/DNTDDI_VERSION=0x0A000008"
    "/DWINVER=0x0A00"
    "/DKMDF_VERSION_MINOR=31"
    "/DKMDF_MINIMUM_VERSION_REQUIRED=31"
)

foreach ($includePath in $includePaths) {
    $compileLines += "/I""$includePath"""
}

$compileLines += @(
    """$sourceRoot\DriverEntry.cpp"""
    """$sourceRoot\DjehutiRouterDriver.cpp"""
)

$compileLines | Set-Content -Path $clRsp -Encoding ASCII

& $cl "@$clRsp"
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed."
}

$objectFiles = @(
    (Join-Path $outDir "DriverEntry.obj")
    (Join-Path $outDir "DjehutiRouterDriver.obj")
)

$linkLines = @(
    "/nologo"
    "/driver"
    "/subsystem:native"
    "/machine:x64"
    "/entry:FxDriverEntry"
    "/nodefaultlib"
    "/debug"
    "/incremental:no"
    "/out:""$outDir\DjehutiRouterDriver.sys"""
    "/pdb:""$outDir\DjehutiRouterDriver.pdb"""
    """$wdfLibRoot\WdfLdr.lib"""
    """$wdfLibRoot\WdfDriverEntry.lib"""
    """$libRoot\ntoskrnl.lib"""
    """$libRoot\hal.lib"""
    """$libRoot\wmilib.lib"""
    """$libRoot\BufferOverflowFastFailK.lib"""
)

$objectFiles | ForEach-Object { $linkLines += """$_""" }
$linkLines | Set-Content -Path $linkRsp -Encoding ASCII

& $link "@$linkRsp"
if ($LASTEXITCODE -ne 0) {
    throw "Link failed."
}

Write-Host "Built driver: $outDir\DjehutiRouterDriver.sys"
