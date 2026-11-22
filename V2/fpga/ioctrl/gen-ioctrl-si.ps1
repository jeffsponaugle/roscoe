<#
.SYNOPSIS
    Writes the CUPL simulator test vectors (.si) for IOCTRL.

.DESCRIPTION
    Every vector carries the expected value of every output, so csim itself reports any output
    that differs. The steps below set the inputs; the expected outputs carry over from step to
    step, and each step gives only the changes. $MSG lines name each test in the simulator's
    output (.so).

    What it covers: power-up with the FPGA unconfigured; the 1024-clock reset stretch and the
    release kick; every device code with and without nDEV_EN; the outputs that end when AS
    negates; the power-off request (codes 12/13); a software reset (code 14); a power
    controller reset; a 68030 RESET instruction; and DONE dropping mid-cycle.

    The header is copied from the .PLD, which csim requires to match.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File fpga\ioctrl\gen-ioctrl-si.ps1
#>
param(
    [string]$Pld = (Join-Path $PSScriptRoot 'IOCTRL_REV1.PLD'),
    [string]$Out = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Out) { $Out = [System.IO.Path]::ChangeExtension($Pld, '.si') }

$inNames  = 'CPU_CLK','CPU_RW','nCPU_AS','FPGA_DONE','nPWR_RESET_SINK','nDEV_EN','DEV_CODE3','DEV_CODE2','DEV_CODE1','DEV_CODE0'
$outNames = 'nIDE1_CSA','nIDE1_CSB','nIDE2_CSA','nIDE2_CSB','nIDE1_BUFFER_EN','nIDE2_BUFFER_EN','nIDE_RD','nIDE_WR',
            'nRTC_CS','RTC_A_SEL','RTC_D_SEL','nUART_CSA','nUART_CSB',
            'nSEVENSEGA_WR','nSEVENSEGB_WR','nSTATUSLED_WR',
            'nCPU_RD','SYS_RESET_POS','POWER_CONTROL_OUT','SPARE22','SPARE31',
            'KICK','SOFT','PWRDN'     # internal nodes, shown so the reset and power logic can be checked

# Current inputs.
$script:in = @{ rw = 1; as = 1; done = 0; sink = 1; en = 1; code = 0 }
# Current expected outputs: H, L, Z, or * (not checked).
$script:ex = @{}
foreach ($n in $outNames) { $script:ex[$n] = 'H' }
$script:ex['RTC_A_SEL'] = 'L'; $script:ex['POWER_CONTROL_OUT'] = 'Z'; $script:ex['SPARE22'] = 'L'; $script:ex['SPARE31'] = 'L'
$script:ex['KICK'] = 'L'; $script:ex['SOFT'] = 'L'
# nSYS_RESET column: H or L = IOCTRL drives the line to this level; 0 or 1 = IOCTRL must leave it
# Hi-Z, and the board (R47, or the CPU) holds it at this level.
$script:rst = 'L'
$script:lines = New-Object System.Collections.Generic.List[string]
$script:count = 0

function Set-In([hashtable]$h) { foreach ($k in $h.Keys) { $script:in[$k] = $h[$k] } }
function Set-Ex([hashtable]$h) { foreach ($k in $h.Keys) { $script:ex[$k] = $h[$k] } }
function Note([string]$t) { $script:lines.Add('$MSG "' + ('{0:D4} {1}' -f ($script:count + 1), $t) + '";') }
function Emit([string]$clk) {
    $c = $script:in.code
    $bits = '{0}{1}{2}{3}' -f (($c -shr 3) -band 1), (($c -shr 2) -band 1), (($c -shr 1) -band 1), ($c -band 1)
    $s = '{0}{1}{2}{3}{4}{5}{6}' -f $clk, $script:in.rw, $script:in.as, $script:in.done, $script:in.sink, $script:in.en, $bits
    $s += ' ' + $script:rst + ' '
    # The power-off bit is what POWER_CONTROL_OUT shows.
    $script:ex['PWRDN'] = switch ($script:ex['POWER_CONTROL_OUT']) { 'H' { 'H' } 'Z' { 'L' } default { '*' } }
    $o = foreach ($n in $outNames) { $script:ex[$n] }
    $s += ($o[0..7] -join '') + ' ' + ($o[8..12] -join '') + ' ' + ($o[13..15] -join '') + ' ' +
          ($o[16..20] -join '') + ' ' + ($o[21..23] -join '')
    $script:lines.Add($s)
    $script:count++
}
function Step([hashtable]$inp = @{}, [hashtable]$exp = @{}) { Set-In $inp; Set-Ex $exp; Emit '1' }   # no clock edge
function Clk ([hashtable]$inp = @{}, [hashtable]$exp = @{}) { Set-In $inp; Set-Ex $exp; Emit 'K' }   # one falling edge of CPU_CLK
function Clks([int]$n, [hashtable]$exp = @{}) { Set-Ex $exp; for ($i = 0; $i -lt $n; $i++) { Emit 'K' } }

