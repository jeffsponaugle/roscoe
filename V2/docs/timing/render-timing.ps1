<#
.SYNOPSIS
    Renders WaveDrom (WaveJSON) timing diagrams to SVG with no external tools.

.DESCRIPTION
    Reads every *.json file in -In and writes a matching .svg into -Out.
    The sources are plain WaveJSON, so they can also be pasted into
    https://wavedrom.com/editor.html or the VS Code WaveDrom extension.

    Supported WaveJSON subset:
      signal rows   { "name", "wave", "data", "node", "phase", "period" }
      groups        [ "label", row, row, ... ]   (nesting allowed)
      spacers       {}
      head / foot   { "text" }
      config        { "hscale" }                 plus "grid": false to hide clock guides
      edge          [ "a->b label", "a~>b", "a-|>b", "a|->b", "a<->b", "a-b" ]
    Wave characters: p n P N h l H L 0 1 x z = 2-9 u d . |
      u / d draw a slow (resistor pull-up / pull-down) transition.

    Roscoe extension (WaveDrom ignores it):
      markers       [ { "at": 6, "label": "DSACK sampled" } ]
                    labelled vertical guide lines; "at" is in wave slots from the left edge.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File docs\timing\render-timing.ps1
#>
param(
    [string] $In  = (Join-Path $PSScriptRoot 'src'),
    [string] $Out = $PSScriptRoot
)
$ErrorActionPreference = 'Stop'
$Inv = [System.Globalization.CultureInfo]::InvariantCulture

# Layout constants, in pixels.
$SlotW = 20; $RowH = 30; $Amp = 16; $Slant = 3
$NameFont = 12; $DataFont = 11; $NoteFont = 10
$Ink = '#222222'; $GridMajor = '#d5d5d5'; $GridMinor = '#ececec'; $ZColor = '#2a5db0'; $MarkColor = '#c0392b'
$Fills = @{ '=' = '#ffffff'; '2' = '#ffffff'; '3' = '#fff4b0'; '4' = '#ffd9b0'; '5' = '#cfe6ff';
            '6' = '#d6f0cf'; '7' = '#ead6ff'; '8' = '#ffd3dc'; '9' = '#e4e4e4' }

function N([double] $v) { $v.ToString('0.##', $Inv) }
function Esc([string] $s) { [System.Security.SecurityElement]::Escape($s) }

function Get-Rows($signal) {
    $script:rows = New-Object System.Collections.ArrayList
    $script:groups = New-Object System.Collections.ArrayList
    Walk-Signal $signal 0
}

function Walk-Signal($items, [int] $depth) {
    foreach ($it in $items) {
        if ($it -is [array]) {
            $first = $script:rows.Count
            Walk-Signal ($it | Select-Object -Skip 1) ($depth + 1)
            [void]$script:groups.Add([pscustomobject]@{ Label = [string]$it[0]; Depth = $depth; First = $first; Last = $script:rows.Count - 1 })
        } elseif ($null -eq $it.wave) {
            [void]$script:rows.Add([pscustomobject]@{ Spacer = $true; Depth = $depth; Sig = $null })
        } else {
            [void]$script:rows.Add([pscustomobject]@{ Spacer = $false; Depth = $depth; Sig = $it })
        }
    }
}

