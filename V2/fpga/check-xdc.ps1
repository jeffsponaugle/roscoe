<#
  check-xdc.ps1 - check roscoe_busctrl_au.xdc in Vivado against the pin list (BrWide_pinlist.csv).

  It builds a throwaway top level that uses every FPGA signal in the pin list: all inputs fold into one register and
  every output comes from it, so each pin keeps its I/O buffer. Then it runs Vivado's whole flow on that with the
  XDC: synthesis, place, route and bitstream. That catches balls the part doesn't have, ports with no pin, bank and
  I/O-standard clashes, and anything the bitstream DRCs reject. Last, it compares Vivado's result with the pin list:
  - each port's ball, bank, direction, I/O standard, and slew/drive on outputs;
  - each ball's pin function, from Vivado's device data, against the Au net name in the pin list (bank, pair number,
    P/N, MRCC, ADC channel). LED0-7 have no such name, so their balls are checked against the Au V2 schematic
    (the table below).

  Run it after gen-xdc.ps1 and gen-pinlist.ps1:
      powershell -ExecutionPolicy Bypass -File fpga\check-xdc.ps1
  It takes about 90 s. Vivado's log and reports (io.rpt, drc.rpt) go to -WorkDir, outside the repo. The bitstream is
  deleted as soon as it's written: it drives every output from a shift register and must never reach a board.
  Exit code 0 means it passed.
#>
param(
  [string]$PinList = (Join-Path $PSScriptRoot 'BrWide_pinlist.csv'),
  [string]$Xdc     = (Join-Path $PSScriptRoot 'roscoe_busctrl_au.xdc'),
  [string]$Part    = 'xc7a35tftg256-2',                       # Alchitry Au V2: XC7A35T-2FTG256
  [string]$WorkDir = (Join-Path $env:TEMP 'roscoe-check-xdc'),
  [string]$Vivado  = ''                                       # vivado.bat; default: the newest one installed
)
$ErrorActionPreference = 'Stop'

# The Au's LED0-7 pins, from the Au V2 schematic (AuSchematic.pdf sheet 1, bank 14 symbol).
$auLedBall = @{ LED0 = 'K13'; LED1 = 'K12'; LED2 = 'L14'; LED3 = 'L13'; LED4 = 'M15'; LED5 = 'M14'; LED6 = 'M12'; LED7 = 'P14' }

if (-not $Vivado) {
  $Vivado = Get-ChildItem 'C:\AMDDesignTools\*\Vivado\bin\vivado.bat', 'C:\Xilinx\Vivado\*\bin\vivado.bat' -ErrorAction SilentlyContinue |
    Sort-Object { [version]([regex]::Match($_.FullName, '\d{4}\.\d+').Value) } -Descending |
    Select-Object -First 1 -ExpandProperty FullName
  if (-not $Vivado) { throw 'Vivado not found under C:\AMDDesignTools or C:\Xilinx; pass -Vivado <path to vivado.bat>' }
}
$PinList = (Resolve-Path $PinList).Path; $Xdc = (Resolve-Path $Xdc).Path
New-Item -ItemType Directory -Force $WorkDir | Out-Null
$rows = @(Import-Csv $PinList)
$sig = @($rows | Where-Object { $_.dir -in 'in', 'out', 'io' })
"check-xdc: $(Split-Path $Xdc -Leaf) on $Part, $($sig.Count) signals"
"  Vivado:   $Vivado"
"  work dir: $WorkDir"

