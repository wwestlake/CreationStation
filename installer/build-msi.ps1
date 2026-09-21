# Builds the Djehuti Station MSI with WiX v3: Station plus the shared storage service it cannot run without.
# Called by the release pipeline (.github/workflows/release.yml). Fails with a clear message on any problem.
#
#   -StageDir       the staged app: DjehutiStation.exe, README.md, LICENSE, EULA.txt
#   -VfsServiceExe  the built DjehutiSuiteVfsService.exe to install as the suite's shared service
#   -ServicesWxs    services\VfsService\installer\SuiteServices.wxs (the shared service fragment)
#   -Version        the product version, three numbers (for example 0.9.1)
#   -OutFile        the MSI to write
param(
    [Parameter(Mandatory)] [string] $StageDir,
    [Parameter(Mandatory)] [string] $VfsServiceExe,
    [Parameter(Mandatory)] [string] $ServicesWxs,
    [Parameter(Mandatory)] [string] $Version,
    [Parameter(Mandatory)] [string] $OutFile,
    [string] $EulaRtf = (Join-Path $PSScriptRoot "EULA.rtf"),
    [string] $WixBin
)

$ErrorActionPreference = "Stop"

if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "The version '$Version' must be three numbers, like 0.9.1." }
foreach ($required in @(
        (Join-Path $StageDir "DjehutiStation.exe"),
        (Join-Path $StageDir "README.md"),
        (Join-Path $StageDir "LICENSE"),
        (Join-Path $StageDir "EULA.txt"),
        $VfsServiceExe, $ServicesWxs, $EulaRtf)) {
    if (-not (Test-Path $required)) { throw "Cannot build the installer: '$required' is missing." }
}

if (-not $WixBin) {
    $WixBin = @("${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin", "${env:ProgramFiles(x86)}\WiX Toolset v3.11\bin") |
        Where-Object { Test-Path (Join-Path $_ "candle.exe") } | Select-Object -First 1
}
if (-not $WixBin) { throw "WiX Toolset v3 was not found." }

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("station-msi-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $work | Out-Null

function Invoke-Wix([string] $tool, [string[]] $arguments) {
    & (Join-Path $WixBin $tool) @arguments
    if ($LASTEXITCODE -ne 0) { throw "$tool failed with exit code $LASTEXITCODE." }
}

$stationWxs = Join-Path $PSScriptRoot "DjehutiStation.wxs"
Invoke-Wix "candle.exe" @(
    "-nologo", "-arch", "x64", "-ext", "WixUtilExtension",
    "-dVersion=$Version", "-dStageDir=$StageDir", "-dEulaRtf=$EulaRtf", "-dVfsServiceExe=$VfsServiceExe",
    "-out", "$work\", $stationWxs, $ServicesWxs)

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutFile) | Out-Null
Invoke-Wix "light.exe" @(
    "-nologo", "-ext", "WixUtilExtension", "-ext", "WixUIExtension", "-cultures:en-us",
    "-sice:ICE57",   # per-user shortcut key paths in a per-machine install: required by ICE38/ICE43, contradicted by ICE57
    "-out", $OutFile, "$work\DjehutiStation.wixobj", "$work\SuiteServices.wixobj")

Remove-Item -Recurse -Force $work
Write-Host "Built $OutFile"