# ---------------- 1. Power-up: FPGA not configured ----------------
Note 'power-up, DONE low: reset driven low, everything off, nCPU_RD held off'
Step
Clks 3
Note 'DONE low: codes and strobes are ignored, even with AS low'
Clk @{ as = 0; code = 1; en = 0 } ; Clk @{ code = 5 } ; Clk @{ code = 7 } ; Clk @{ code = 12 } ; Clk @{ code = 14 }
Clk @{ as = 1; code = 0; en = 1 }

# ---------------- 2. DONE rises: 1024-clock stretch, then release with a kick ----------------
Note 'DONE high: nCPU_RD follows R/W; reset still held'
Step @{ done = 1 } @{ nCPU_RD = 'L' }
Clks 1023
Note 'edge 1024 after DONE: released, IOCTRL drives nSYS_RESET high (kick)'
$script:rst = 'H'; Clk @{} @{ SYS_RESET_POS = 'L'; KICK = 'H' }
Note 'edge 1025: still driven high (second kick clock)'
Clk
Note 'edge 1026: Hi-Z; R47 holds it high'
$script:rst = '1'; Clk @{} @{ KICK = 'L' }
Clks 3

# ---------------- 3. IDE1 read, code 1 ----------------
Note 'IDE1 read: AS low, select on the first edge'
Step @{ as = 0; rw = 1 }
Clk @{ code = 1 } @{ nIDE1_CSA = 'L'; nIDE1_BUFFER_EN = 'L' }
Clk @{}
Note 'strobe window: DIOR low'
Clk @{ en = 0 } @{ nIDE_RD = 'L' }
Clks 3
Note 'strobe ends on the latch edge (nDEV_EN high)'
Clk @{ en = 1 } @{ nIDE_RD = 'H' }
Note 'AS negates: buffer enable off at once, /CS0 stays'
Step @{ as = 1 } @{ nIDE1_BUFFER_EN = 'H' }
Note 'code removed: /CS0 off'
Clk @{ code = 0 } @{ nIDE1_CSA = 'H' }

# ---------------- 4. IDE1 control block, code 2, write ----------------
Note 'IDE1 control write (code 2): /CS1, buffer, DIOW'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 2 } @{ nIDE1_CSB = 'L'; nIDE1_BUFFER_EN = 'L' }
Clk @{ en = 0 } @{ nIDE_WR = 'L' }
Clks 4
Clk @{ en = 1 } @{ nIDE_WR = 'H' }
Clk
Step @{ as = 1 } @{ nIDE1_BUFFER_EN = 'H' }
Clk @{ code = 0 } @{ nIDE1_CSB = 'H' }

# ---------------- 5. IDE2, codes 3 and 4 ----------------
Note 'IDE2 command write (code 3)'
Step @{ as = 0; rw = 0 }
Clk @{ code = 3 } @{ nIDE2_CSA = 'L'; nIDE2_BUFFER_EN = 'L' }
Clk @{ en = 0 } @{ nIDE_WR = 'L' }
Clk @{ en = 1 } @{ nIDE_WR = 'H' }
Step @{ as = 1 } @{ nIDE2_BUFFER_EN = 'H' }
Clk @{ code = 0 } @{ nIDE2_CSA = 'H' }
Note 'IDE2 control read (code 4); AS negates during the strobe and ends it'
Step @{ as = 0; rw = 1 } @{ nCPU_RD = 'L' }
Clk @{ code = 4 } @{ nIDE2_CSB = 'L'; nIDE2_BUFFER_EN = 'L' }
Clk @{ en = 0 } @{ nIDE_RD = 'L' }
Step @{ as = 1 } @{ nIDE_RD = 'H'; nIDE2_BUFFER_EN = 'H' }
Note 'while AS is high, the AS-tied outputs stay off even with the code and strobe present'
Clk
Clk @{ en = 1; code = 0 } @{ nIDE2_CSB = 'H' }

