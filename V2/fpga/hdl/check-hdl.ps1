<#
.SYNOPSIS
    Simulates every testbench in fpga\hdl with Vivado's xsim, then builds the whole FPGA design against
    the real pin constraints and checks the result.

.DESCRIPTION
    Works on copies in a scratch folder (-WorkDir), so nothing is written next to the sources.
      1. For each testbench: xvlog, xelab, xsim. A testbench checks itself and ends with "TB PASS" or
         "TB FAIL". Any warning from the compiler or elaborator fails too.
           tb_intctrl      intctrl.v on its own
           tb_roscoe_top   the whole design: roscoe_top.v, busctrl.v, dramctrl.v, intctrl.v
      2. Synthesis, place and route of roscoe_top for the Au's part, with ..\roscoe_busctrl_au.xdc (pins and
         the clock) and ..\roscoe_timing.xdc (I/O timing). It fails on errors, critical warnings, latches,
         DRC errors, a port off its planned ball, negative setup or hold slack, or a warning not in
         $expected below. It prints the worst setup and hold slack of each group of signals (io_slack.txt).
         The bitstream isn't written. -SimOnly skips this step (about 3-4 minutes).
    Prints PASS (exit 0) or the problems (exit 1). The logs and reports stay in -WorkDir.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File fpga\hdl\check-hdl.ps1
#>
param(
    [string]$WorkDir = (Join-Path $env:TEMP 'roscoe-check-hdl'),
    [string]$Part    = 'xc7a35tftg256-2',                  # Alchitry Au V2: XC7A35T-2FTG256
    [string]$Xdc     = (Join-Path $PSScriptRoot '..\roscoe_busctrl_au.xdc'),
    [string]$TimingXdc = (Join-Path $PSScriptRoot '..\roscoe_timing.xdc'),
    [switch]$SimOnly,
    [string]$Vivado  = ''                                  # vivado.bat; default: the newest one installed
)
$ErrorActionPreference = 'Stop'
$problems = New-Object System.Collections.Generic.List[string]
function Fail([string]$m) { $problems.Add($m); "  FAIL: $m" }

$benches = @(   # unisim: the design instantiates Xilinx primitives (ODDR), so xsim needs unisims_ver and glbl
    @{ name = 'tb_intctrl';    files = @('intctrl.v', 'tb_intctrl.v'); unisim = $false },
    @{ name = 'tb_roscoe_top'; files = @('intctrl.v', 'dramctrl.v', 'busctrl.v', 'roscoe_top.v', 'tb_roscoe_top.v'); unisim = $true }
)
$design = @('intctrl.v', 'dramctrl.v', 'busctrl.v', 'roscoe_top.v')

# Warnings this stage of the design is known to give. Each is matched against the whole log line.
$expected = @(
    @{ re = 'Synth 8-7129\].*\bcbreq_n\b';                   why = 'cbreq_n is unused until the SRAM burst step' },
    @{ re = 'Synth 8-3917\].*\b(sterm_n|cback_n)\b';         why = 'STERM and CBACK are tied off until the SRAM burst step' },
    @{ re = 'Synth 8-7129\] Port a\[2[45]\] in module dramctrl'; why = 'A25/A24 only pick the rank with 16 MB ranks (RANK_64MB = 0)' }
)

if (-not $Vivado) {
    $Vivado = Get-ChildItem 'C:\AMDDesignTools\*\Vivado\bin\vivado.bat', 'C:\Xilinx\Vivado\*\bin\vivado.bat' -ErrorAction SilentlyContinue |
        Sort-Object { [version]([regex]::Match($_.FullName, '\d{4}\.\d+').Value) } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $Vivado) { throw 'Vivado not found under C:\AMDDesignTools or C:\Xilinx; pass -Vivado <path to vivado.bat>' }
}
$bin = Split-Path $Vivado
$Xdc = (Resolve-Path $Xdc).Path
$TimingXdc = (Resolve-Path $TimingXdc).Path
New-Item -ItemType Directory -Force $WorkDir | Out-Null
"check-hdl: $($design -join ', ')"
"  Vivado:   $Vivado"
"  work dir: $WorkDir"