# ---- the throwaway top level: ports grouped into buses as the XDC names them
$bus = [ordered]@{}
foreach ($r in $sig) {
  if ($r.hdl_port -match '^(\w+)\[(\d+)\]$') { $n = $matches[1]; $i = [int]$matches[2] } else { $n = $r.hdl_port; $i = -1 }
  if (-not $bus.Contains($n)) { $bus[$n] = @{ dir = $r.dir; idx = New-Object System.Collections.Generic.List[int] } }
  if ($bus[$n].dir -ne $r.dir) { throw "$n has mixed directions in the pin list" }
  $bus[$n].idx.Add($i)
}
if (-not $bus.Contains('cpu_clk')) { throw 'no cpu_clk in the pin list' }
$decl = @(); $ins = @(); $outs = @(); $ios = @(); $inBits = 0; $outBits = 0; $ioBits = 0
foreach ($n in $bus.Keys) {
  $b = $bus[$n]; $idx = @($b.idx | Sort-Object)
  if ($idx[0] -lt 0) { $range = ''; $w = 1 } else {
    $lo = $idx[0]; $hi = $idx[-1]; $w = $hi - $lo + 1
    if ($w -ne $idx.Count) { throw "$n has gaps in the pin list: $($idx -join ',')" }
    $range = "[${hi}:${lo}] "
  }
  $decl += '  {0} wire {1}{2}' -f @{ in = 'input '; out = 'output'; io = 'inout ' }[$b.dir], $range, $n
  if ($n -eq 'cpu_clk') { continue }
  switch ($b.dir) { 'in' { $ins += $n; $inBits += $w } 'out' { $outs += $n; $outBits += $w } 'io' { $ios += $n; $ioBits += $w } }
}
$W = [math]::Max(64, [math]::Max($inBits + $ioBits, [math]::Max($outBits, 8 + $ioBits)))
$v = @('// xdc_check_top.v - written by check-xdc.ps1. Throwaway: never load its bitstream onto a board.', 'module xdc_check_top (')
$v += ($decl -join ",`n")
$v += ');'
$v += "  wire [$($inBits + $ioBits - 1):0] ins = {$(($ins + $ios) -join ', ')};"
$v += "  reg  [$($W - 1):0] q = $W'h1;"
$v += "  always @(posedge cpu_clk) q <= {q[$($W - 2):0], q[$($W - 1)] ^ q[$($W - 4)]} ^ {{$($W - $inBits - $ioBits){1'b0}}, ins};"
if ($ios) { $v += "  assign {$($ios -join ', ')} = q[0] ? q[$(8 + $ioBits - 1):8] : {$ioBits{1'bz}};" }
$v += "  assign {$($outs -join ', ')} = q[$($outBits - 1):0] ^ q[$($W - 1):$($W - $outBits)];"
$v += 'endmodule'
[IO.File]::WriteAllText((Join-Path $WorkDir 'xdc_check_top.v'), ($v -join "`n") + "`n")
'  top level: {0} ports ({1} in + clock, {2} out, {3} inout)' -f $bus.Count, $inBits, $outBits, $ioBits

