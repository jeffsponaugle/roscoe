<#
.SYNOPSIS
    Compiles, simulates and fits IOCTRL with WinCUPL's command-line tools, and checks the result.

.DESCRIPTION
    Works on a copy in a scratch folder, so nothing is written next to the source.
      1. Writes the test vectors (gen-ioctrl-si.ps1).
      2. Compiles with cupl.exe, minimisation -m2 as in WinCUPL here, and runs the simulator (-s).
         Every vector carries expected values, so csim reports any output that differs. One
         report is expected and allowed: csim cannot show the pulled-up nSYS_RESET line that
         IOCTRL starts to drive at the kick after a 68030 RESET instruction.
      3. Fits with fit1504 for the TQFP-44, JTAG on, and checks that every pin is where the
         .PLD puts it and that TDI, TMS, TCK and TDO are on pins 1, 7, 26 and 32.
    Prints PASS (exit 0) or the problems (exit 1). The fit report and JEDEC file stay in the
    scratch folder.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File fpga\ioctrl\check-ioctrl.ps1
#>
param(
    [string]$Pld = (Join-Path $PSScriptRoot 'IOCTRL_REV1.PLD'),
    [string]$WorkDir = (Join-Path $env:TEMP 'roscoe-check-ioctrl'),
    [string]$Wincupl = 'C:\Wincupl'
)
$ErrorActionPreference = 'Stop'
$problems = New-Object System.Collections.Generic.List[string]
function Fail([string]$m) { $problems.Add($m); "  FAIL: $m" }

$cupl = Join-Path $Wincupl 'Shared\cupl.exe'
$lib  = Join-Path $Wincupl 'Shared\cupl.dl'
$fit  = Join-Path $Wincupl 'WinCupl\Fitters\fit1504.exe'
foreach ($t in $cupl, $lib, $fit) { if (-not (Test-Path $t)) { throw "not found: $t (use -Wincupl)" } }

$base = [System.IO.Path]::GetFileNameWithoutExtension($Pld)
New-Item -ItemType Directory -Force $WorkDir | Out-Null
Get-ChildItem $WorkDir -Filter "$base.*" | Remove-Item -Force
Copy-Item $Pld (Join-Path $WorkDir "$base.PLD")
"check-ioctrl: $base"
& (Join-Path $PSScriptRoot 'gen-ioctrl-si.ps1') -Pld (Join-Path $WorkDir "$base.PLD") -Out (Join-Path $WorkDir "$base.si") | Out-Null
$nVec = @(Get-Content (Join-Path $WorkDir "$base.si") | Where-Object { $_ -match '^[01K]{10} ' }).Count

function Invoke-Tool([string]$exe, [string[]]$arguments, [string]$log) {
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $WorkDir -NoNewWindow -PassThru `
         -RedirectStandardOutput (Join-Path $WorkDir "$log.out.txt") -RedirectStandardError (Join-Path $WorkDir "$log.err.txt")
    if (-not $p.WaitForExit(300000)) { $p.Kill(); throw "$exe timed out" }
    return (Get-Content (Join-Path $WorkDir "$log.out.txt")) -join "`n"
}

# ---- 1-2. Compile and simulate ----
$out = Invoke-Tool $cupl @('-m2lxfnabes', '-u', $lib, "$base.PLD") 'cupl'
if (-not (Test-Path (Join-Path $WorkDir "$base.tt2"))) { Fail "cupl did not compile it; see $WorkDir\cupl.out.txt and $base.lst" }
$soPath = Join-Path $WorkDir "$base.so"
if (-not (Test-Path $soPath)) { Fail 'the simulator wrote no .so file' }
else {
    $so = Get-Content $soPath
    $simulated = @($so | Where-Object { $_ -match '^\d{4}: ' }).Count
    if ($simulated -ne $nVec) { Fail "simulated $simulated of $nVec vectors" }
    $allowed = 0
    for ($i = 0; $i -lt $so.Count; $i++) {
        if ($so[$i] -notmatch '^\s*\[\d{4}') { continue }
        $vec = ''
        for ($j = $i - 1; $j -ge 0; $j--) { if ($so[$j] -match '^\d{4}: ') { $vec = $so[$j]; break } }
        $kick = if ($vec) { ($vec.TrimEnd() -split ' ')[-1][0] } else { '' }
        if ($so[$i] -match 'user expected \(1\) for nSYS_RESET' -and $kick -eq 'H' -and $allowed -eq 0) { $allowed++; continue }
        Fail ("simulation: {0}  at  {1}" -f $so[$i].Trim(), $vec.Trim())
    }
    "simulation: $simulated vectors, $allowed known csim report"
}

# ---- 3. Fit ----
$out = Invoke-Tool $fit @("$base.tt2", '-CUPL', '-dev', 'P1504T44', '-JTAG', 'ON') 'fit'
$fitPath = Join-Path $WorkDir "$base.fit"
if (-not (Test-Path $fitPath)) { Fail 'the fitter wrote no .fit report' }
else {
    $fitText = Get-Content $fitPath
    if (-not ($fitText -match 'Design FITS')) { Fail "the design does not fit; see $fitPath" }
    # pins as placed by the fitter
    $placed = @{}
    foreach ($l in $fitText) { if ($l -match '^Pin\s+(\d+)\s*=\s*(\w+);') { $placed[[int]$matches[1]] = $matches[2] } }
    # pins as declared in the source, including the  Pin [a,b] = [NAMEx..y];  form
    $declared = @{}
    foreach ($l in (Get-Content $Pld)) {
        $code = ($l -split '//')[0]
        if ($code -match '^\s*Pin\s+(\d+)\s*=\s*!?(\w+)\s*;') { $declared[[int]$matches[1]] = $matches[2] }
        elseif ($code -match '^\s*Pin\s*\[([\d,\s]+)\]\s*=\s*\[(\w*?)(\d+)\.\.(\d+)\]\s*;') {
            $nums = $matches[1] -split '\s*,\s*' | ForEach-Object { [int]$_ }
            $stem = $matches[2]; $a = [int]$matches[3]; $b = [int]$matches[4]
            $names = if ($a -le $b) { $a..$b } else { $a..$b }
            for ($k = 0; $k -lt $nums.Count; $k++) { $declared[$nums[$k]] = $stem + $names[$k] }
        }
    }
    foreach ($p in $declared.Keys | Sort-Object) {
        if ($placed[$p] -ne $declared[$p]) { Fail "pin ${p}: source has $($declared[$p]), fitter placed $($placed[$p])" }
    }
    foreach ($j in @(@(1, 'TDI'), @(7, 'TMS'), @(26, 'TCK'), @(32, 'TDO'))) {
        if ($placed[$j[0]] -ne $j[1]) { Fail "JTAG: pin $($j[0]) should be $($j[1]), fitter has $($placed[$j[0]])" }
    }
    $cells = ($fitText | Where-Object { $_ -match 'Total Logic cells used' }) -replace '\s+', ' '
    $ffs   = ($fitText | Where-Object { $_ -match 'Total Flip-Flop used' }) -replace '\s+', ' '
    "fit: $($declared.Count) signal pins as declared; JTAG on 1/7/26/32;$cells;$ffs"
}

if ($problems.Count) { "FAIL: $($problems.Count) problem(s). Files are in $WorkDir"; exit 1 }
"PASS. Files are in $WorkDir"
exit 0
