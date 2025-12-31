[CmdletBinding()]
param(
  [Parameter(Mandatory = $false)]
  [string]$E2StudioExe = $env:E2STUDIO_EXE,

  [Parameter(Mandatory = $false)]
  [string]$ProjectName = "EMIC_LORA_PROTOCOL",

  [Parameter(Mandatory = $false)]
  [string]$Config = "HardwareDebug",

  [Parameter(Mandatory = $false)]
  [switch]$Clean,

  [Parameter(Mandatory = $false)]
  [string]$WorkspaceDir = ".e2studio_headless_ws"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-WorkspaceRoot {
  $scriptDir = Split-Path -Parent $PSCommandPath
  return (Resolve-Path (Join-Path $scriptDir ".." )).Path
}

$root = Resolve-WorkspaceRoot

if (-not $E2StudioExe) {
  throw @(
    "Chưa set đường dẫn e2studio.exe.",
    "Hãy set env var E2STUDIO_EXE trỏ tới e2studio.exe (ví dụ):",
    "  setx E2STUDIO_EXE \"C:\\Renesas\\e2_studio\\e2_studio.exe\"",
    "Sau đó mở lại VS Code."
  ) -join "`n"
}

if (-not (Test-Path $E2StudioExe)) {
  throw "Không tìm thấy e2studio.exe tại: $E2StudioExe"
}

$wsPath = Join-Path $root $WorkspaceDir
New-Item -ItemType Directory -Force -Path $wsPath | Out-Null

$buildTarget = "$ProjectName/$Config"

# Notes:
# -import imports the project into a headless Eclipse workspace.
# Some Renesas builders may require extra plugins; if this fails, use the makefile task.

$commonArgs = @(
  "-nosplash",
  "-application", "org.eclipse.cdt.managedbuilder.core.headlessbuild",
  "-data", $wsPath,
  "-import", $root
)

if ($Clean) {
  Write-Host "[e2studio-headless] cleanBuild $buildTarget"
  & $E2StudioExe @commonArgs "-cleanBuild" $buildTarget
} else {
  Write-Host "[e2studio-headless] build $buildTarget"
  & $E2StudioExe @commonArgs "-build" $buildTarget
}

exit $LASTEXITCODE
