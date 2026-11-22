<#
  gen-pinlist.ps1 - write BrWide_pinlist.csv: every hole of the Alchitry Br Wide V2, with its silkscreen label,
  Au pin, FPGA ball and bank, the Roscoe signal on it, and its position for the Roscoe PCB pattern.

  Source: Roscoe_FPGA_pinmap_AuV2.xlsx, sheets "Br Wide footprint" (holes, labels, positions) and "FPGA pins"
  (balls, ports, translators). After changing the workbook, run:
      powershell -ExecutionPolicy Bypass -File fpga\gen-pinlist.ps1

  Pin numbers (1-136) are Roscoe's single numbering for the Br connector, for the schematic symbol and the PCB
  pattern alike: pin = 14*(J-7) + hole for J7-J14 (1-112), pin = 112 + 6*(J-17) + hole for J17-J20 (113-136).
  On the board every field then reads the same way: columns left to right, holes top to bottom.

  Positions (x_mm, y_mm): looking down on Roscoe with the Br fitted silkscreen side up (the Au hangs underneath),
  Alchitry logo at the top and the notch at the bottom. Origin at the lower-left corner of the Br's 70 x 55 mm
  outline box; X right, Y up. They come from Alchitry's BrWide.step. step_x/step_y are the same holes in that
  model's own frame (the Au's frame: mounting holes at 2.5/52.5 mm).
#>
param(
  [string]$Workbook = (Join-Path $PSScriptRoot 'Roscoe_FPGA_pinmap_AuV2.xlsx'),
  [string]$Out      = (Join-Path $PSScriptRoot 'BrWide_pinlist.csv')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $Workbook).Path)
function Read-Entry([string]$name) {
  $e = $zip.GetEntry($name); if (-not $e) { return $null }
  $sr = New-Object System.IO.StreamReader($e.Open()); $t = $sr.ReadToEnd(); $sr.Close(); return $t
}
function ColIndex([string]$ref) { $n = 0; foreach ($ch in ($ref -replace '\d', '').ToCharArray()) { $n = $n * 26 + ([int][char]$ch - 64) }; $n }
try {
  $shared = @()
  $ssText = Read-Entry 'xl/sharedStrings.xml'
  if ($ssText) { $ss = [xml]$ssText; foreach ($si in $ss.sst.si) { $shared += $si.InnerText } }
  $wb   = [xml](Read-Entry 'xl/workbook.xml')
  $rels = [xml](Read-Entry 'xl/_rels/workbook.xml.rels')
  function Read-Sheet([string]$sheetName) {
    $sheet = $wb.workbook.sheets.sheet | Where-Object { $_.name -eq $sheetName }
    if (-not $sheet) { throw "No '$sheetName' sheet in $Workbook" }
    $rid = $sheet.GetAttribute('id', 'http://schemas.openxmlformats.org/officeDocument/2006/relationships')
    $target = ($rels.Relationships.Relationship | Where-Object { $_.Id -eq $rid }).Target -replace '^/?xl/', ''
    $sx = [xml](Read-Entry ('xl/' + $target))
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
    return ,$rows
  }
  $br  = Read-Sheet 'Br Wide footprint'
  $fp  = Read-Sheet 'FPGA pins'
} finally { $zip.Dispose() }

$expectBr = 'Header','Hole','Au pin','Signal','Roscoe connection','Silkscreen','X (mm)','Y (mm)','Pin'
for ($i = 0; $i -lt $expectBr.Count; $i++) { if ($br[0][$i + 1] -ne $expectBr[$i]) { throw "Br Wide footprint column $($i + 1): '$($br[0][$i + 1])', expected '$($expectBr[$i])'" } }
$expectFp = 'Br header','Hole','Au pin','Ball','Bank','Au net','HDL port','Dir','Group','Carrier part','Ch','5V-side net / function','Notes','Net'
for ($i = 0; $i -lt $expectFp.Count; $i++) { if ($fp[0][$i + 1] -ne $expectFp[$i]) { throw "FPGA pins column $($i + 1): '$($fp[0][$i + 1])', expected '$($expectFp[$i])'" } }
function PinOf([string]$h, [int]$hole) {
  $j = [int]$h.Substring(1)
  if ($j -ge 7 -and $j -le 14) { return 14 * ($j - 7) + $hole }
  if ($j -ge 17 -and $j -le 20) { return 112 + 6 * ($j - 17) + $hole }
  throw "No pin number for $h"
}