function Invoke-Vivado([string]$dir, [string]$exe, [string[]]$arguments) {
    Push-Location $dir
    try {
        $ErrorActionPreference = 'Continue'
        & (Join-Path $bin $exe) @arguments | Out-Null
        return $LASTEXITCODE
    } finally { Pop-Location }
}
function Get-Log([string]$path) { if (Test-Path $path) { return @(Get-Content $path) } else { return @() } }

# ---- 1. Simulation ----
foreach ($b in $benches) {
    $dir = Join-Path $WorkDir $b.name
    New-Item -ItemType Directory -Force $dir | Out-Null
    Get-ChildItem $dir -File | Remove-Item -Force
    foreach ($f in $b.files) { Copy-Item (Join-Path $PSScriptRoot $f) (Join-Path $dir $f) -Force }
    $srcs = @($b.files); $tops = @("work.$($b.name)"); $libs = @()
    if ($b.unisim) {
        Copy-Item (Join-Path (Split-Path $bin) 'data\verilog\src\glbl.v') (Join-Path $dir 'glbl.v') -Force
        $srcs += 'glbl.v'; $tops += 'work.glbl'; $libs = @('-L', 'unisims_ver')
    }
    $t = Measure-Command {
        $script:cv = Invoke-Vivado $dir 'xvlog.bat' (@('--log', 'xvlog.log') + $srcs)
        $script:ce = if ($script:cv -eq 0) { Invoke-Vivado $dir 'xelab.bat' (@('--log', 'xelab.log', '--debug', 'off') + $libs + @('-s', 'sim') + $tops) } else { -1 }
        $script:cs = if ($script:ce -eq 0) { Invoke-Vivado $dir 'xsim.bat' @('sim', '-R', '--log', 'xsim.log') } else { -1 }
    }
    foreach ($l in 'xvlog.log', 'xelab.log') {
        foreach ($w in (Get-Log (Join-Path $dir $l) | Where-Object { $_ -match '^(WARNING|ERROR|CRITICAL WARNING)' })) { Fail "$($b.name) $l`: $w" }
    }
    if ($cv -ne 0) { Fail "$($b.name): xvlog failed (exit $cv); see $dir\xvlog.log"; continue }
    if ($ce -ne 0) { Fail "$($b.name): xelab failed (exit $ce); see $dir\xelab.log"; continue }
    $sim = Get-Log (Join-Path $dir 'xsim.log')
    foreach ($e in ($sim | Where-Object { $_ -match 'FAIL|TB BUG|CONFIG ERROR|^(ERROR|WARNING|CRITICAL WARNING)' } | Select-Object -First 40)) {
        if ($e -notmatch '^TB FAIL') { Fail "$($b.name): $e" }
    }
    $verdict = $sim | Where-Object { $_ -match '^TB (PASS|FAIL)' } | Select-Object -Last 1
    if (-not $verdict) { Fail "$($b.name): no verdict; see $dir\xsim.log" }
    elseif ($verdict -notmatch '^TB PASS') { Fail "$($b.name): $verdict" }
    '{0,-14} {1} ({2} s)' -f $b.name, $(if ($verdict) { $verdict } else { 'no verdict' }), [int]$t.TotalSeconds
}

