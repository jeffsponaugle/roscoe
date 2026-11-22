<#
  gen-diptrace.ps1 - write Roscoe_BrWide.libxml, a DipTrace pattern library (DipTrace XML, Pattern Editor dialect)
  holding one pattern: the footprint Roscoe needs under an Alchitry Br Wide V2.

  Source: BrWide_pinlist.csv (run gen-pinlist.ps1 first). Then:
      powershell -ExecutionPolicy Bypass -File fpga\gen-diptrace.ps1
  Load it in DipTrace's Pattern Editor with Library > Import > DipXML.

  What the pattern holds:
  - 136 through-hole pads, numbered 1-136 (Roscoe's Br pin numbers), at the hole positions from Alchitry's
    BrWide.step. Drill 1.016, pad 1.88 (the Br's own sizes); pad 1 is square. Each pad's Note is its Br hole and net.
  - The four 2.2 mm mounting holes (shared with the Au), keepout 4.0.
  - Top Silk: socket outlines around the three hole fields and the first pin number of each field.
  - Top Courtyard: the same three outlines plus 0.25.
  - Top Assy: the Br outline, the Au underneath, and labels.
  Pattern origin (0,0) = centre of the mounting-hole rectangle = centre of the Br's 70 x 55 outline box, which is
  (35.00, 27.50) in the CSV frame. Seen from above with the Br fitted silkscreen side up: Bank B on the left,
  Control and Bank A on the right, the Br's logo edge (+Y) at the top. DipTrace XML is Y-up, so nothing is mirrored.
#>
param(
  [string]$PinList = (Join-Path $PSScriptRoot 'BrWide_pinlist.csv'),
  [string]$Out     = (Join-Path $PSScriptRoot 'Roscoe_BrWide.libxml'),
  [double]$Drill   = 1.016,
  [double]$PadDia  = 1.88
)
$ErrorActionPreference = 'Stop'
$inv = [System.Globalization.CultureInfo]::InvariantCulture
$rows = @(Import-Csv $PinList | Sort-Object { [int]$_.pin })
if ($rows.Count -ne 136) { throw "Expected 136 holes in $PinList, found $($rows.Count)" }
for ($i = 0; $i -lt 136; $i++) { if ([int]$rows[$i].pin -ne $i + 1) { throw "Pin numbers are not 1..136 (row $i has $($rows[$i].pin))" } }

$cx = 35.0; $cy = 27.5                     # CSV frame -> pattern frame
function N([double]$v) { $v.ToString('0.###', $inv) }
function PX([double]$x) { N ($x - $cx) }
function PY([double]$y) { N ($y - $cy) }

# stable library id from the name
$md5 = [System.Security.Cryptography.MD5]::Create()
$uid = [BitConverter]::ToUInt32($md5.ComputeHash([Text.Encoding]::UTF8.GetBytes('Roscoe_BrWide_V2')), 0)

$ws = New-Object System.Xml.XmlWriterSettings
$ws.Indent = $true; $ws.IndentChars = '  '; $ws.NewLineChars = "`n"; $ws.Encoding = New-Object System.Text.UTF8Encoding($false)
$sbOut = New-Object System.IO.MemoryStream
$w = [System.Xml.XmlWriter]::Create($sbOut, $ws)
function Attr([string]$n, $v) { $w.WriteAttributeString($n, [string]$v) }

$w.WriteStartDocument()
$w.WriteStartElement('Library')
Attr 'Type' 'DipTrace-PatternLibrary'; Attr 'Name' 'Roscoe Br Wide'
Attr 'Hint' 'Alchitry Br Wide V2 socket for Roscoe (Au underneath), pins 1-136'
Attr 'Version' '5.3.5.0'; Attr 'UID32' $uid; Attr 'Units' 'mm'

# ---- pad styles
$w.WriteStartElement('PadStyles')
foreach ($ps in @(@('BrWidePad', 'Ellipse'), @('BrWidePad1', 'Rectangle'))) {
  $w.WriteStartElement('PadStyle')
  Attr 'Name' $ps[0]; Attr 'Type' 'Through'; Attr 'HoleType' 'Round'; Attr 'Hole' (N $Drill)
  $w.WriteStartElement('MainStack'); Attr 'Shape' $ps[1]; Attr 'Width' (N $PadDia); Attr 'Height' (N $PadDia); $w.WriteEndElement()
  $w.WriteEndElement()
}
$w.WriteEndElement()

# ---- the pattern
$w.WriteStartElement('Patterns')
$w.WriteStartElement('Pattern')
Attr 'Id' 0; Attr 'RefDes' 'J'; Attr 'Mounting' 'Through'; Attr 'Width' '70'; Attr 'Height' '55'; Attr 'Orientation' '0'
Attr 'LockTypeChange' 'N'; Attr 'Type' 'Free'; Attr 'Float1' '0'; Attr 'Float2' '0'; Attr 'Float3' '0'; Attr 'Int1' '0'; Attr 'Int2' '0'
$w.WriteElementString('Name', 'Roscoe_BrWide_V2')
$w.WriteElementString('Name_Description', 'Alchitry Br Wide V2 with the Au plugged in underneath. Roscoe pins 1-136: pin = 14*(J-7)+hole for J7-J14, 112+6*(J-17)+hole for J17-J20. Origin = centre of the mounting-hole rectangle.')
$w.WriteStartElement('Value'); $w.WriteEndElement()
$w.WriteStartElement('DefPad'); Attr 'Style' 'BrWidePad'; $w.WriteEndElement()

$w.WriteStartElement('Pads')
foreach ($r in $rows) {
  $pin = [int]$r.pin
  $first = ($pin -eq 1)
  $w.WriteStartElement('Pad')
  Attr 'Id' ($pin - 1); Attr 'Style' ($(if ($first) { 'BrWidePad1' } else { 'BrWidePad' }))
  Attr 'X' (PX ([double]::Parse($r.x_mm, $inv))); Attr 'Y' (PY ([double]::Parse($r.y_mm, $inv)))
  Attr 'Angle' '0'; Attr 'Locked' 'N'; Attr 'Side' 'Top'; Attr 'DefStyle' ($(if ($first) { 'N' } else { 'Y' })); Attr 'Group' '-1'
  $w.WriteElementString('Number', [string]$pin)
  $w.WriteElementString('Note', ('{0}-{1} {2}' -f $r.header, $r.hole, $r.net))
  $w.WriteEndElement()
}
$w.WriteEndElement()

# ---- shapes
$script:sid = 0
function Shape([string]$type, [string]$layer, [double[][]]$pts) {
  $w.WriteStartElement('Shape')
  Attr 'Id' $script:sid; Attr 'Type' $type; Attr 'Locked' 'N'; Attr 'Layer' $layer; Attr 'Group' '-1'
  $w.WriteStartElement('Points')
  foreach ($p in $pts) { $w.WriteStartElement('Point'); Attr 'X' (PX $p[0]); Attr 'Y' (PY $p[1]); $w.WriteEndElement() }
  $w.WriteEndElement(); $w.WriteEndElement(); $script:sid++
}
function Text([string]$layer, [double]$x, [double]$y, [string]$text, [string]$horz = 'Center', [double]$size = 3, [double]$angle = 0) {
  $w.WriteStartElement('Shape')
  Attr 'Id' $script:sid; Attr 'Type' 'Text'; Attr 'Locked' 'N'; Attr 'Layer' $layer; Attr 'FontVector' 'Y'
  Attr 'FontSize' ([int][math]::Round($size)); Attr 'FontSizeFloat' (N $size); Attr 'FontScale' '1'; Attr 'FontWidth' '-2'
  Attr 'TextShow' 'Any Text'; Attr 'HorzAlign' $horz; Attr 'VertAlign' 'Center'; Attr 'TextAlign' 'Center'; Attr 'LineSpacing' '1.2'
  Attr 'Angle' (N $angle); Attr 'AllLayers' 'N'; Attr 'Group' '-1'
  $w.WriteStartElement('TextLines'); $w.WriteElementString('TextLine', $text); $w.WriteEndElement()
  $w.WriteStartElement('Points'); $w.WriteStartElement('Point'); Attr 'X' (PX $x); Attr 'Y' (PY $y); $w.WriteEndElement(); $w.WriteEndElement()
  $w.WriteEndElement(); $script:sid++
}
# field extents from the pin list
function Field([string[]]$hdrs) {
  $sel = $rows | Where-Object { $_.header -in $hdrs }
  $xs = $sel | ForEach-Object { [double]::Parse($_.x_mm, $inv) }; $ys = $sel | ForEach-Object { [double]::Parse($_.y_mm, $inv) }
  return @((($xs | Measure-Object -Minimum).Minimum), (($xs | Measure-Object -Maximum).Maximum), (($ys | Measure-Object -Minimum).Minimum), (($ys | Measure-Object -Maximum).Maximum))
}
$fields = [ordered]@{ B = (Field 'J11','J12','J13','J14'); A = (Field 'J7','J8','J9','J10'); C = (Field 'J17','J18','J19','J20') }
$w.WriteStartElement('Shapes')
foreach ($k in $fields.Keys) {
  $f = $fields[$k]; $m = 1.27
  Shape 'Rectangle' 'Top Silk' @(@(($f[0] - $m), ($f[2] - $m)), @(($f[1] + $m), ($f[3] + $m)))
}
foreach ($k in $fields.Keys) {
  $f = $fields[$k]; $m = 1.52
  Shape 'Rectangle' 'Top Courtyard' @(@(($f[0] - $m), ($f[2] - $m)), @(($f[1] + $m), ($f[3] + $m)))
}
# first pin of each field
Text 'Top Silk' 58.0 35.26 '1' 'Right'
Text 'Top Silk' 2.06 37.75 '57'
Text 'Top Silk' 60.26 54.15 '113'
# Br outline (from BrWide.step), the Au underneath, labels
$outline = @(@(1.5,0), @(19,0), @(20.5,1.5), @(20.5,4), @(22,5.5), @(48,5.5), @(49.5,4), @(49.5,1.5), @(51,0), @(68.5,0),
             @(70,1.5), @(70,53.5), @(68.5,55), @(1.5,55), @(0,53.5), @(0,1.5), @(1.5,0))
Shape 'Polyline' 'Top Assy' $outline
Shape 'Rectangle' 'Top Assy' @(@(12.5, 0), @(57.5, 55))
Text 'Top Assy' 35 30 'Au V2 underneath, 4 mm below the Br'
Text 'Top Assy' 35 26.5 'keep parts here low'
Text 'Top Assy' 47.65 53 'Au USB-C'
Text 'Top Assy' 6 41 'BANK B 57-112'
Text 'Top Assy' 64.1 -1.8 'BANK A 1-56'
Text 'Top Assy' 64.1 56.8 'CONTROL 113-136'
$w.WriteEndElement()

# ---- mounting holes (the Au's own, through both boards)
$w.WriteStartElement('Holes')
$mh = @(@(15, 2.5), @(15, 52.5), @(55, 2.5), @(55, 52.5))
for ($i = 0; $i -lt 4; $i++) {
  $w.WriteStartElement('Hole'); Attr 'Id' $i; Attr 'Locked' 'N'; Attr 'X' (PX $mh[$i][0]); Attr 'Y' (PY $mh[$i][1])
  Attr 'Diam' '4'; Attr 'HoleDiam' '2.2'; Attr 'Group' '-1'; $w.WriteEndElement()
}
$w.WriteEndElement()

$w.WriteEndElement()   # Pattern
$w.WriteEndElement()   # Patterns
$w.WriteEndElement()   # Library
$w.WriteEndDocument(); $w.Flush(); $w.Close()
$xmlText = [System.Text.Encoding]::UTF8.GetString($sbOut.ToArray()).Replace('<?xml version="1.0" encoding="utf-8"?>', '<?xml version="1.0" encoding="UTF-8"?>')
[System.IO.File]::WriteAllText($Out, $xmlText + "`n", (New-Object System.Text.UTF8Encoding($false)))
'Wrote {0}: 136 pads, {1} shapes, 4 mounting holes (pattern origin = CSV (35.00, 27.50))' -f $Out, $script:sid
