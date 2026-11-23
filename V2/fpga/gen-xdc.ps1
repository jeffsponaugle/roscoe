<#
  gen-xdc.ps1 - generate roscoe_busctrl_au.xdc from Roscoe_FPGA_pinmap_AuV2.xlsx ("FPGA pins" sheet).

  The workbook is the source of truth for the FPGA pin assignment. After changing it, run:
      powershell -ExecutionPolicy Bypass -File fpga\gen-xdc.ps1
  No Excel or Python needed: the .xlsx is unzipped and its sheet XML read directly.
#>
param(
  [string]$Workbook = (Join-Path $PSScriptRoot 'Roscoe_FPGA_pinmap_AuV2.xlsx'),
  [string]$Out      = (Join-Path $PSScriptRoot 'roscoe_busctrl_au.xdc'),
  [double]$CpuClkPeriodNs = 62.5     # 16 MHz Y1; use 83.333 for 12 MHz
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# ---- read the "FPGA pins" sheet -------------------------------------------------------------
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $Workbook).Path)
function Read-Entry([string]$name) {
  $e = $zip.GetEntry($name); if (-not $e) { return $null }
  $sr = New-Object System.IO.StreamReader($e.Open()); $t = $sr.ReadToEnd(); $sr.Close(); return $t
}
try {
  $shared = @()
  $ssText = Read-Entry 'xl/sharedStrings.xml'
  if ($ssText) { $ss = [xml]$ssText; foreach ($si in $ss.sst.si) { $shared += $si.InnerText } }
  $wb   = [xml](Read-Entry 'xl/workbook.xml')
  $rels = [xml](Read-Entry 'xl/_rels/workbook.xml.rels')
  $sheet = $wb.workbook.sheets.sheet | Where-Object { $_.name -eq 'FPGA pins' }
  if (-not $sheet) { throw "No 'FPGA pins' sheet in $Workbook" }
  $rid = $sheet.GetAttribute('id', 'http://schemas.openxmlformats.org/officeDocument/2006/relationships')
  $target = ($rels.Relationships.Relationship | Where-Object { $_.Id -eq $rid }).Target -replace '^/?xl/', ''
  $sx = [xml](Read-Entry ('xl/' + $target))
} finally { $zip.Dispose() }

function ColIndex([string]$ref) { $n = 0; foreach ($ch in ($ref -replace '\d', '').ToCharArray()) { $n = $n * 26 + ([int][char]$ch - 64) }; $n }
$rows = @()
foreach ($row in $sx.worksheet.sheetData.row) {
  $v = @{}
  foreach ($c in $row.c) {
    $t = $c.GetAttribute('t'); $val = ''
    if ($t -eq 'inlineStr') { $val = $c.is.InnerText } elseif ($t -eq 's') { $val = $shared[[int]$c.v] } elseif ($c.v) { $val = [string]$c.v }
    $v[(ColIndex $c.r)] = $val
  }
  $rows += ,$v
}
$hdr = $rows[0]
$expect = 'Br header','Hole','Au pin','Ball','Bank','Au net','HDL port','Dir','Group','Carrier part','Ch','5V-side net / function'
for ($i = 0; $i -lt $expect.Count; $i++) { if ($hdr[$i + 1] -ne $expect[$i]) { throw "Unexpected column $($i + 1): '$($hdr[$i + 1])', expected '$($expect[$i])'" } }

$sig = @()
foreach ($v in $rows[1..($rows.Count - 1)]) {
  $dir = $v[8]
  if ($dir -notin 'in', 'out', 'io') { continue }          # SPARE and DEAD rows
  $sig += [pscustomobject]@{ Hdr = $v[1]; Hole = $v[2]; AuPin = $v[3]; Ball = $v[4]; Bank = $v[5]; Port = $v[7]
                             Dir = $dir; Group = $v[9]; Part = $v[10]; Ch = $v[11]; Net = $v[12] }
}

# ---- sanity checks --------------------------------------------------------------------------
$dupPort = $sig | Group-Object Port | Where-Object Count -gt 1
$dupBall = $sig | Group-Object Ball | Where-Object Count -gt 1
if ($dupPort) { throw "Port used twice: $($dupPort.Name -join ', ')" }
if ($dupBall) { throw "Ball used twice: $($dupBall.Name -join ', ')" }
foreach ($s in $sig) { if (-not $s.Ball -or -not $s.Port) { throw "Row $($s.Hdr)-$($s.Hole) has no ball or port" } }
if ($sig | Where-Object { $_.Bank -match '^15' }) { throw 'A signal is assigned to the 1.35V bank 15' }
# The control-header pins (Br J17-J20) also drive the Au LEDs, and have no ground: slow inputs only.
foreach ($s in $sig | Where-Object { $_.Hdr -match '^J(17|18|19|20)$' }) {
  if ($s.Dir -ne 'in') { throw "Control-header pin $($s.Hdr)-$($s.Hole) ($($s.Port)) must be an input" }
}