# ---- 2. Implementation ----
if (-not $SimOnly) {
    $dir = Join-Path $WorkDir 'impl'
    New-Item -ItemType Directory -Force $dir | Out-Null
    Get-ChildItem $dir -File | Remove-Item -Force
    foreach ($f in $design) { Copy-Item (Join-Path $PSScriptRoot $f) (Join-Path $dir $f) -Force }
    Copy-Item $Xdc (Join-Path $dir 'roscoe.xdc') -Force
    Copy-Item $TimingXdc (Join-Path $dir 'roscoe_timing.xdc') -Force
    $tcl = @'
read_verilog {@SOURCES@}
read_xdc roscoe.xdc
read_xdc roscoe_timing.xdc
synth_design -top roscoe_top -part @PART@
opt_design
place_design
route_design
write_checkpoint -force routed.dcp
report_utilization -file util.rpt
report_timing_summary -max_paths 5 -file timing.rpt
report_drc -file drc.rpt
check_timing -verbose -file check_timing.rpt
set fh [open ports.txt w]
foreach p [lsort [get_ports]] { puts $fh "[get_property NAME $p] [get_property PACKAGE_PIN $p]" }
close $fh
# worst setup and hold slack per group of signals
set groups {
    {in  {cpu_as_n}                                             {AS, taken at R1}}
    {in  {cpu_a_hi[*] cpu_a_lo[*] cpu_fc[*] cpu_siz[*] cpu_rw}   {address, FC, SIZ, R/W}}
    {in  {cpu_d[*]}                                             {write data D31-D24}}
    {out {dsack_d[*] dsack_oe_n}                                {DSACK to the CPU}}
    {out {berr_n}                                               {BERR to the CPU}}
    {out {cpu_d[*] data_oe_n}                                   {read data D31-D24}}
    {out {dev_code[*] dev_en_n}                                 {DEV_CODE, nDEV_EN to IOCTRL}}
    {out {ras_n[*] cas_n[*] dram_we_n dram_buf_oe_n}            {DRAM RAS, CAS, WE, 245 enable}}
    {out {dram_mux}                                             {DRAM_MUX}}
    {out {sram_ce_n[*] be_n[*] flash_ce_n}                      {SRAM and flash selects}}
    {out {exp_vmem_cs_n exp_vio_cs_n}                           {nEXP_VMEM_CS, nEXP_VIO_CS (card selects)}}
    {out {ciin_n sram_ba[*]}                                    {CIIN, SRAM_BA (address decodes)}}
}
set fh [open io_slack.txt w]
foreach g $groups {
    lassign $g dir pat name
    set ports [get_ports $pat]
    if {$dir eq "in"} {
        set s [get_timing_paths -from $ports -max_paths 1 -setup]
        set h [get_timing_paths -from $ports -max_paths 1 -hold]
    } else {
        set s [get_timing_paths -to $ports -max_paths 1 -setup]
        set h [get_timing_paths -to $ports -max_paths 1 -hold]
    }
    set ss [expr {[llength $s] ? [get_property SLACK $s] : "none"}]
    set hs [expr {[llength $h] ? [get_property SLACK $h] : "none"}]
    puts $fh "$name|$ss|$hs"
    if {[llength $s]} { report_timing -of_objects $s -append -file paths_setup.rpt }
    if {[llength $h]} { report_timing -of_objects $h -append -file paths_hold.rpt }
}
close $fh
puts "CHECK-HDL IMPL DONE"
'@
    $tcl = $tcl.Replace('@SOURCES@', ($design -join ' ')).Replace('@PART@', $Part)
    [IO.File]::WriteAllText((Join-Path $dir 'impl.tcl'), $tcl)
    $t = Measure-Command { $script:vi = Invoke-Vivado $dir 'vivado.bat' @('-mode', 'batch', '-nojournal', '-log', 'impl.log', '-notrace', '-source', 'impl.tcl') }
    $log = Get-Log (Join-Path $dir 'impl.log')
    if (-not ($log | Where-Object { $_ -cmatch '^CHECK-HDL IMPL DONE\s*$' })) { Fail "implementation didn't finish (exit $vi); see $dir\impl.log" }
    foreach ($e in ($log | Where-Object { $_ -cmatch '^(ERROR|CRITICAL WARNING)' })) { Fail "implementation: $e" }
    foreach ($e in ($log | Where-Object { $_ -match 'inferring latch|Synth 8-327\]' })) { Fail "implementation: $e" }
    $seen = @{}
    foreach ($w in ($log | Where-Object { $_ -cmatch '^WARNING' })) {
        $known = $expected | Where-Object { $w -match $_.re } | Select-Object -First 1
        if ($known) { $seen[$known.why] = 1 } else { Fail "implementation: $w" }
    }
    foreach ($k in $seen.Keys) { "  expected warning: $k" }

    # every port on the ball the XDC gives it
    $want = @{}
    foreach ($l in (Get-Content $Xdc)) {
        if ($l -match '^\s*set_property\s+-dict\s+\{PACKAGE_PIN\s+(\w+)[^}]*\}\s+\[get_ports\s+\{([^}]+)\}\]') { $want[$matches[2]] = $matches[1] }
    }
    $placed = @{}
    foreach ($l in (Get-Log (Join-Path $dir 'ports.txt'))) { $f = $l -split ' '; $placed[$f[0]] = $f[1] }
    foreach ($p in $want.Keys) { if ($placed[$p] -ne $want[$p]) { Fail "port $p is on '$($placed[$p])', the XDC says $($want[$p])" } }
    foreach ($p in $placed.Keys) { if (-not $want.ContainsKey($p)) { Fail "port $p isn't in the XDC" } }

    $drc = @(Get-Log (Join-Path $dir 'drc.rpt') | Where-Object { $_ -match '^\|\s*[A-Z0-9]+-\d+\s*\|' })
    foreach ($r in $drc) { if ($r -match '\|\s*(Error|Critical Warning)\s*\|') { Fail "DRC $r" } else { "  DRC: $($r -replace '\s+', ' ')" } }

    $util = Get-Log (Join-Path $dir 'util.rpt')
    $luts = ($util | Where-Object { $_ -match '^\|\s*Slice LUTs\*?\s*\|' } | Select-Object -First 1) -replace '^\|\s*Slice LUTs\*?\s*\|\s*(\d+).*$', '$1'
    $ffs  = ($util | Where-Object { $_ -match '^\|\s*Slice Registers\s*\|' } | Select-Object -First 1) -replace '^\|\s*Slice Registers\s*\|\s*(\d+).*$', '$1'
    $timing = Get-Log (Join-Path $dir 'timing.rpt')
    $wns = $null; $whs = $null
    for ($i = 0; $i -lt $timing.Count; $i++) {
        if ($timing[$i] -match '^\s*WNS\(ns\)\s+TNS\(ns\)') {
            $vals = ($timing[$i + 2].Trim() -split '\s+')
            if ($vals[0] -match '^-?\d+(\.\d+)?$') { $wns = [double]$vals[0] }
            if ($vals[4] -match '^-?\d+(\.\d+)?$') { $whs = [double]$vals[4] }
            break
        }
    }
    if ($null -eq $wns -or $null -eq $whs) { Fail "no worst slack in $dir\timing.rpt" }
    else {
        if ($wns -lt 0) { Fail "timing: worst setup slack $wns ns" }
        if ($whs -lt 0) { Fail "timing: worst hold slack $whs ns" }
    }
    # Every check_timing count must be 0: a port without both input (or output) delays, an unconstrained
    # endpoint and the like are holes in roscoe_timing.xdc. (The report lists each check twice.)
    $ctSeen = @{}
    foreach ($l in (Get-Log (Join-Path $dir 'check_timing.rpt'))) {
        if ($l -match '^\s*\d+\.\s+checking\s+(\w+)\s+\((\d+)\)' -and [int]$matches[2] -gt 0 -and -not $ctSeen.ContainsKey($matches[1])) {
            $ctSeen[$matches[1]] = $true
            Fail "check_timing: $($matches[1]) ($($matches[2])); see $dir\check_timing.rpt"
        }
    }
    '  {0,-44} {1,10} {2,10}' -f 'worst slack, ns', 'setup', 'hold'
    foreach ($l in (Get-Log (Join-Path $dir 'io_slack.txt'))) {
        $f = $l -split '\|'
        '  {0,-44} {1,10} {2,10}' -f $f[0], $f[1], $f[2]
        foreach ($v in $f[1], $f[2]) { if ($v -match '^-\d') { Fail "timing: $($f[0]) has negative slack ($v ns)" } }
    }
    'implementation: {0} ports placed as the XDC says; {1} LUTs, {2} flip-flops; worst setup slack {3} ns, hold {4} ns ({5} s)' -f $placed.Count, $luts, $ffs, $wns, $whs, [int]$t.TotalSeconds
}

if ($problems.Count) { "FAIL: $($problems.Count) problem(s). The logs are in $WorkDir"; exit 1 }
"PASS. The logs are in $WorkDir"
exit 0
