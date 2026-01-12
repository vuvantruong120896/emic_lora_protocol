param(
    [Parameter(Mandatory = $false)]
    [string]$RfpExe = $env:RFP_EXE,

    [Parameter(Mandatory = $false)]
    [string]$ProjectFile = $env:RFP_PROJECT_FILE,

    [Parameter(Mandatory = $false)]
    [string]$ImageFile = "",

    [Parameter(Mandatory = $false)]
    [string]$ExtraArgs = $env:RFP_EXTRA_ARGS
)

$ErrorActionPreference = 'Stop'

function Convert-HexToInt {
    param([Parameter(Mandatory = $true)][string]$Hex)

    $h = $Hex.Trim()
    if ($h.StartsWith('0x')) {
        $h = $h.Substring(2)
    }
    if (-not $h) {
        throw "Invalid hex value: '$Hex'"
    }
    return [Convert]::ToInt32($h, 16)
}

function Try-GetMaxFlashAddressFromMap {
    param([Parameter(Mandatory = $true)][string]$MapFile)

    if (-not (Test-Path -LiteralPath $MapFile)) {
        return $null
    }

    $text = Get-Content -LiteralPath $MapFile -Raw

    # Prefer explicit debug monitor range if present (commonly matches flash top).
    $m = [regex]::Match($text, '-debug_monitor=([0-9A-Fa-f]+)-([0-9A-Fa-f]+)')
    if ($m.Success) {
        return (Convert-HexToInt $m.Groups[2].Value)
    }

    # Fallback: use mapping list for .monitor2 if present.
    $m2 = [regex]::Match($text, '(?m)^\.monitor2\s*\r?\n\s*([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})')
    if ($m2.Success) {
        return (Convert-HexToInt $m2.Groups[2].Value)
    }

    return $null
}

function Try-GetSrecAddress {
    param([Parameter(Mandatory = $true)][string]$Line)

    if ($Line.Length -lt 4 -or $Line[0] -ne 'S') {
        return $null
    }

    $type = $Line.Substring(0, 2)
    switch ($type) {
        'S1' {
            if ($Line.Length -lt 8) { return $null }
            return Convert-HexToInt $Line.Substring(4, 4)
        }
        'S2' {
            if ($Line.Length -lt 10) { return $null }
            return Convert-HexToInt $Line.Substring(4, 6)
        }
        'S3' {
            if ($Line.Length -lt 12) { return $null }
            return Convert-HexToInt $Line.Substring(4, 8)
        }
        default { return $null }
    }
}

function New-FilteredMotFile {
    param(
        [Parameter(Mandatory = $true)][string]$InputMot,
        [Parameter(Mandatory = $true)][string]$OutputMot,
        [Parameter(Mandatory = $true)][int]$MaxAddr
    )

    $lines = Get-Content -LiteralPath $InputMot
    $kept = New-Object System.Collections.Generic.List[string]

    $removedDataRecords = 0
    $removedCountRecords = 0

    foreach ($line in $lines) {
        if (-not $line) {
            continue
        }

        # Drop record-count lines to avoid stale counts after filtering.
        if ($line.StartsWith('S5') -or $line.StartsWith('S6')) {
            $removedCountRecords++
            continue
        }

        $addr = Try-GetSrecAddress -Line $line
        if ($null -ne $addr) {
            if ($addr -le $MaxAddr) {
                $kept.Add($line)
            }
            else {
                $removedDataRecords++
            }
            continue
        }

        # Keep non-address records (S0 header, S7/S8/S9 termination, etc.).
        $kept.Add($line)
    }

    Set-Content -LiteralPath $OutputMot -Value $kept -Encoding ASCII
    return [pscustomobject]@{
        RemovedDataRecords  = $removedDataRecords
        RemovedCountRecords = $removedCountRecords
        OutputFile          = $OutputMot
    }
}

function Resolve-RfpExe {
    param([string]$Candidate)

    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }

    $names = @(
        'rfp-cli.exe',
        'RFPV3.exe',
        'rfp.exe'
    )

    foreach ($n in $names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }

    $roots = @(
        ${env:ProgramFiles},
        ${env:'ProgramFiles(x86)'}
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($root in $roots) {
        $baseDirs = New-Object System.Collections.Generic.List[string]

        # Common vendor folders (varies by installer/version)
        $common = @(
            (Join-Path $root 'Renesas Electronics'),
            (Join-Path $root 'Renesas'),
            (Join-Path $root 'RENESAS')
        )
        foreach ($c in $common) {
            if ($c -and (Test-Path -LiteralPath $c)) {
                $baseDirs.Add($c)
            }
        }

        # Also pick up any top-level directory that starts with "Renesas"
        try {
            $top = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'Renesas*' }
            foreach ($d in $top) {
                $baseDirs.Add($d.FullName)
            }
        }
        catch {
            # ignore
        }

        $baseDirs = $baseDirs | Select-Object -Unique

        foreach ($base in $baseDirs) {
            foreach ($n in $names) {
                $hits = Get-ChildItem -LiteralPath $base -Recurse -File -Filter $n -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($hits) {
                    return $hits.FullName
                }
            }
        }
    }

    return $null
}

