<#
.SYNOPSIS
    Checks that the WinCUPL editor can open each .PLD file.

.DESCRIPTION
    When WinCUPL 5.0 opens a file it builds a symbol list with its own small parser, separate
    from the CUPL compiler. If that parser hits a VB runtime error, WinCUPL shows
    "The file ... doesn't exist. New File assumed" and an empty editor, even for a file that
    cupl.exe compiles cleanly. This script runs the same parser, ported from a disassembly of
    Wincupl.exe (loader at 0x4B8B0E, AddSymbols 0x4B9ABD, AddOne 0x4BA6B3), and names the
    line that would stop it.

    What stops the parser:
      - Line endings other than CRLF.
      - A "field" (any case, also inside a name such as field_CPU_ADDRESS) whose rest of the
        line holds both ";" and "=" in any order other than: a space, then "=", then ";".
        Example:  ONB = field_CPU_ADDRESS:['h'0XXXXXX];   // A27-A24 = 0
        Fix: keep "=" out of comments on lines that use a FIELD.

    The parser does not skip // or /* */ comments, so every "pin", "node" and "field" in the
    file is read as the start of a declaration. Most stray matches are harmless. One that
    puts "[...]" between "=" and ";" makes WinCUPL show a "Possible syntax error in symbol
    declaration" box; this script reports that as a warning.

    Exit code 1 if any file fails, otherwise 0.

.PARAMETER Path
    Files or folders to check. Folders are searched recursively for *.PLD.
    Default: the folder holding this script.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File plds\check-wincupl-open.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File plds\check-wincupl-open.ps1 plds\DEVMAPV2\DEVMAP2.PLD
#>
param(
    [string[]] $Path = @($PSScriptRoot)
)

# VB runtime error 5 (Invalid procedure call or argument), the only error the parser can raise.
class VbError : System.Exception {
    VbError([string] $Message) : base($Message) {}
}

# ---- VB6 string functions, with the runtime's argument checks ----

function VbInStr([int] $Start, [string] $Text, [string] $Find, [int] $Compare) {
    if ($Start -lt 1) { throw [VbError]::new("InStr start $Start") }
    if ($Start -gt $Text.Length) { return 0 }
    if ($Find.Length -eq 0) { return $Start }
    $mode = if ($Compare -eq 1) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    return $Text.IndexOf($Find, $Start - 1, $mode) + 1
}

function VbMid([string] $Text, [int] $Start, $Length = $null) {
    if ($Start -lt 1) { throw [VbError]::new("Mid start $Start") }
    if ($null -ne $Length -and $Length -lt 0) { throw [VbError]::new("Mid length $Length") }
    if ($Start -gt $Text.Length) { return '' }
    $left = $Text.Length - $Start + 1
    if ($null -eq $Length -or $Length -gt $left) { $Length = $left }
    return $Text.Substring($Start - 1, $Length)
}

function VbLeft([string] $Text, [int] $Count) {
    if ($Count -lt 0) { throw [VbError]::new("Left length $Count") }
    return $Text.Substring(0, [math]::Min($Count, $Text.Length))
}

function VbRight([string] $Text, [int] $Count) {
    if ($Count -lt 0) { throw [VbError]::new("Right length $Count") }
    $Count = [math]::Min($Count, $Text.Length)
    return $Text.Substring($Text.Length - $Count)
}

function VbTrim([string] $Text) { return $Text.Trim(' ') }   # VB's Trim removes spaces only

function VbAsc([string] $Text) {
    if ($Text.Length -eq 0) { throw [VbError]::new('Asc of an empty string') }
    return [int][char]$Text[0]
}

function IsDigit([string] $Char) { $a = VbAsc $Char; return ($a -gt 0x2F -and $a -lt 0x3A) }   # Wincupl 0x4D9F34

function StripBlanks([string] $Text) {                                                        # Wincupl 0x4D9DC8
    $out = ''
    for ($i = 1; $i -le $Text.Length; $i++) {
        $c = VbMid $Text $i 1; $a = VbAsc $c
        if ($a -ge 0x21 -and $a -le 0x7E) { $out += $c }
    }
    return $out
}

# ---- The editor's symbol loader ----

function Add-One([string] $Name) {
    if ((VbLeft $Name 1) -eq '!') { $Name = VbMid $Name 2 }
    if ((VbInStr 1 $Name '.' 0) -ne 0) { $Name = VbLeft $Name ((VbInStr 1 $Name '.' 0) - 1) }
    # WinCUPL inserts $Name into its sorted symbol list here; that step cannot fail.
}