# Splits a wave string into merged segments measured in slots.
function Get-Segments([string] $wave, [double] $period) {
    $segs = New-Object System.Collections.ArrayList
    $gaps = New-Object System.Collections.ArrayList
    $cur = $null; $dataIdx = 0
    for ($i = 0; $i -lt $wave.Length; $i++) {
        $c = [string]$wave[$i]; $x0 = $i * $period
        if ($c -eq '.' -or $c -eq '|') {
            if ($c -eq '|') { [void]$gaps.Add($x0 + $period / 2) }
            if ($cur) { $cur.X1 = $x0 + $period }
            continue
        }
        $kind = $null; $val = $c
        switch -CaseSensitive ($c) {
            { $_ -in 'p','n','P','N' } { $kind = 'clock' }
            { $_ -in '1','h','H' }     { $kind = 'level'; $val = 'H' }
            { $_ -in '0','l','L' }     { $kind = 'level'; $val = 'L' }
            'u'                        { $kind = 'pull'; $val = 'H' }
            'd'                        { $kind = 'pull'; $val = 'L' }
            'z'                        { $kind = 'z' }
            'x'                        { $kind = 'x' }
            { $_ -in '=','2','3','4','5','6','7','8','9' } { $kind = 'data' }
        }
        if (-not $kind) { throw "Unsupported wave character '$c' in '$wave'" }
        $cur = [pscustomobject]@{ Kind = $kind; Val = $val; X0 = $x0; X1 = $x0 + $period; Data = -1; Sharp = ($c -cmatch '[hlHLpnPN]') }
        if ($kind -eq 'data') { $cur.Data = $dataIdx; $dataIdx++ }
        [void]$segs.Add($cur)
    }
    return @{ Segs = $segs; Gaps = $gaps }
}