function Get-HelpText {
    param([string]$Exe)

    $candidates = @('/?','-?','--help','-h')
    foreach ($arg in $candidates) {
        try {
            $out = & $Exe $arg 2>&1
            if ($out) {
                return ($out | Out-String)
            }
        }
        catch {
            # ignore and continue
        }
    }

    return ""
}

function Detect-Flag {
    param(
        [string]$Help,
        [string[]]$Preferred,
        [string[]]$Fallback
    )

    foreach ($p in $Preferred) {
        if ($Help -match [regex]::Escape($p)) {
            return $p
        }
    }

    foreach ($f in $Fallback) {
        if ($Help -match [regex]::Escape($f)) {
            return $f
        }
    }

    return $null
}

$rfp = Resolve-RfpExe -Candidate $RfpExe
if (-not $rfp) {
    throw "Renesas Flash Programmer executable not found. Set RFP_EXE env var to the full path of RFP (e.g. RFPV3.exe) or add it to PATH."
}

if (-not $ProjectFile) {
    throw "RFP project file not set. Create a Flash Programmer project for RL78 + E2 Lite and set RFP_PROJECT_FILE env var to its path."
}

if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "RFP project file not found: $ProjectFile"
}

if ($ImageFile) {
    if (-not (Test-Path -LiteralPath $ImageFile)) {
        throw "Image file not found: $ImageFile"
    }

    # Many CCRL/RL78 .mot images include RAM initialization records at high addresses (e.g. 0x00FBxxxx).
    # RFP will reject those as "outside device memory area". Filter to flash range before running RFP.
    $mapCandidate = [System.IO.Path]::ChangeExtension($ImageFile, '.map')
    $maxAddr = Try-GetMaxFlashAddressFromMap -MapFile $mapCandidate
    if ($null -eq $maxAddr) {
        # Conservative default for many RL78 configs; override by fixing the .map generation or project.
        $maxAddr = 0x1FFFF
    }

    $tmpFiltered = Join-Path $env:TEMP ("{0}.flashonly.mot" -f ([System.IO.Path]::GetFileNameWithoutExtension($ImageFile)))
    $filterInfo = New-FilteredMotFile -InputMot $ImageFile -OutputMot $tmpFiltered -MaxAddr $maxAddr
    Write-Host "Prepared flash-only image (max addr 0x$($maxAddr.ToString('X'))): $($filterInfo.OutputFile)"
    Write-Host "- Removed data records: $($filterInfo.RemovedDataRecords)"
    Write-Host "- Removed count records: $($filterInfo.RemovedCountRecords)"

    $backup = "$ImageFile.bak"
    Copy-Item -LiteralPath $ImageFile -Destination $backup -Force
    Copy-Item -LiteralPath $tmpFiltered -Destination $ImageFile -Force
    Write-Host "NOTE: Temporarily replaced ImageFile contents for RFP project: $ImageFile"
}

$help = Get-HelpText -Exe $rfp

# Try to auto-detect common switches. RFP versions differ, so we only proceed if we can
# see plausible switches in help output.
$projectFlag = Detect-Flag -Help $help -Preferred @('-p','/p','-project','/project') -Fallback @()
$autoFlag    = Detect-Flag -Help $help -Preferred @('-auto','/auto') -Fallback @()
$quitFlag    = Detect-Flag -Help $help -Preferred @('-quit','/quit','-exit','/exit') -Fallback @()

$args = @()

if ($projectFlag -and $autoFlag -and $quitFlag) {
    $args += $projectFlag
    $args += $ProjectFile
    $args += $autoFlag
    $args += $quitFlag
}
else {
    $msg = "Unable to auto-detect RFP command-line switches from help output.\n" +
           "- Executable: $rfp\n" +
           "- Please run '$rfp /?' in a terminal and identify the flags to run a project in batch mode.\n" +
           "- Then set RFP_EXTRA_ARGS env var to the exact arguments (excluding the exe path), e.g. '-p <project_file> -auto -quit'.\n"

    if (-not $ExtraArgs) {
        throw $msg
    }
}

if ($ExtraArgs) {
    # Allow user override/append.
    $split = @()
    $m = [regex]::Matches($ExtraArgs, '([^\s\"]+|\"[^\"]*\")+')
    foreach ($x in $m) {
        $token = $x.Value
        if ($token.StartsWith('"') -and $token.EndsWith('"')) {
            $token = $token.Substring(1, $token.Length - 2)
        }
        $split += $token
    }

    # If auto-detect failed, ExtraArgs becomes the full arg list.
    if (-not ($projectFlag -and $autoFlag -and $quitFlag)) {
        $args = $split
    }
    else {
        $args += $split
    }
}

Write-Host "Flashing with Renesas Flash Programmer..."
Write-Host "- RFP: $rfp"
Write-Host "- Project: $ProjectFile"
if ($args) {
    Write-Host "- Args: $($args -join ' ')"
}

try {
    & $rfp @args
    Write-Host "Flash task finished."
}
finally {
    if ($ImageFile) {
        $backup = "$ImageFile.bak"
        if (Test-Path -LiteralPath $backup) {
            Move-Item -LiteralPath $backup -Destination $ImageFile -Force
            Write-Host "Restored original ImageFile: $ImageFile"
        }
    }
}