# ---------------- 6. RTC, code 5 ----------------
Note 'RTC read: /CS (its AS), then DS high and U35 on'
Step @{ as = 0; rw = 1 }
Clk @{ code = 5 } @{ nRTC_CS = 'L' }
Clk @{ en = 0 } @{ RTC_A_SEL = 'H'; RTC_D_SEL = 'L' }
Clks 2
Note 'read ended by AS negation: DS low, U35 off; the RTC /CS stays'
Step @{ as = 1 } @{ RTC_A_SEL = 'L'; RTC_D_SEL = 'H' }
Clk
Clk @{ en = 1; code = 0 } @{ nRTC_CS = 'H' }
Note 'RTC write: DS ends by nDEV_EN a clock before the cycle, /CS a clock after DS'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 5 } @{ nRTC_CS = 'L' }
Clk @{ en = 0 } @{ RTC_A_SEL = 'H'; RTC_D_SEL = 'L' }
Clks 2
Clk @{ en = 1 } @{ RTC_A_SEL = 'L'; RTC_D_SEL = 'H' }
Step @{ as = 1 }
Clk @{ code = 0 } @{ nRTC_CS = 'H' }

# ---------------- 7. UART, codes 7 and 8 ----------------
Note 'UART A read: select on, then off the moment AS negates'
Step @{ as = 0; rw = 1 } @{ nCPU_RD = 'L' }
Clk @{ code = 7 } @{ nUART_CSA = 'L' }
Step @{ as = 1 } @{ nUART_CSA = 'H' }
Note 'code still 7 at the next edge with AS high: stays off'
Clk
Note 'next cycle starts (AS low) with the stale code: still off until an edge samples it'
Step @{ as = 0 }
Clk @{ code = 0 }
Step @{ as = 1 }
Note 'UART B write: select ends by code a clock before the cycle'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 8 } @{ nUART_CSB = 'L' }
Clk @{ code = 0 } @{ nUART_CSB = 'H' }
Step @{ as = 1 }

# ---------------- 8. Latches, codes 9-11 ----------------
Note 'latch strobes, each a clock wide'
Step @{ as = 0; rw = 0 }
Clk @{ code = 9 }  @{ nSEVENSEGA_WR = 'L' }
Clk @{ code = 0 }  @{ nSEVENSEGA_WR = 'H' }
Clk @{ code = 10 } @{ nSEVENSEGB_WR = 'L' }
Clk @{ code = 0 }  @{ nSEVENSEGB_WR = 'H' }
Clk @{ code = 11 } @{ nSTATUSLED_WR = 'L' }
Note 'AS negation ends a latch strobe too'
Step @{ as = 1 } @{ nSTATUSLED_WR = 'H' }
Clk @{ code = 0 }

# ---------------- 9. Spare codes 6 and 15 do nothing ----------------
Note 'codes 6 and 15, with and without nDEV_EN: nothing'
Step @{ as = 0; rw = 1 } @{ nCPU_RD = 'L' }
Clk @{ code = 6 } ; Clk @{ en = 0 } ; Clk @{ code = 15 } ; Clk @{ en = 1 } ; Clk @{ code = 0 }
Step @{ as = 1 }

# ---------------- 10. Power-off request, codes 12 and 13 ----------------
Note 'code 12 without nDEV_EN: no change'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 12 }
Note 'code 12 with nDEV_EN: POWER_CONTROL_OUT driven high'
Clk @{ en = 0 } @{ POWER_CONTROL_OUT = 'H' }
Clk @{ en = 1; code = 0 }
Clks 2
Note 'code 13 with nDEV_EN: released (Hi-Z)'
Clk @{ code = 13 }
Clk @{ en = 0 } @{ POWER_CONTROL_OUT = 'Z' }
Clk @{ en = 1; code = 0 }
Note 'set it again for the reset tests'
Clk @{ code = 12; en = 0 } @{ POWER_CONTROL_OUT = 'H' }
Clk @{ code = 0; en = 1 }
Step @{ as = 1 }