# ---- write the XDC --------------------------------------------------------------------------
$groupOrder = 'CLK','IN-A','IN-B','IN-C','IN-D','IN-E','IRQ','DATA','OUT-CPU','BERR','IPL','DRAM-1','DRAM-2','MEM-1','MEM-2','BURST','DEV'
$groupText = @{
  'CLK'     = 'CPU clock: Y1 -> U_CLK (74LVC1G34) -> 33R -> MRCC pin A41. Keep A39 (its N partner) empty, A47 static, A33/A45 slow or static.'
  'IN-A'    = 'Address A8-A15 (U_IN1, 74LVC244A)'
  'IN-B'    = 'Address A16-A23 (U_IN2)'
  'IN-C'    = 'Address A24-A31 (U_IN3)'
  'IN-D'    = 'A0, A1, SIZ0/1, FC0-2, R/W (U_IN4)'
  'IN-E'    = 'AS, CBREQ, SYS_RESET, EXP_ACK, A2, A3, IDE IORDY, power request (U_IN5)'
  'IRQ'     = 'Interrupt inputs from U_IN6 (100R series) on the Au LED0-7 pins (control headers J17-J20); each pin also drives an Au LED through 330R. Polarity is handled in the FPGA.'
  'DATA'    = 'D24-D31 through U_DATA (74LVC245A, DIR = CPU R/W), and its /OE'
  'OUT-CPU' = 'U_CPU (74LVC244A): DSACK0/1 three-stated with their shared enable (group 1); STERM, CIIN, CBACK always driven (group 2)'
  'BERR'    = 'BERR, push-pull through U_MEM2 (74AHCT244), as BUSCTRL drives it today'
  'IPL'     = 'IPL0-2 through U_MEM2 (74AHCT244)'
  'DRAM-1'  = 'RAS0-7 through U_DRAM1 (74AHCT244)'
  'DRAM-2'  = 'CAS0-3, DRAM WE, MUX and the data-buffer /OE through U_DRAM2 (74AHCT244); the buffer DIR is CPU R/W'
  'MEM-1'   = 'SRAM CE0-3 and byte enables through U_MEM1 (74AHCT244)'
  'MEM-2'   = 'Flash CE through U_MEM2 (74AHCT244); the flash /WE is CPU R/W. (The 68882 select is a 74AHCT138 on the 5V side.)'
  'BURST'   = 'SRAM burst address BA3:BA2 (CPU A3:A2 plus the burst count) through U_MEM2 ch4 and U_DRAM2 ch7 (74AHCT244) to riser pins A18/B18'
  'DEV'     = 'Device code bus, wired straight to IOCTRL (ATF1504, TTL inputs)'
}
function PortKey([string]$p) { if ($p -match '^(.*)\[(\d+)\]$') { '{0}[{1:D3}]' -f $matches[1], [int]$matches[2] } else { $p } }
# Roscoe's pin number for the Br connector (1-136), as in BrWide_pinlist.csv and the schematic.
function PinOf([string]$h, [int]$hole) {
  $j = [int]$h.Substring(1)
  if ($j -ge 7 -and $j -le 14) { return 14 * ($j - 7) + $hole }
  if ($j -ge 17 -and $j -le 20) { return 112 + 6 * ($j - 17) + $hole }
  throw "No pin number for $h"
}

