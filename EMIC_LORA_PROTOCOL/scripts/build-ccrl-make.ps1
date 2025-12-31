[CmdletBinding()]
param(
  [Parameter(Mandatory = $false)]
  [string]$Config = "HardwareDebug",

  [Parameter(Mandatory = $false)]
  [ValidateSet("all", "clean")]
  [string]$Target = "all",

  # Optional second target (used for "clean" then "all")
  [Parameter(Mandatory = $false)]
  [ValidateSet("all", "clean")]
  [string]$Then,

  [Parameter(Mandatory = $false)]
  [int]$Jobs = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-WorkspaceRoot {
  $scriptDir = Split-Path -Parent $PSCommandPath
  return (Resolve-Path (Join-Path $scriptDir ".." )).Path
}

function Find-ExeOnPath([string]$name) {
  $cmd = Get-Command $name -ErrorAction SilentlyContinue
  if ($null -ne $cmd) { return $cmd.Source }
  return $null
}

function Find-MakeFallback([string]$workspaceRoot) {
  $candidates = @()

  if ($env:E2STUDIO_HOME) { $candidates += $env:E2STUDIO_HOME }
  if ($env:E2STUDIO) { $candidates += $env:E2STUDIO }
  if ($env:RENESAS_E2STUDIO_HOME) { $candidates += $env:RENESAS_E2STUDIO_HOME }

  # Common install roots
  $candidates += @(
    "C:\\Renesas",
    "C:\\Program Files\\Renesas Electronics",
    "C:\\Program Files (x86)\\Renesas Electronics"
  )

  foreach ($root in $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique) {
    try {
      $hit = Get-ChildItem -Path $root -Filter "make.exe" -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
      if ($hit) { return $hit.FullName }
    } catch {
      # ignore and continue
    }
  }

  return $null
}

$workspaceRoot = Resolve-WorkspaceRoot
$cfgDir = Join-Path $workspaceRoot $Config
$makefile = Join-Path $cfgDir "makefile"

if (-not (Test-Path $makefile)) {
  throw "Không tìm thấy makefile: $makefile"
}

$makeExe = Find-ExeOnPath "make"
if (-not $makeExe) {
  $makeExe = Find-MakeFallback $workspaceRoot
}

if (-not $makeExe) {
  $msg = @(
    "Không tìm thấy 'make' trong PATH.",
    "Cách xử lý nhanh:",
    "- Mở e2studio -> Window > Preferences (hoặc toolchain settings) và xem đường dẫn GNU Make / MSYS đi kèm.",
    "- Thêm thư mục chứa make.exe (và sh.exe/rm.exe) vào PATH của Windows hoặc VS Code terminal env.",
    "",
    "Hoặc dùng task 'Build (e2studio headless / HardwareDebug)' sau khi set E2STUDIO_EXE."
  ) -join "`n"
  throw $msg
}

$baseArgs = @("-C", $cfgDir, "-f", "makefile")
if ($Jobs -gt 0) {
  $baseArgs += @("-j", "$Jobs")
}

Write-Host "[CCRL] make: $makeExe"
Write-Host "[CCRL] config: $Config"
Write-Host "[CCRL] target: $Target" + ($(if ($Then) { " then $Then" } else { "" }))

& $makeExe @baseArgs $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Then) {
  & $makeExe @baseArgs $Then
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