# ---------------- 11. Software reset, code 14 ----------------
Note 'code 14 with nDEV_EN: SOFT set on this edge, reset asserted on the next'
Step @{ as = 0; rw = 0 }
Clk @{ code = 14; en = 0 } @{ SOFT = 'H' }
Note 'reset asserted: driven low, UART reset, power-off bit cleared'
# csim applies an asynchronous clear that depends on a pin changed in the same vector one vector
# late, so POWER_CONTROL_OUT is checked from the next vector on.
$script:rst = 'L'; Clk @{ code = 0; en = 1 } @{ SYS_RESET_POS = 'H'; POWER_CONTROL_OUT = '*'; SOFT = 'L' }
Step @{ as = 1; rw = 1 } @{ nCPU_RD = 'L'; POWER_CONTROL_OUT = 'Z' }
Clks 1023
Note 'edge 1024 after the reset edge: released with a kick'
$script:rst = 'H'; Clk @{} @{ SYS_RESET_POS = 'L'; KICK = 'H' }
Clk
$script:rst = '1'; Clk @{} @{ KICK = 'L' }
Clks 2

# ---------------- 12. Power controller reset (asynchronous) ----------------
Note 'set the power-off bit, then nPWR_RESET_SINK low: reset at once, bit cleared'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 12; en = 0 } @{ POWER_CONTROL_OUT = 'H' }
Clk @{ code = 0; en = 1 }
Step @{ as = 1; rw = 1 } @{ nCPU_RD = 'L' }
$script:rst = 'L'; Step @{ sink = 0 } @{ SYS_RESET_POS = 'H'; POWER_CONTROL_OUT = '*' }
Clks 5 @{ POWER_CONTROL_OUT = 'Z' }
Note 'nPWR_RESET_SINK high again: 1024 more clocks'
Step @{ sink = 1 }
Clks 1023
$script:rst = 'H'; Clk @{} @{ SYS_RESET_POS = 'L'; KICK = 'H' }
Clk
$script:rst = '1'; Clk @{} @{ KICK = 'L' }
Clks 2

# ---------------- 13. 68030 RESET instruction ----------------
Note 'set the power-off bit, then the CPU pulls nSYS_RESET low (IOCTRL stays Hi-Z)'
Step @{ as = 0; rw = 0 } @{ nCPU_RD = 'H' }
Clk @{ code = 12; en = 0 } @{ POWER_CONTROL_OUT = 'H' }
Clk @{ code = 0; en = 1 }
Step @{ as = 1; rw = 1 } @{ nCPU_RD = 'L' }
$script:rst = '0'; Step @{} @{ SYS_RESET_POS = 'H'; POWER_CONTROL_OUT = 'Z' }
Clks 6
Note 'the CPU releases it: R47 pulls it high, IOCTRL sees it at the next edge and kicks for one clock'
$script:rst = '1'; Step @{} @{ SYS_RESET_POS = 'L' }
# csim cannot show a pulled-up line that IOCTRL starts to drive on this edge: it reports
# "user expected (1) for nSYS_RESET" here, once. KICK going high is the check.
$script:rst = '1'; Clk @{} @{ KICK = 'H' }
$script:rst = '1'; Clk @{} @{ KICK = 'L' }
Clks 2

# ---------------- 14. FPGA reload: DONE drops ----------------
Note 'DONE drops mid-cycle: reset at once, IDE and RTC selects off, nCPU_RD off'
Step @{ as = 0; rw = 1 }
Clk @{ code = 1 } @{ nIDE1_CSA = 'L'; nIDE1_BUFFER_EN = 'L' }
$script:rst = 'L'; Step @{ done = 0 } @{ nIDE1_CSA = 'H'; nCPU_RD = 'H'; SYS_RESET_POS = 'H' }
Note 'the CPU is reset, so AS floats high on its pull-up: the AS-tied outputs go off'
Step @{ as = 1 } @{ nIDE1_BUFFER_EN = 'H' }
Clks 3

# ---------------- write the .si (CRLF, ASCII) ----------------
$hdr = (Get-Content $Pld -TotalCount 9) -join "`r`n"
$order = 'ORDER: ' + ($inNames -join ', ') + ', %1, nSYS_RESET, %1, ' + ($outNames[0..7] -join ', ') + ', %1, ' +
         ($outNames[8..12] -join ', ') + ', %1, ' + ($outNames[13..15] -join ', ') + ', %1, ' +
         ($outNames[16..20] -join ', ') + ', %1, ' + ($outNames[21..23] -join ', ') + ';'
$body = $hdr + "`r`n`r`n" + $order + "`r`n`r`nVECTORS:`r`n" + ($script:lines -join "`r`n") + "`r`n"
[System.IO.File]::WriteAllText($Out, $body, (New-Object System.Text.ASCIIEncoding))
"gen-ioctrl-si: $($script:count) vectors -> $Out"