# ---- the Vivado script; every ball the pin list names gets its bank and pin function dumped
$balls = @()
foreach ($r in $rows) {
  if ($r.fpga_ball) { $balls += $r.fpga_ball } elseif ($r.au_net -match '^([A-Z]\d+)_1V35$') { $balls += $matches[1] }
}
$tcl = @"
read_verilog xdc_check_top.v
read_xdc {$($Xdc.Replace('\', '/'))}
synth_design -top xdc_check_top -part $Part
opt_design
place_design
route_design
report_drc -file drc.rpt
report_io -file io.rpt
set fh [open ports.txt w]
foreach p [lsort [get_ports]] {
  set pp [get_package_pins -quiet [get_property PACKAGE_PIN `$p]]
  if {`$pp eq ""} { puts `$fh "[get_property NAME `$p] NONE NONE NONE [get_property DIRECTION `$p] NONE NONE NONE"; continue }
  puts `$fh "[get_property NAME `$p] [get_property NAME `$pp] [get_property BANK `$pp] [get_property PIN_FUNC `$pp] [get_property DIRECTION `$p] [get_property IOSTANDARD `$p] [get_property SLEW `$p] [get_property DRIVE `$p]"
}
close `$fh
set fh [open balls.txt w]
foreach b {$($balls -join ' ')} {
  set pp [get_package_pins -quiet `$b]
  if {`$pp eq ""} { puts `$fh "`$b NONE NONE" } else { puts `$fh "`$b [get_property BANK `$pp] [get_property PIN_FUNC `$pp]" }
}
close `$fh
write_bitstream -force xdc_check_top.bit
file delete -force xdc_check_top.bit
puts "CHECK-XDC DONE"
"@
[IO.File]::WriteAllText((Join-Path $WorkDir 'check-xdc.tcl'), $tcl)

# ---- run Vivado
foreach ($f in 'check-xdc.log', 'ports.txt', 'balls.txt', 'drc.rpt', 'io.rpt') { Remove-Item (Join-Path $WorkDir $f) -ErrorAction SilentlyContinue }
Push-Location $WorkDir
try {
  $t = Measure-Command { & $Vivado -mode batch -nojournal -log check-xdc.log -notrace -source check-xdc.tcl | Out-Null }
  $vcode = $LASTEXITCODE
} finally { Pop-Location }

$bad = 0
function Bad([string]$m) { "  FAIL: $m"; $script:bad++ }
$log = Join-Path $WorkDir 'check-xdc.log'
$logText = @(if (Test-Path $log) { Get-Content $log })
$errs = @($logText | Where-Object { $_ -cmatch '^(ERROR|CRITICAL WARNING)' })
$warns = @($logText | Where-Object { $_ -cmatch '^WARNING' })
'Vivado: exit {0}, {1} s, {2} errors or critical warnings, {3} warnings' -f $vcode, [int]$t.TotalSeconds, $errs.Count, $warns.Count
foreach ($e in $errs) { Bad $e }
foreach ($w in ($warns | Select-Object -First 20)) { "  warning: $w" }
if (-not ($logText | Where-Object { $_ -cmatch '^CHECK-XDC DONE\s*$' })) { Bad "Vivado didn't finish the flow; see $log" }

$drc = Join-Path $WorkDir 'drc.rpt'
if (Test-Path $drc) {
  $drcRows = @(Get-Content $drc | Where-Object { $_ -match '^\|\s*[A-Z0-9]+-\d+\s*\|' })
  foreach ($r in $drcRows) { if ($r -match '\|\s*(Error|Critical Warning)\s*\|') { Bad "DRC $r" } else { "  DRC: $r" } }
  "DRC: $($drcRows.Count) rule(s) with findings"
}

# ---- ports: ball, bank, direction, I/O standard, slew/drive
$ports = @{}
$portsFile = Join-Path $WorkDir 'ports.txt'
if (Test-Path $portsFile) {
  foreach ($l in Get-Content $portsFile) {
    $f = $l -split ' '
    $ports[$f[0]] = @{ pin = $f[1]; bank = $f[2]; func = $f[3]; dir = $f[4]; std = $f[5]; slew = $f[6]; drive = $f[7] }
  }
}
foreach ($r in $sig) {
  $p = $ports[$r.hdl_port]
  if (-not $p) { Bad "$($r.hdl_port) (pin $($r.pin)) missing from Vivado's port list"; continue }
  if ($p.pin -ne $r.fpga_ball) { Bad "$($r.hdl_port) (pin $($r.pin)) is on ball $($p.pin); the pin list says $($r.fpga_ball)" }
  $csvBank = ($r.fpga_bank -split '\D')[0]
  if ($p.bank -ne $csvBank) { Bad "$($r.hdl_port) (pin $($r.pin)) is in bank $($p.bank); the pin list says $($r.fpga_bank)" }
  $expDir = @{ in = 'IN'; out = 'OUT'; io = 'INOUT' }[$r.dir]
  if ($p.dir -ne $expDir) { Bad "$($r.hdl_port) direction $($p.dir), expected $expDir" }
  if ($p.std -ne 'LVCMOS33') { Bad "$($r.hdl_port) IOSTANDARD $($p.std), expected LVCMOS33" }
  if ($r.dir -ne 'in' -and ($p.slew -ne 'SLOW' -or $p.drive -ne '8')) { Bad "$($r.hdl_port) slew $($p.slew) drive $($p.drive), expected SLOW 8" }
}
$extra = @($ports.Keys | Where-Object { $_ -notin $sig.hdl_port })
if ($extra) { Bad "ports not in the pin list: $($extra -join ', ')" }

# ---- balls: Vivado's pin function against the Au net name
$ballInfo = @{}
$ballsFile = Join-Path $WorkDir 'balls.txt'
if (Test-Path $ballsFile) { foreach ($l in Get-Content $ballsFile) { $f = $l -split ' '; $ballInfo[$f[0]] = @{ bank = $f[1]; func = $f[2] } } }
$checked = 0
foreach ($r in $rows) {
  $ball = $r.fpga_ball; $net = $r.au_net; $where = "pin $($r.pin) ($($r.header)-$($r.hole))"
  if (-not $ball -and $net -match '^([A-Z]\d+)_1V35$') {
    $b = $ballInfo[$matches[1]]
    if (-not $b -or $b.bank -ne '15') { Bad "${where}: $net should be a bank 15 (1.35V) ball; Vivado: $($b.bank) $($b.func)" }
    $checked++; continue
  }
  if (-not $ball) { continue }
  $b = $ballInfo[$ball]
  if (-not $b -or $b.bank -eq 'NONE') { Bad "${where}: ball $ball isn't a pin of $Part"; continue }
  if ($net -match '^(\d+)_L(\d+)_(?:(MRCC|SRCC|AD\d+)_)?([PN])$') {
    $nb = $matches[1]; $nl = $matches[2]; $ns = $matches[3]; $np = $matches[4]
    if ($b.func -notmatch '^IO_L(\d+)([PN])_T\d(?:_(.*))?_(\d+)$') { Bad "${where}: ball $ball is $($b.func), not the pair pin the Au net $net names"; continue }
    if ($matches[1] -ne $nl -or $matches[2] -ne $np -or $matches[4] -ne $nb) { Bad "${where}: ball $ball is $($b.func); the Au net is $net" }
    elseif ($ns -eq 'MRCC' -and $b.func -notmatch 'MRCC') { Bad "${where}: the Au net $net says MRCC; ball $ball is $($b.func)" }
    elseif ($ns -match '^AD(\d+)$' -and $b.func -notmatch "AD$($matches[1])$np") { Bad "${where}: the Au net $net; ball $ball is $($b.func)" }
  } elseif ($net -match '^(\d+)_([A-Z]\d+)(?:_\w+)?$') {
    if ($matches[1] -ne $b.bank -or $matches[2] -ne $ball) { Bad "${where}: the Au net $net, but ball $ball is in bank $($b.bank)" }
  } elseif ($auLedBall.ContainsKey($net)) {
    if ($auLedBall[$net] -ne $ball) { Bad "${where}: $net is ball $($auLedBall[$net]) on the Au schematic; the pin list says $ball" }
    elseif ($b.bank -ne '14') { Bad "${where}: $net ball $ball is in bank $($b.bank), not 14" }
  } else { Bad "${where}: can't read the Au net name '$net'" }
  $checked++
}

$clk = $ports['cpu_clk']
'ports: {0} placed; banks {1}; cpu_clk on {2} ({3})' -f $ports.Count, (($ports.Values | Group-Object { $_.bank } | Sort-Object Name | ForEach-Object { "$($_.Name)x$($_.Count)" }) -join ' '), $clk.pin, $clk.func
"balls: $checked checked against the Au net names and schematic"
if ($bad) { "FAIL: $bad problem(s). Vivado's files are in $WorkDir"; exit 1 }
"PASS. Vivado's files are in $WorkDir"
exit 0
