<#
  gen-diptrace-symbol.ps1 - write Roscoe_BrWide.elixml, a DipTrace component library (Component Editor dialect)
  holding one component, Roscoe_BrWide_V2: the schematic symbol for the Alchitry Br Wide V2 with the Au
  underneath, with the Br footprint attached.

  Sources: BrWide_pinlist.csv (from gen-pinlist.ps1) for the pins, and Roscoe_BrWide.libxml (from gen-diptrace.ps1)
  for the footprint, which is copied in unchanged. Run those two first, then:
      powershell -ExecutionPolicy Bypass -File fpga\gen-diptrace-symbol.ps1

  The symbol has three parts (sections), one per Br hole field:
    Part 1  "Bank A"   pins 1-56     J7, J8 down the left side, J9, J10 down the right
    Part 2  "Bank B"   pins 57-112   J11, J12 left, J13, J14 right
    Part 3  "Control"  pins 113-136  J17, J18 left, J19, J20 right
  Each header's holes run top to bottom in hole order, under a label row that names the header (the top label
  row also holds the part's title). The label rows make each side's row count odd, so the body is centred on
  the part origin and every pin's free end lands on the 0.1-inch grid.

  Pins: number = pad number = Roscoe's Br pin number (1-136). Name = the net from the pin list, cleaned up:
    spare -> SPARE_<Au pin>, except A39 (the clock's partner, must stay empty) -> NC_A39
    NC (1.35V) -> NC_1V35, VCC (RAW in) -> VCC_RAW, VBSEL (= Au VBSEL_A) -> VBSEL_A,
    "+2.5V" (= Au VBSEL_B) -> VBSEL_B (the Br's silkscreen calls this hole +2.5V)
  ERC type: FPGA in/out/io -> Input/Output/Bidirectional; GND, AGND, +3V3, VCC_RAW, A1V8 -> Power; the rest
  (config, JTAG, analog, spare, NC) -> Undefined. CPU_CLK has the clock mark.

  DipTrace geometry used here (checked against Roscoe's own DipTrace symbols): a pin's X/Y is its body end, on
  the body edge, and the pin runs Length away from the body. Orientation 0 = a pin on the left side (free end to
  the left), 180 = right side. The XML is Y-up.
#>
param(
  [string]$PinList = (Join-Path $PSScriptRoot 'BrWide_pinlist.csv'),
  [string]$Pattern = (Join-Path $PSScriptRoot 'Roscoe_BrWide.libxml'),
  [string]$Out     = (Join-Path $PSScriptRoot 'Roscoe_BrWide.elixml')
)
$ErrorActionPreference = 'Stop'
$inv = [System.Globalization.CultureInfo]::InvariantCulture
function N([double]$v) { $v.ToString('0.###', $inv) }

# ---- pins
$rows = @(Import-Csv $PinList | Sort-Object { [int]$_.pin })
if ($rows.Count -ne 136) { throw "Expected 136 holes in $PinList, found $($rows.Count)" }
for ($i = 0; $i -lt 136; $i++) { if ([int]$rows[$i].pin -ne $i + 1) { throw "Pin numbers are not 1..136 (row $i has $($rows[$i].pin))" } }
$byHdr = @{}
foreach ($r in $rows) {
  if (-not $byHdr.ContainsKey($r.header)) { $byHdr[$r.header] = New-Object System.Collections.Generic.List[object] }
  $byHdr[$r.header].Add($r)
}

function PinName($r) {
  $n = $r.net
  if ($n -eq 'spare') { if ($r.au_pin -eq 'A39') { return 'NC_A39' } else { return 'SPARE_' + $r.au_pin } }
  if ($n -eq 'NC (1.35V)') { return 'NC_1V35' }
  if ($n -eq 'VCC (RAW in)') { return 'VCC_RAW' }
  if ($n -like '*Au VBSEL_A*') { return 'VBSEL_A' }
  if ($n -like '*Au VBSEL_B*') { return 'VBSEL_B' }
  if ($n -notmatch '^[A-Za-z0-9_+/]+$') { throw "Pin $($r.pin): net '$n' needs a pin-name rule in gen-diptrace-symbol.ps1" }
  return $n
}
function ErcType($r, [string]$name) {
  switch ($r.dir) { 'in' { return 'Input' } 'out' { return 'Output' } 'io' { return 'Bidirectional' } }
  if ($name -in 'GND', 'AGND', '+3V3', 'VCC_RAW', 'A1V8') { return 'Power' }
  return 'Undefined'
}

# ---- footprint, from the pattern library
[xml]$lib = Get-Content $Pattern -Raw -Encoding UTF8
$pats = @($lib.Library.Patterns.Pattern)
if ($pats.Count -ne 1) { throw "Expected one pattern in $Pattern, found $($pats.Count)" }
$pat = $pats[0]
$padId = @{}
foreach ($p in $pat.Pads.Pad) { $padId[[int]$p.Number] = [int]$p.Id }
foreach ($n in 1..136) { if (-not $padId.ContainsKey($n)) { throw "Pattern $Pattern has no pad $n" } }
$ps = $lib.CreateAttribute('PatternStyle'); $ps.Value = 'PatType0'; [void]$pat.Attributes.Prepend($ps)

# ---- symbol layout
$pitch = 2.54; $hx = 17.78; $len = 5.08          # body 35.56 wide; pins 0.2 in, free ends at +/-22.86
$parts = @(
  @{ Name = 'Bank A';  Title = 'BANK A 1-56';     Left = @('J7', 'J8');   Right = @('J9', 'J10') },
  @{ Name = 'Bank B';  Title = 'BANK B 57-112';   Left = @('J11', 'J12'); Right = @('J13', 'J14') },
  @{ Name = 'Control'; Title = 'CONTROL 113-136'; Left = @('J17', 'J18'); Right = @('J19', 'J20') }
)

$md5 = [System.Security.Cryptography.MD5]::Create()
$uid = [BitConverter]::ToUInt32($md5.ComputeHash([Text.Encoding]::UTF8.GetBytes('Roscoe_BrWide_V2 component')), 0)

$ws = New-Object System.Xml.XmlWriterSettings
$ws.Indent = $true; $ws.IndentChars = '  '; $ws.NewLineChars = "`n"; $ws.Encoding = New-Object System.Text.UTF8Encoding($false)
$buf = New-Object System.IO.MemoryStream
$w = [System.Xml.XmlWriter]::Create($buf, $ws)
function Attr([string]$n, $v) { $w.WriteAttributeString($n, [string]$v) }
function Text([int]$id, [double]$x, [double]$y, [string]$text, [double]$size) {
  $w.WriteStartElement('Shape')
  Attr 'Id' $id; Attr 'Type' 'Text'; Attr 'LineWidth' '0.254'; Attr 'Locked' 'N'; Attr 'FontVector' 'Y'; Attr 'FontMono' 'N'
  Attr 'FontSize' ([int][math]::Round($size)); Attr 'FontSizeFloat' (N $size); Attr 'FontColor' '0'; Attr 'TextShow' 'Any Text'
  Attr 'FontName' 'Tahoma'; Attr 'FontWidth' '-2'; Attr 'FontScale' '1'; Attr 'Angle' '0'
  Attr 'HorzAlign' 'Center'; Attr 'VertAlign' 'Center'; Attr 'TextAlign' 'Center'; Attr 'LineSpacing' '1.2'; Attr 'Group' '-1'
  $w.WriteStartElement('TextLines'); $w.WriteElementString('TextLine', $text); $w.WriteEndElement()
  $w.WriteStartElement('Points'); $w.WriteStartElement('Point'); Attr 'X' (N $x); Attr 'Y' (N $y); $w.WriteEndElement(); $w.WriteEndElement()
  $w.WriteEndElement()
}

$w.WriteStartDocument()
$w.WriteStartElement('Library')
Attr 'Type' 'DipTrace-ComponentLibrary'; Attr 'Name' 'Roscoe Br Wide'
Attr 'Hint' 'Alchitry Br Wide V2 (Au underneath) for Roscoe: 3-part symbol, pins 1-136, footprint attached'
Attr 'Version' '5.3.5.0'; Attr 'UID32' $uid; Attr 'Units' 'mm'

# nested pattern library: the attached footprint, exactly as in Roscoe_BrWide.libxml
$w.WriteStartElement('Library'); Attr 'Type' 'DipTrace-PatternLibrary'; Attr 'Units' $lib.Library.Units
$lib.Library.PadStyles.WriteTo($w)
$w.WriteStartElement('Patterns'); $pat.WriteTo($w); $w.WriteEndElement()
$w.WriteEndElement()

$w.WriteStartElement('Components')
$w.WriteStartElement('Component'); Attr 'Id' 0
$stats = @()
for ($pi = 0; $pi -lt $parts.Count; $pi++) {
  $pd = $parts[$pi]
  $n = $byHdr[$pd.Left[0]].Count
  foreach ($h in $pd.Left + $pd.Right) { if ($byHdr[$h].Count -ne $n) { throw "$($pd.Name): header $h has $($byHdr[$h].Count) holes, expected $n" } }
  $mid = $n + 1                                   # rows 0..2n+2: label, n pins, label, n pins, margin
  $top = ($mid + 1) * $pitch                      # body half-height
  function RowY([int]$row) { ($mid - $row) * $pitch }

  $w.WriteStartElement('Part')
  Attr 'Id' $pi
  if ($pi -eq 0) { Attr 'RefDes' 'J' }
  Attr 'PartType' 'Normal'; Attr 'ShowNumbers' 'Show'; Attr 'Type' 'Free'; Attr 'Int1' '0'; Attr 'Int2' '0'
  Attr 'Width' (N (2 * $hx)); Attr 'Height' (N (2 * $top)); Attr 'LockTypeChange' 'Y'
  if ($pi -eq 0) {
    $w.WriteStartElement('Pattern'); Attr 'Style' 'PatType0'; $w.WriteEndElement()
    $w.WriteElementString('Name', 'Roscoe_BrWide_V2')
  }
  $w.WriteElementString('PartName', $pd.Name)
  $w.WriteElementString('Value', '')
  $w.WriteStartElement('Origin'); Attr 'X' '0'; Attr 'Y' '0'; $w.WriteEndElement()
  if ($pi -eq 0) { $w.WriteElementString('Manufacturer', 'Alchitry') }

  $w.WriteStartElement('Pins')
  $pinId = 0
  foreach ($side in @(@($pd.Left, -$hx, '0'), @($pd.Right, $hx, '180'))) {
    for ($g = 0; $g -lt 2; $g++) {
      $first = $(if ($g -eq 0) { 1 } else { $n + 2 })
      foreach ($r in ($byHdr[$side[0][$g]] | Sort-Object { [int]$_.hole })) {
        $pin = [int]$r.pin; $name = PinName $r
        $w.WriteStartElement('Pin')
        Attr 'Id' $pinId; Attr 'X' (N $side[1]); Attr 'Y' (N (RowY ($first + [int]$r.hole - 1))); Attr 'Locked' 'N'
        Attr 'Type' ($(if ($name -eq 'CPU_CLK') { 'Clock' } else { 'Default' })); Attr 'ElectricType' (ErcType $r $name)
        Attr 'Orientation' $side[2]; Attr 'PadId' $padId[$pin]; Attr 'Length' (N $len); Attr 'ShowName' 'Y'
        Attr 'NumXShift' '0'; Attr 'NumYShift' '0'; Attr 'NameXShift' '0'; Attr 'NameYShift' '0'; Attr 'SignalDelay' '0'
        Attr 'NumOrientation' '0'; Attr 'NameOrientation' '0'
        $w.WriteElementString('Name', $name)
        $w.WriteElementString('PadNumber', [string]$pin)
        $w.WriteStartElement('NameFont'); Attr 'Size' '5'; Attr 'FontSizeFloat' '5'; Attr 'Width' '-2'; Attr 'Scale' '1'; Attr 'FontMono' 'N'; $w.WriteEndElement()
        $w.WriteEndElement()
        $pinId++
      }
    }
  }
  $w.WriteEndElement()

  $w.WriteStartElement('Shapes')
  $w.WriteStartElement('Shape'); Attr 'Id' 0; Attr 'Type' 'Rectangle'; Attr 'LineWidth' '0.254'; Attr 'Locked' 'N'; Attr 'Group' '-1'
  $w.WriteStartElement('Points')
  $w.WriteStartElement('Point'); Attr 'X' (N (-$hx)); Attr 'Y' (N $top); $w.WriteEndElement()
  $w.WriteStartElement('Point'); Attr 'X' (N $hx); Attr 'Y' (N (-$top)); $w.WriteEndElement()
  $w.WriteEndElement(); $w.WriteEndElement()
  Text 1 0 (RowY 0) $pd.Title 6
  Text 2 (-$hx + $pitch) (RowY 0) $pd.Left[0] 6
  Text 3 (-$hx + $pitch) (RowY ($n + 1)) $pd.Left[1] 6
  Text 4 ($hx - $pitch) (RowY 0) $pd.Right[0] 6
  Text 5 ($hx - $pitch) (RowY ($n + 1)) $pd.Right[1] 6
  $w.WriteEndElement()

  $w.WriteEndElement()   # Part
  $stats += ('{0} {1} pins, body {2} x {3}' -f $pd.Name, $pinId, (N (2 * $hx)), (N (2 * $top)))
}
$w.WriteEndElement()   # Component
$w.WriteEndElement()   # Components
$w.WriteEndElement()   # Library
$w.WriteEndDocument(); $w.Flush(); $w.Close()
$xmlText = [System.Text.Encoding]::UTF8.GetString($buf.ToArray()).Replace('<?xml version="1.0" encoding="utf-8"?>', '<?xml version="1.0" encoding="UTF-8"?>')
[System.IO.File]::WriteAllText($Out, $xmlText + "`n", (New-Object System.Text.UTF8Encoding($false)))
'Wrote {0}: {1}; footprint {2} attached as PatType0' -f $Out, ($stats -join '; '), $pat.Name