$sb = New-Object System.Text.StringBuilder
function L([string]$s = '') { [void]$sb.AppendLine($s) }
$nIn = @($sig | Where-Object Dir -eq 'in').Count; $nOut = @($sig | Where-Object Dir -eq 'out').Count; $nIo = @($sig | Where-Object Dir -eq 'io').Count
L '## roscoe_busctrl_au.xdc - Roscoe 68030 core logic on an Alchitry Au V2 (XC7A35T-FTG256) on a Br Wide V2.'
L '##'
L '## GENERATED by gen-xdc.ps1 from Roscoe_FPGA_pinmap_AuV2.xlsx ("FPGA pins" sheet). Do not edit by hand:'
L '## change the workbook, then run   powershell -ExecutionPolicy Bypass -File fpga\gen-xdc.ps1'
L ('## {0} signals: {1} inputs, {2} outputs, {3} bidirectional. Design notes: Roscoe_FPGA_Redesign.md.' -f $sig.Count, $nIn, $nOut, $nIo)
L '##'
L '## Every pin is LVCMOS33. Bank 34 is only at 3.3V while VBSEL_A/B (Br J19-4, J20-4) are left unconnected.'
L '## Outputs use SLEW SLOW / DRIVE 8: the 0.1" headers have one ground per 13 signals (section 12).'
L ''
L '## ---- Configuration: CFGBVS is tied to VCCO on the Au; Master SPI x4 from the on-board QSPI flash ----'
L 'set_property CFGBVS VCCO [current_design]'
L 'set_property CONFIG_VOLTAGE 3.3 [current_design]'
L 'set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]'
L 'set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]'
L ''
L '## ---- Clocks ----'
L ('## CPU_CLK period for the Y1 fitted today ({0:0.###} MHz). Regenerate with -CpuClkPeriodNs for another oscillator.' -f (1000 / $CpuClkPeriodNs))
L ('create_clock -period {0:0.000} -name cpu_clk [get_ports cpu_clk]' -f $CpuClkPeriodNs)
L ''
L '## ---- I/O timing ----'
L '## In roscoe_timing.xdc (hand-written; read it after this file): set_input_delay / set_output_delay against'
L '## cpu_clk from the 68030 bus timing and the translator delays (Roscoe_FPGA_Redesign.md section 16).'
L ''
L '## ---- Pins:  port  <-  Roscoe pin (1-136), Br hole, Au pin, carrier part and channel, 5V-side net ----'
foreach ($g in $groupOrder + @($sig.Group | Where-Object { $_ -notin $groupOrder } | Select-Object -Unique)) {
  $members = @($sig | Where-Object Group -eq $g | Sort-Object { PortKey $_.Port })
  if ($members.Count -eq 0) { continue }
  L ''
  L ('## ' + $(if ($groupText.ContainsKey($g)) { $groupText[$g] } else { $g }))
  foreach ($s in $members) {
    $attrs = "PACKAGE_PIN $($s.Ball) IOSTANDARD LVCMOS33"
    if ($s.Dir -in 'out', 'io') { $attrs += ' SLEW SLOW DRIVE 8' }
    $cmd = 'set_property -dict {{{0}}} [get_ports {{{1}}}]' -f $attrs, $s.Port
    $ch = if ($s.Ch -match '^\d+$') { " ch$($s.Ch)" } elseif ($s.Ch) { " $($s.Ch)" } else { '' }
    $where = 'pin {5,3}  {0}-{1} {2} {3}{4}' -f $s.Hdr, $s.Hole, $s.AuPin, (($s.Part -split ' ')[0]), $ch, (PinOf $s.Hdr ([int]$s.Hole))
    L ('{0,-92} ;# {1,-36} {2}' -f $cmd, $where, $s.Net)
  }
}
L ''
L '## ---- Au on-board pins (uncomment the ones the design uses) ----'
L '## The Au LED0-7 pins carry the interrupt inputs above, so the LEDs show the raw interrupt lines; never drive them.'
L '# set_property -dict {PACKAGE_PIN N14 IOSTANDARD LVCMOS33} [get_ports {clk100}]      ;# 100 MHz oscillator (MRCC)'
L '# create_clock -period 10.000 -name clk100 [get_ports clk100]'
L '# set_property -dict {PACKAGE_PIN P6  IOSTANDARD LVCMOS33} [get_ports {au_rst_n}]    ;# Au reset button (Br J17-4, unused on Roscoe), 10K pull-up'
L '# set_property -dict {PACKAGE_PIN P15 IOSTANDARD LVCMOS33} [get_ports {usb_rx}]      ;# FT2232 UART, FPGA input'
L '# set_property -dict {PACKAGE_PIN P16 IOSTANDARD LVCMOS33} [get_ports {usb_tx}]      ;# FT2232 UART, FPGA output'

[System.IO.File]::WriteAllText($Out, $sb.ToString().Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding($false)))
'Wrote {0}: {1} signals ({2} in, {3} out, {4} io)' -f $Out, $sig.Count, $nIn, $nOut, $nIo