function Render-Diagram($json, [string] $title) {
    $hscale = 1.0
    if ($json.config -and $json.config.hscale) { $hscale = [double]$json.config.hscale }
    $showGrid = -not ($json.config -and ($json.config.PSObject.Properties.Name -contains 'grid') -and -not $json.config.grid)
    $sw = $SlotW * $hscale

    Get-Rows $json.signal
    $rows = $script:rows; $groups = $script:groups

    # Width of the name column and the wave area.
    $maxDepth = 0; $nameChars = 4; $slots = 1
    foreach ($r in $rows) {
        if ($r.Depth -gt $maxDepth) { $maxDepth = $r.Depth }
        if ($r.Spacer) { continue }
        $nm = [string]$r.Sig.name
        if ($nm.Length -gt $nameChars) { $nameChars = $nm.Length }
        $per = 1.0; if ($r.Sig.period) { $per = [double]$r.Sig.period }
        $len = ([string]$r.Sig.wave).Length * $per
        if ($len -gt $slots) { $slots = $len }
    }
    foreach ($r in $rows) {
        if ($r.Spacer) { continue }
        $per = 1.0; if ($r.Sig.period) { $per = [double]$r.Sig.period }
        $len = ([string]$r.Sig.wave).Length * $per
        if ($len -ne $slots) { Write-Warning ("{0}: '{1}' spans {2} slots, diagram is {3}" -f $title, $r.Sig.name, $len, $slots) }
    }
    $groupW = $maxDepth * 16
    $x0 = [math]::Ceiling($groupW + $nameChars * 7.4 + 18)
    $waveW = $slots * $sw
    $width = [math]::Ceiling($x0 + $waveW + 14)

    $markers = @(); if ($json.markers) { $markers = @($json.markers) }
    $headText = $null; if ($json.head -and $json.head.text) { $headText = [string]$json.head.text }
    $footText = $null; if ($json.foot -and $json.foot.text) { $footText = [string]$json.foot.text }

    $y = 8
    $titleY = $null; if ($headText) { $titleY = $y + 14; $y += 24 }
    $markY = $y; if ($markers.Count -gt 0) { $y += 30 }
    $rowsY = $y
    $rowsBottom = $rowsY + $rows.Count * $RowH
    $height = $rowsBottom + 10
    # Footnote: honour explicit line breaks, then wrap to the diagram width (about 5.9 px per character).
    $footLines = @()
    if ($footText) {
        $maxChars = [math]::Max(40, [math]::Floor(($width - 16) / 5.9))
        foreach ($para in ($footText -split "`n")) {
            $line = ''
            foreach ($word in ($para -split ' ')) {
                if ($line.Length -gt 0 -and ($line.Length + 1 + $word.Length) -gt $maxChars) { $footLines += $line; $line = $word }
                elseif ($line.Length -gt 0) { $line += ' ' + $word } else { $line = $word }
            }
            $footLines += $line
        }
    }
    $footY = $null; if ($footLines.Count -gt 0) { $footY = $rowsBottom + 16; $height = $footY + 14 * $footLines.Count }

    $sb = New-Object System.Text.StringBuilder
    function W([string] $s) { [void]$sb.AppendLine($s) }

    W ('<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" viewBox="0 0 {0} {1}" font-family="Helvetica, Arial, sans-serif">' -f $width, (N $height))
    if ($title) { W ("<title>{0}</title>" -f (Esc $title)) }
    W '<defs>'
    W '<pattern id="hatch" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="5" height="5" fill="#f2f2f2"/><line x1="0" y1="0" x2="0" y2="5" stroke="#b0b0b0" stroke-width="1.4"/></pattern>'
    W ('<clipPath id="wave"><rect x="{0}" y="0" width="{1}" height="{2}"/></clipPath>' -f $x0, (N $waveW), (N $height))
    W '</defs>'
    W ('<rect width="{0}" height="{1}" fill="#ffffff"/>' -f $width, (N $height))
    if ($titleY) { W ('<text x="8" y="{0}" font-size="14" font-weight="bold" fill="{1}">{2}</text>' -f $titleY, $Ink, (Esc $headText)) }

    # Clock guide lines from the first clock row (rising edges darker).
    if ($showGrid) {
        foreach ($r in $rows) {
            if ($r.Spacer) { continue }
            $w = [string]$r.Sig.wave
            if ($w.Length -gt 0 -and 'pnPN'.Contains($w.Substring(0,1))) {
                $per = 1.0; if ($r.Sig.period) { $per = [double]$r.Sig.period }
                $ph = 0.0; if ($r.Sig.phase) { $ph = [double]$r.Sig.phase }
                $k = 0
                for ($t = -$ph * $per; $t -le $slots + 0.001; $t += $per / 2) {
                    if ($t -ge -0.001) {
                        $gx = $x0 + $t * $sw
                        $col = $GridMinor; if ($k % 2 -eq 0) { $col = $GridMajor }
                        W ('<line x1="{0}" y1="{1}" x2="{0}" y2="{2}" stroke="{3}" stroke-width="1"/>' -f (N $gx), $rowsY, $rowsBottom, $col)
                    }
                    $k++
                }
                break
            }
        }
    }

    # Group brackets.
    foreach ($g in $groups) {
        if ($g.Last -lt $g.First) { continue }
        $gx = 6 + $g.Depth * 16
        $gy0 = $rowsY + $g.First * $RowH + 4; $gy1 = $rowsY + ($g.Last + 1) * $RowH - 4
        W ('<path d="M{0},{1} h-4 V{2} h4" fill="none" stroke="#777" stroke-width="1"/>' -f (N ($gx + 8)), $gy0, $gy1)
        $cy = ($gy0 + $gy1) / 2
        W ('<text x="{0}" y="{1}" font-size="{2}" fill="#555" text-anchor="middle" transform="rotate(-90 {0} {1})">{3}</text>' -f (N ($gx + 1)), (N $cy), $NoteFont, (Esc $g.Label))
    }

    $nodes = @{}
    W '<g clip-path="url(#wave)">'
    $names = New-Object System.Text.StringBuilder
    for ($ri = 0; $ri -lt $rows.Count; $ri++) {
        $r = $rows[$ri]
        if ($r.Spacer) { continue }
        $sig = $r.Sig
        $top = $rowsY + $ri * $RowH
        $yH = $top + ($RowH - $Amp) / 2; $yL = $yH + $Amp; $yM = ($yH + $yL) / 2
        [void]$names.AppendLine(('<text x="{0}" y="{1}" font-size="{2}" font-family="Consolas, Menlo, monospace" fill="{3}" text-anchor="end">{4}</text>' -f ($x0 - 8), (N ($yM + 4)), $NameFont, $Ink, (Esc ([string]$sig.name))))

        $per = 1.0; if ($sig.period) { $per = [double]$sig.period }
        $ph = 0.0; if ($sig.phase) { $ph = [double]$sig.phase }
        $data = @()
        if ($sig.data) { if ($sig.data -is [string]) { $data = $sig.data -split '\s+' } else { $data = @($sig.data) } }
        $parsed = Get-Segments ([string]$sig.wave) $per
        $segs = $parsed.Segs
        $X = { param($s) $x0 + ($s - $ph * $per) * $sw }

        $prevEnd = $null
        for ($si = 0; $si -lt $segs.Count; $si++) {
            $s = $segs[$si]
            $next = $null; if ($si + 1 -lt $segs.Count) { $next = $segs[$si + 1] }
            $xa = & $X $s.X0; $xb = & $X $s.X1
            switch ($s.Kind) {
                'clock' {
                    $hiFirst = ($s.Val -ceq 'p' -or $s.Val -ceq 'P')
                    $d = ''; $cyc = [int][math]::Round(($s.X1 - $s.X0) / $per)
                    $cw = $per * $sw
                    for ($k = 0; $k -lt $cyc; $k++) {
                        $cx = $xa + $k * $cw
                        if ($hiFirst) { $d += ('M{0},{1} V{2} H{3} V{1} H{4} ' -f (N $cx), (N $yL), (N $yH), (N ($cx + $cw / 2)), (N ($cx + $cw))) }
                        else          { $d += ('M{0},{1} V{2} H{3} V{1} H{4} ' -f (N $cx), (N $yH), (N $yL), (N ($cx + $cw / 2)), (N ($cx + $cw))) }
                        if ($s.Val -ceq 'P') { W ('<path d="M{0},{1} l-3,5 h6 z" fill="{2}"/>' -f (N $cx), (N ($yM - 3)), $Ink) }
                        if ($s.Val -ceq 'N') { W ('<path d="M{0},{1} l-3,-5 h6 z" fill="{2}"/>' -f (N $cx), (N ($yM + 3)), $Ink) }
                    }
                    W ('<path d="{0}" fill="none" stroke="{1}" stroke-width="1.5"/>' -f $d.Trim(), $Ink)
                    if ($hiFirst) { $prevEnd = $yL } else { $prevEnd = $yH }
                }
                { $_ -in 'level','pull','z' } {
                    $yv = $yM; if ($s.Val -eq 'H') { $yv = $yH } elseif ($s.Val -eq 'L') { $yv = $yL }
                    if ($s.Kind -eq 'z') { $yv = $yM }
                    $col = $Ink; if ($s.Kind -eq 'z') { $col = $ZColor }
                    $sl = $Slant; if ($s.Sharp) { $sl = 0 }
                    if ($s.Kind -eq 'pull' -and $null -ne $prevEnd -and $prevEnd -ne $yv) {
                        # RC charge curve across the whole segment (about 3 time constants).
                        $d = 'M{0},{1}' -f (N $xa), (N $prevEnd)
                        for ($k = 1; $k -le 16; $k++) {
                            $f = 1 - [math]::Exp(-3.0 * $k / 16)
                            $d += ' L{0},{1}' -f (N ($xa + ($xb - $xa) * $k / 16)), (N ($prevEnd + ($yv - $prevEnd) * $f))
                        }
                    } elseif ($null -ne $prevEnd -and $prevEnd -ne $yv) {
                        $d = 'M{0},{1} L{2},{3} H{4}' -f (N $xa), (N $prevEnd), (N ($xa + $sl)), (N $yv), (N $xb)
                    } else {
                        $d = 'M{0},{1} H{2}' -f (N $xa), (N $yv), (N $xb)
                    }
                    W ('<path d="{0}" fill="none" stroke="{1}" stroke-width="1.5"/>' -f $d, $col)
                    $prevEnd = $yv
                }
                { $_ -in 'data','x' } {
                    # Note: PowerShell names are case-insensitive, so the apex points must not be called $yl / $yr.
                    $apexL = $yM; if ($null -ne $prevEnd) { $apexL = $prevEnd }
                    $apexR = $yM
                    if ($next -and ($next.Kind -eq 'level' -or $next.Kind -eq 'pull')) { if ($next.Val -eq 'H') { $apexR = $yH } else { $apexR = $yL } }
                    if ($null -eq $next) { $apexR = $null }
                    $tl = $xa + $Slant; if ($apexL -eq $yH -or $si -eq 0) { $tl = $xa }
                    $bl = $xa + $Slant; if ($apexL -eq $yL -or $si -eq 0) { $bl = $xa }
                    if ($si -eq 0) { $apexL = $null }
                    if ($null -eq $apexR) { $tr = $xb; $br = $xb } else {
                        $tr = $xb - $Slant; if ($apexR -eq $yH) { $tr = $xb }
                        $br = $xb - $Slant; if ($apexR -eq $yL) { $br = $xb }
                    }
                    $pts = @()
                    if ($null -ne $apexL) { $pts += ('{0},{1}' -f (N $xa), (N $apexL)) }
                    $pts += ('{0},{1}' -f (N $tl), (N $yH)); $pts += ('{0},{1}' -f (N $tr), (N $yH))
                    if ($null -ne $apexR) { $pts += ('{0},{1}' -f (N $xb), (N $apexR)) }
                    $pts += ('{0},{1}' -f (N $br), (N $yL)); $pts += ('{0},{1}' -f (N $bl), (N $yL))
                    $fill = 'url(#hatch)'; if ($s.Kind -eq 'data') { $fill = $Fills[$s.Val] }
                    W ('<polygon points="{0}" fill="{1}" stroke="{2}" stroke-width="1.2"/>' -f ($pts -join ' '), $fill, $Ink)
                    if ($s.Kind -eq 'data' -and $s.Data -lt $data.Count) {
                        $label = [string]$data[$s.Data]
                        $avail = ($xb - $xa) - 2 * $Slant
                        $fs = $DataFont
                        while ($fs -gt 7 -and ($label.Length * $fs * 0.56) -gt $avail) { $fs-- }
                        if (($label.Length * $fs * 0.56) -le $avail + 6) {
                            W ('<text x="{0}" y="{1}" font-size="{2}" fill="{3}" text-anchor="middle">{4}</text>' -f (N (($xa + $xb) / 2)), (N ($yM + $fs * 0.36)), $fs, $Ink, (Esc $label))
                        }
                    }
                    $prevEnd = $yM; if ($null -ne $apexR) { $prevEnd = $apexR }
                }
            }
        }
        foreach ($gp in $parsed.Gaps) {
            $gx = & $X $gp
            W ('<path d="M{0},{1} l4,{2} M{3},{1} l4,{2}" stroke="#ffffff" stroke-width="4"/>' -f (N ($gx - 4)), (N ($yH - 3)), (N ($Amp + 6)), (N ($gx + 1)))
            W ('<path d="M{0},{1} l4,{2} M{3},{1} l4,{2}" stroke="{4}" stroke-width="1"/>' -f (N ($gx - 6)), (N ($yH - 3)), (N ($Amp + 6)), (N ($gx + 3)), $Ink)
        }
        if ($sig.node) {
            $ns = [string]$sig.node
            for ($k = 0; $k -lt $ns.Length; $k++) {
                $ch = [string]$ns[$k]
                if ($ch -ne '.') { $nodes[$ch] = @{ X = (& $X ($k * $per)) + $Slant / 2; Y = $yM } }
            }
        }
    }
    W '</g>'
    W $names.ToString()

    # Vertical markers with staggered labels.
    $mi = 0
    foreach ($m in $markers) {
        $mx = $x0 + [double]$m.at * $sw
        $ly = $markY + 11 + ($mi % 2) * 13
        W ('<line x1="{0}" y1="{1}" x2="{0}" y2="{2}" stroke="{3}" stroke-width="1" stroke-dasharray="4,3"/>' -f (N $mx), ($ly + 3), $rowsBottom, $MarkColor)
        if ($m.label) { W ('<text x="{0}" y="{1}" font-size="{2}" fill="{3}" text-anchor="middle">{4}</text>' -f (N $mx), $ly, $NoteFont, $MarkColor, (Esc ([string]$m.label))) }
        $mi++
    }

    # Edges between nodes.
    foreach ($e in @($json.edge)) {
        if (-not $e) { continue }
        if ($e -notmatch '^\s*(\w)([-~|<>]+)(\w)\s*(.*)$') { continue }
        $a = $nodes[$matches[1]]; $b = $nodes[$matches[3]]; $spec = $matches[2]; $label = $matches[4]
        if (-not $a -or -not $b) { continue }
        if ($spec -match '~') {
            $dx = ($b.X - $a.X) / 2
            $d = 'M{0},{1} C{2},{1} {3},{4} {5},{4}' -f (N $a.X), (N $a.Y), (N ($a.X + $dx)), (N ($b.X - $dx)), (N $b.Y), (N $b.X)
            $endDir = @(1, 0); if ($b.X -lt $a.X) { $endDir = @(-1, 0) }
            $startDir = @(-$endDir[0], 0)
            $lx = ($a.X + $b.X) / 2; $lyy = ($a.Y + $b.Y) / 2
        } elseif ($spec -match '-\|') {
            $d = 'M{0},{1} H{2} V{3}' -f (N $a.X), (N $a.Y), (N $b.X), (N $b.Y)
            $endDir = @(0, [math]::Sign($b.Y - $a.Y)); $startDir = @(-[math]::Sign($b.X - $a.X), 0)
            $lx = ($a.X + $b.X) / 2; $lyy = $a.Y
        } elseif ($spec -match '\|-') {
            $d = 'M{0},{1} V{2} H{3}' -f (N $a.X), (N $a.Y), (N $b.Y), (N $b.X)
            $endDir = @([math]::Sign($b.X - $a.X), 0); $startDir = @(0, -[math]::Sign($b.Y - $a.Y))
            $lx = ($a.X + $b.X) / 2; $lyy = $b.Y
        } else {
            $d = 'M{0},{1} L{2},{3}' -f (N $a.X), (N $a.Y), (N $b.X), (N $b.Y)
            $len = [math]::Sqrt([math]::Pow($b.X - $a.X, 2) + [math]::Pow($b.Y - $a.Y, 2)); if ($len -eq 0) { $len = 1 }
            $endDir = @((($b.X - $a.X) / $len), (($b.Y - $a.Y) / $len)); $startDir = @(-$endDir[0], -$endDir[1])
            $lx = ($a.X + $b.X) / 2; $lyy = ($a.Y + $b.Y) / 2
        }
        W ('<path d="{0}" fill="none" stroke="#1f6fb2" stroke-width="1.2"/>' -f $d)
        foreach ($end in @(@{ On = ($spec -match '>$'); P = $b; D = $endDir }, @{ On = ($spec -match '^<'); P = $a; D = $startDir })) {
            if (-not $end.On) { continue }
            $ux = $end.D[0]; $uy = $end.D[1]
            $px = $end.P.X; $py = $end.P.Y
            $p1 = '{0},{1}' -f (N ($px - 7 * $ux + 3 * $uy)), (N ($py - 7 * $uy - 3 * $ux))
            $p2 = '{0},{1}' -f (N ($px - 7 * $ux - 3 * $uy)), (N ($py - 7 * $uy + 3 * $ux))
            W ('<polygon points="{0},{1} {2} {3}" fill="#1f6fb2"/>' -f (N $px), (N $py), $p1, $p2)
        }
        if ($label) {
            $tw = $label.Length * $NoteFont * 0.55 + 6
            W ('<rect x="{0}" y="{1}" width="{2}" height="13" fill="#ffffff" fill-opacity="0.85"/>' -f (N ($lx - $tw / 2)), (N ($lyy - 10)), (N $tw))
            W ('<text x="{0}" y="{1}" font-size="{2}" fill="#1f6fb2" text-anchor="middle">{3}</text>' -f (N $lx), (N ($lyy + 0)), $NoteFont, (Esc $label))
        }
    }

    for ($k = 0; $k -lt $footLines.Count; $k++) {
        W ('<text x="8" y="{0}" font-size="11" fill="#555">{1}</text>' -f ($footY + 14 * $k), (Esc $footLines[$k]))
    }
    W '</svg>'
    return $sb.ToString()
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
foreach ($f in Get-ChildItem -Path $In -Filter *.json | Sort-Object Name) {
    $json = Get-Content -Raw -Path $f.FullName | ConvertFrom-Json
    $svg = Render-Diagram $json $f.BaseName
    $dest = Join-Path $Out ($f.BaseName + '.svg')
    [System.IO.File]::WriteAllText($dest, $svg, (New-Object System.Text.UTF8Encoding($false)))
    "wrote $dest"
}