$pins = @{}
foreach ($v in $fp[1..($fp.Count - 1)]) { $pins['{0}-{1}' -f $v[1], $v[2]] = $v }

$inv = [System.Globalization.CultureInfo]::InvariantCulture
$list = New-Object System.Collections.Generic.List[object]
foreach ($v in $br[1..($br.Count - 1)]) {
  $key = '{0}-{1}' -f $v[1], $v[2]
  $p = $pins[$key]
  $x = [double]::Parse($v[7], $inv); $y = [double]::Parse($v[8], $inv)
  $pin = [int]$v[9]
  if ($pin -ne (PinOf $v[1] ([int]$v[2]))) { throw "$key : pin $pin does not match the numbering rule" }
  $port = ''; $dir = ''; $ball = ''; $bank = ''; $aunet = ''; $via = ''; $signal = $v[5]; $net = $v[4]
  if ($p) {
    if ($p[3] -ne $v[3]) { throw "$key : Au pin differs between sheets ('$($p[3])' vs '$($v[3])')" }
    $ball = $p[4]; $bank = $p[5]; $aunet = $p[6]
    if ($p[8] -in 'in', 'out', 'io') {
      $port = $p[7]; $dir = $p[8]; $net = $p[14]
      $ch = if ($p[11] -match '^\d+$') { " ch$($p[11])" } elseif ($p[11]) { " $($p[11])" } else { '' }
      $via = "$($p[10])$ch"
      $signal = $p[12]
      if (-not $v[4].StartsWith($port)) { throw "$key : signal differs between sheets ('$port' vs '$($v[4])')" }
    } elseif ($p[9] -eq 'SPARE') { $port = 'spare'; $net = 'spare' } elseif ($p[9] -eq 'DEAD') { $port = '(1.35V, unusable)'; $net = 'NC (1.35V)' }
  } else {
    $port = $v[4]
  }
  $list.Add([pscustomobject][ordered]@{
    pin = $pin; header = $v[1]; hole = [int]$v[2]; silkscreen = $v[6]; net = $net; au_pin = $v[3]
    fpga_ball = $ball; fpga_bank = $bank; au_net = $aunet; hdl_port = $port; dir = $dir; roscoe_signal = $signal; via = $via
    x_mm = $x.ToString('0.00', $inv); y_mm = $y.ToString('0.00', $inv)
    step_x = (55 - $y).ToString('0.00', $inv); step_y = ($x - 12.5).ToString('0.00', $inv)
  })
}
$list = [System.Collections.Generic.List[object]]@($list | Sort-Object pin)
# Sanity: 136 holes, no two at one position, every FPGA-pins row placed.
if ($list.Count -ne 136) { throw "Expected 136 holes, found $($list.Count)" }
$dup = $list | Group-Object { "$($_.x_mm),$($_.y_mm)" } | Where-Object Count -gt 1
if ($dup) { throw "Two holes share a position: $($dup.Name -join '; ')" }
foreach ($k in $pins.Keys) { if (-not ($list | Where-Object { "$($_.header)-$($_.hole)" -eq $k })) { throw "FPGA pins row $k has no Br hole" } }

$csv = $list | ConvertTo-Csv -NoTypeInformation
[System.IO.File]::WriteAllText($Out, (($csv -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding($false)))
$sig = @($list | Where-Object { $_.dir }).Count
'Wrote {0}: {1} holes, {2} carrying FPGA signals' -f $Out, $list.Count, $sig