function Add-Symbols([string] $Decl) {
    $script:stage = 'expanding the symbol list "' + $Decl + '"'
    $ok = $false
    if ((VbLeft $Decl 1) -ne '[') { Add-One $Decl; $ok = $true }
    if ((VbLeft $Decl 1) -eq '[' -and (VbRight $Decl 1) -eq ']') {
        $inner = StripBlanks (VbTrim (VbMid $Decl 2 ($Decl.Length - 2)))
        if ((VbInStr 1 $inner '..' 0) -ne 0) {
            $base = VbMid $inner 1 ((VbInStr 1 $inner '..' 1) - 1)
            $from = '1'; $to = '1'; $k = 1
            for ($i = $base.Length; $i -ge 0; $i--) {
                if (-not (IsDigit (VbMid $base $i 1))) { break }
                $from = VbMid $base $i $k; $k++
            }
            $k = 1
            for ($i = $inner.Length; $i -ge 0; $i--) {
                if (-not (IsDigit (VbMid $inner $i 1))) { break }
                $to = VbMid $inner $i $k; $k++
            }
            if (IsDigit (VbMid $base ($base.Length - 1) 1)) { $base = VbMid $base 1 ($base.Length - 2) }
            else { $base = VbMid $base 1 ($base.Length - 1) }
            # WinCUPL compares the two numbers as strings and swaps them, so ascending ranges
            # such as [T1..3] add nothing to its list. Harmless; kept to stay faithful.
            if ([string]::CompareOrdinal($to, $from) -gt 0) { $t = $to; $to = $from; $from = $t }
            for ($i = [int]$from; $i -le [int]$to; $i++) { Add-One ($base + $i) }
            $ok = $true
        }
        if ((VbInStr 1 $Decl ',' 0) -ne 0) {
            while ($inner.Length -ne 0) {
                if ((VbInStr 1 $inner ',' 0) -ne 0) {
                    Add-One (VbLeft $inner ((VbInStr 1 $inner ',' 0) - 1)); $ok = $true
                    $inner = VbMid $inner ((VbInStr 1 $inner ',' 0) + 1)
                } else { Add-One $inner; break }
            }
        }
    }
    if (-not $ok) { $script:warnings.Add([pscustomobject]@{ Pos = $script:pos; Text = $Decl }) }
}

function Invoke-SymbolLoader([string] $Text) {
    $crlf = "`r`n"
    $commentEnd = 0
    foreach ($keyword in 'Pin', 'Node', 'Field') {
        $hit = VbInStr 1 $Text $keyword 1
        $comment = VbInStr 1 $Text '/*' 1
        if ($comment -ne 0) { $commentEnd = VbInStr $comment $Text '*\' 1 }   # sic: "*\", so comments are never skipped
        while ($hit -ne 0) {
            $script:pos = $hit
            if ($hit -lt $comment -or $hit -gt $commentEnd) {
                $script:stage = 'finding the CRLF that ends this line (WinCUPL needs CRLF line endings)'
                $line = VbMid $Text $hit ((VbInStr $hit $Text $crlf 0) - $hit)
                if ([string]::Compare((VbLeft $line $keyword.Length), $keyword, $true) -eq 0) {
                    if ($keyword -ne 'Field') {
                        if ((VbInStr 1 $line '=' 0) -ne 0) {
                            $line = VbTrim (VbMid $line ((VbInStr 1 $line '=' 0) + 1))
                            if ((VbInStr 1 $line ';' 0) -ne 0) { Add-Symbols (VbTrim (VbLeft $line ((VbInStr 1 $line ';' 0) - 1))) }
                        }
                    } elseif ((VbInStr 1 $line ';' 0) -ne 0 -and (VbInStr 1 $line '=' 0) -ne 0) {
                        $script:stage = '"field" here has no space after it on this line'
                        $line = VbTrim (VbMid $line (VbInStr 1 $line ' ' 0))
                        $script:stage = '"field" here: its "=" comes before the first space after it'
                        Add-Symbols (VbTrim (VbLeft $line ((VbInStr 1 $line '=' 0) - 1)))
                        $line = VbTrim (VbMid $line ((VbInStr 1 $line '=' 0) + 1))
                        $script:stage = '"field" here: ";" comes before "=" (often an "=" in a // comment)'
                        Add-Symbols (VbTrim (VbLeft $line ((VbInStr 1 $line ';' 0) - 1)))
                    }
                }
            } else {
                $comment = VbInStr $comment $Text '/*' 0
                if ($comment -ne 0) { $commentEnd = VbInStr $comment $Text '*\' 0 }
            }
            $hit = VbInStr ($hit + 3) $Text $keyword 1
        }
    }
}

# ---- Main ----

function LineAt([string] $Text, [int] $Pos) {
    return ([regex]::Matches($Text.Substring(0, [math]::Max(0, $Pos - 1)), "`n")).Count + 1
}

$files = foreach ($p in $Path) {
    if (Test-Path -LiteralPath $p -PathType Container) {
        Get-ChildItem -LiteralPath $p -Recurse -Filter *.PLD | Sort-Object FullName | Select-Object -ExpandProperty FullName
    } else { (Resolve-Path -LiteralPath $p).Path }
}

$failed = 0
foreach ($file in $files) {
    $name = Resolve-Path -LiteralPath $file -Relative
    $text = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::GetEncoding(1252))
    $lines = $text -split "`n"
    $script:warnings = New-Object System.Collections.Generic.List[object]
    $script:pos = 0; $script:stage = ''

    $bare = [regex]::Match($text, "(?<!`r)`n")
    if ($bare.Success) {
        'FAIL  {0}  line {1} ends in LF only; WinCUPL needs CRLF line endings' -f $name, (LineAt $text ($bare.Index + 1))
        $failed++; continue
    }
    try {
        Invoke-SymbolLoader $text
        if ($script:warnings.Count -eq 0) { 'OK    {0}' -f $name }
        foreach ($w in $script:warnings) {
            $n = LineAt $text $w.Pos
            'WARN  {0}  line {1}: WinCUPL will show "Possible syntax error in symbol declaration: {2}"' -f $name, $n, $w.Text
            '        ' + $lines[$n - 1].TrimEnd("`r")
        }
    } catch [VbError] {
        $n = LineAt $text $script:pos
        'FAIL  {0}  line {1}: {2}' -f $name, $n, $script:stage
        '        ' + $lines[$n - 1].TrimEnd("`r")
        $failed++
    }
}
if ($failed) { exit 1 }
