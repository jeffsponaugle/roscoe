## roscoe_timing.xdc - I/O timing constraints for the Roscoe FPGA (hand-written, 22 Nov 2026; DRAM loads 23 Nov 2026)
##
## Read after roscoe_busctrl_au.xdc, which gen-xdc.ps1 writes from the workbook and which creates cpu_clk
## (62.5 ns) at the FPGA's clock pin. gen-xdc.ps1 never touches this file. The design and its rules are
## in Roscoe_FPGA_Redesign.md, sections 9, 10, 12 and 16.
##
## How the delays are worked out
##   Clock: Y1 drives the 68030, the 68882 and IOCTRL directly. The FPGA's copy passes through U_CLK
##   (74LVC1G34) and 33R, so the CPU's clock edges come uclk_min..uclk_max EARLIER than the FPGA pin's.
##   Every delay below is relative to the FPGA's clock pin, so it carries that skew.
##     input delay  max = CPU output max + buffer max + wire max - uclk_min
##                  min = CPU output min + buffer min + wire min - uclk_max
##     output delay max = wire max + buffer max + receiver setup + uclk_max
##                  min = wire min + buffer min - receiver hold   + uclk_min
##   68030: the 20 MHz column of the 1991 data sheet (datasheets/MC68030_Data_Sheet_1991.pdf, pages 37-40),
##   the slowest grade it lists. A 16 MHz part is no faster, so recheck against the fitted grade.
##   Buffers: data-sheet maxima over -40..85 C. The 5 V parts are TI's (74AHCT541, 74AHCT244 and 74AHCT245
##   in datasheets/). The CY74FCT2257T address muxes' figures (TI SCCS038B) and the 74LVC244A/245A's aren't
##   from sheets in datasheets/ yet, so recheck those.
##   Loads: each 5 V output is timed at its own estimated load (below), not a flat 50 pF. The DRAM lines
##   carry 80-500 pF, and section 16 explains the model.

## ---- Part and board timing, ns ---------------------------------------------------------------------
set tclk   62.5   ;# CPU_CLK period: must match create_clock in roscoe_busctrl_au.xdc
set duty   3.125  ;# Y1's 45-55 % duty cycle: how much a half period can be short
set t6     25.0   ;# 68030 #6:  CLK high to address, FC, SIZ valid (max); #8: invalid from 0 ns (min)
set t9     20.0   ;# 68030 #9:  CLK low to AS asserted (max)
set t12     0.0   ;# 68030 #12: CLK low to AS negated (min 0, max 20)
set t18    25.0   ;# 68030 #18/#20: CLK high to R/W valid (max; min 0)
set t23    25.0   ;# 68030 #23: CLK high to write data valid (max)
set t53     3.0   ;# 68030 #53: write data held after CLK high (min)
set t47a    4.0   ;# 68030 #47A: DSACK, BERR setup to CLK low
set t47b   12.0   ;# 68030 #47B: DSACK, BERR hold after CLK low
set t27     4.0   ;# 68030 #27: read data setup to CLK low
set t60     4.0   ;# 68030 #60: synchronous input (CIIN, STERM, CBACK) setup to CLK high
set lvc_min  1.0  ;# 74LVC244A / 74LVC245A, 3.3 V: propagation min
set lvc_max  5.9  ;#                                propagation max
set lvc_en   8.0  ;#                                output enable max
set buf15    6.5  ;# 74AHCT541 (U_DRAM1-3, U_MEM1/2), 5 V, A to Y: max at 15 pF (a 74AHCT244 is 8.5)
set buf50    9.5  ;#                                   max at 50 pF (the '244 too)
set buf_min  1.0  ;#                                   min
set buf_skew 1.0  ;#                                   output-to-output skew within one package
## DRAM address muxes U26-U28: CY74FCT2257T, CT grade (Jeff, 23 Nov 2026), in the AHC157s' place. FCT outputs
## with 25R series resistors and a TTL-level high. Their timing is measured to 1.5 V at 50 pF.
set mux_s50  5.2  ;# select to output, max at 50 pF (the SN74AHC157 was 11.5)
set mux_i50  4.3  ;# I0/I1 to output, max at 50 pF (AHC157: 9.5)
set mux_min  1.5  ;# min (AHC157: 1.0)
set fct_rlow  46.0 ;# pulling low, max: VOL 0.55 V at 12 mA (25R of it is the series resistor)
set fct_rhigh 53.0 ;# pulling high: the line through VOH 2.4 V at -15 mA and IOS -60 mA at 0 V
set fct_voc   3.2  ;#   and the level that line pulls toward, worst case
set fct_vtop  3.8  ;# the highest level a falling edge starts from (VOH at no load, typical)
set fct_rc50  1.8  ;# the share of the 50 pF delay that is the 50 pF itself (46R x 50 pF, down to 1.5 V)
set x245_pd  9.5  ;# 74AHCT245 (DRAM data, U29-U32): A to B or B to A, max at 50 pF
set x245_en 16.0  ;#                                  OE to outputs on, max at 50 pF
set x245_dis 16.5 ;#                                  OE to outputs off
                  ;# (74HCT245s, as on V2/V3, are 28, 58 and 50 ns, and fail the DRAM checks below)
set drv_slope 0.057 ;# any AHC/AHCT output above 50 pF: ns per pF more (TI's SN74AHC157, 15-to-50 pF maxima)
set drv_ron  55.0   ;# AHC/AHCT output resistance near ground, max (VOL 0.44 V at 8 mA): sets the tail to VIL
set uclk_min 1.0  ;# U_CLK (74LVC1G34) plus its 33R: how much later the FPGA's clock is than the CPU's
set uclk_max 4.0
set wire_min 0.2  ;# headers and traces, one way
set wire_max 1.0
set ioc_su   7.0  ;# IOCTRL (ATF1504AS-10): input setup to its CLK
set ioc_h    0.0  ;#                         input hold
## The SIMMs: -70 parts, from datasheets/TMS417400-DRAM.pdf (tASC, tASR, tWCS and tDS are all 0)
set t_rah  10.0   ;# row address hold after RAS
set t_cah  15.0   ;# column address hold after CAS
set t_rac  70.0   ;# access time from RAS
set t_cac  18.0   ;# access time from CAS
set t_aa   35.0   ;# access time from the column address

## ---- Loads on the 5 V outputs, pF --------------------------------------------------------------------
## The SIMMs: up to four double-sided SIMMs of x4 chips, 16 per SIMM (parity SIMMs have up to 24). The
## TMS417400A's inputs are 5 pF (address) and 7 pF (RAS, CAS, W, DQ). Each SIMM adds about 12 pF of its own
## trace on a line that reaches all its chips, and the main board about 14 pF.
set simm_chips 16
set socks_addr  4 ;# SIMM sockets on each DRAM address bus: 4, one set of muxes for all four, as V2/V3.
                  ;#   (Two sets of AHC157s, 2 sockets each, was the other option; section 16.)
set socks_we    2 ;# sockets on each WE line: 2, WE_A and WE_B on U_DRAM3 (23 Nov 2026); 4 would be one line,
                  ;#   whose edge is too slow for the DRAM's 30 ns tT
set r_strobe   15 ;# ohms, in series with each RAS and CAS line at its buffer
set c_addr  [expr {$socks_addr * ($simm_chips * 5 + 12) + 14}]   ;# 382
set c_we    [expr {$socks_we * ($simm_chips * 7 + 12) + 14}]     ;# 262 (510)
set c_ras   [expr {$simm_chips / 2 * 7 + 8 + 14}]                 ;# 78: one rank
set c_cas   [expr {4 * ($simm_chips / 4 * 7 + 4) + 14}]           ;# 142: one byte lane of every SIMM
set c_mux   [expr {4 / $socks_addr * 3 * 10 + 5}]                 ;# 35: the muxes' select inputs
set c_dq    [expr {4 * (2 * 7 + 4) + 14}]                         ;# 86: a DRAM data line, at the 245s
set c_cpud  70    ;# the CPU's data bus, at the 245s
set c_bufoe 45    ;# nDRAM_BUF_OE: four 245 /OE inputs
set c_berr  30    ;# BERR: the CPU and the U4 header
set c_srsel 40    ;# each SRAM CE and BE: riser chips and connector
set c_flash 20    ;# FLASH_CE: one chip
set c_card  60    ;# nEXP_VMEM_CS, nEXP_VIO_CS: all three slots
set c_sba   80    ;# SRAM_BA2/3: the riser SRAMs' A0/A1

## ---- Buffer delays at those loads, ns (max, slow corner) ----------------------------------------------
## (Each delay handed to a constraint below is rounded to 1 ps; Vivado warns about finer values.)
## A light load interpolates between the 15 pF and 50 pF maxima; a heavy one adds drv_slope per pF above
## 50 pF. A DRAM input takes a falling edge only at VIL (0.8 V), so a DRAM line also adds the tail from
## mid-swing down to VIL, about (output resistance + series resistor) x C. A rising edge is earlier.
set d_berr  [expr {$buf15 + ($buf50 - $buf15) * ($c_berr  - 15) / 35.0}]
set d_srsel [expr {$buf15 + ($buf50 - $buf15) * ($c_srsel - 15) / 35.0}]
set d_flash [expr {$buf15 + ($buf50 - $buf15) * ($c_flash - 15) / 35.0}]
set d_bufoe [expr {$buf15 + ($buf50 - $buf15) * ($c_bufoe - 15) / 35.0}]
set d_card  [expr {$buf50 + $drv_slope * ($c_card - 50)}]
set d_sba   [expr {$buf50 + $drv_slope * ($c_sba  - 50)}]
set d_mux   [expr {$c_mux <= 50 ? $buf15 + ($buf50 - $buf15) * ($c_mux - 15) / 35.0 : $buf50 + $drv_slope * ($c_mux - 50)}]
## At the far SIMM, to VIL: the strobes through their resistors, and WE
set d_ras   [expr {$buf50 + $drv_slope * ($c_ras - 50) + ($r_strobe + $drv_ron) * $c_ras / 1000.0}]   ;# 16.6
set d_cas   [expr {$buf50 + $drv_slope * ($c_cas - 50) + ($r_strobe + $drv_ron) * $c_cas / 1000.0}]   ;# 24.7
set d_we    [expr {$buf50 + $drv_slope * ($c_we  - 50) + $drv_ron * $c_we / 1000.0}]                  ;# 36.0 (63.8)
## The address muxes' outputs at the far SIMM: an RC edge, the later of a falling bit reaching VIL (from
## fct_vtop through fct_rlow) and a rising bit reaching VIH, 2.4 V (toward fct_voc through fct_rhigh)
set d_afall [expr {$fct_rlow  * $c_addr / 1000.0 * log($fct_vtop / 0.8)}]               ;# 27.4
set d_arise [expr {$fct_rhigh * $c_addr / 1000.0 * log($fct_voc / ($fct_voc - 2.4))}]   ;# 28.1
set d_col   [expr {$mux_s50 - $fct_rc50 + max($d_afall, $d_arise)}]   ;# select to column: 31.5 (AHC157: 51.4)
set d_row   [expr {$mux_i50 - $fct_rc50 + max($d_afall, $d_arise)}]   ;# CPU address to row: 30.6 (49.4)
## The 245s: onto the CPU's data bus (reads), onto the DRAM data lines (writes), and turning on
set d_xcpu  [expr {$x245_pd + $drv_slope * ($c_cpud - 50)}]
set d_xdq   [expr {$x245_pd + $drv_slope * ($c_dq - 50)}]
set d_xen   [expr {$x245_en + $drv_slope * ($c_dq - 50)}]

## ---- Clock: Y1's duty cycle, 45-55 %, shortens the half-clock paths by up to 3.1 ns ---------------------
set_clock_uncertainty -setup $duty -rise_from [get_clocks cpu_clk] -fall_to [get_clocks cpu_clk]
set_clock_uncertainty -setup $duty -fall_from [get_clocks cpu_clk] -rise_to [get_clocks cpu_clk]

## ---- Inputs from the CPU (U_IN1-U_IN5 and U_DATA) --------------------------------------------------------
## Address, FC, SIZ and R/W change after the CPU's rising edge that starts S0; the FPGA takes them at R1.
set cpu_addr [get_ports {cpu_a_hi[*] cpu_a_lo[*] cpu_fc[*] cpu_siz[*] cpu_rw}]
set_input_delay -clock cpu_clk -max [expr {$t6 + $lvc_max + $wire_max - $uclk_min}] $cpu_addr
set_input_delay -clock cpu_clk -min [expr {0.0 + $lvc_min + $wire_min - $uclk_max}] $cpu_addr
## AS changes after the CPU's falling edges (S1 asserts it, S5 negates it); the FPGA takes it at the next
## rising edge, R1. This is the half-clock path every 0-wait-state cycle depends on.
set_input_delay -clock cpu_clk -clock_fall -max [expr {$t9 + $lvc_max + $wire_max - $uclk_min}] [get_ports cpu_as_n]
set_input_delay -clock cpu_clk -clock_fall -min [expr {$t12 + $lvc_min + $wire_min - $uclk_max}] [get_ports cpu_as_n]
## Write data on D31-D24 comes through U_DATA after the CPU's rising edge at S2 (R1); the FPGA takes it at R2.
## (U_DATA's enable comes on at R1 and is constrained below; it is on long before the data arrives.)
set_input_delay -clock cpu_clk -max [expr {$t23 + $lvc_max + $wire_max - $uclk_min}] [get_ports {cpu_d[*]}]
set_input_delay -clock cpu_clk -min [expr {$t53 + $lvc_min + $wire_min - $uclk_max}] [get_ports {cpu_d[*]}]
## No same-edge hold on these inputs. The CPU changes the address, FC, SIZ and R/W only after the rising
## edge that starts S0, with AS negated, and holds them to the end of S5. Every register that takes them
## at that edge is a don't-care: either AS gates it off (busctrl's `go`), or it is loaded again at R1 from
## the new address (cyc = 0 there). Write data changes after R1 and is only taken at R2. Vivado can't know
## that: with the min input delay below zero and the clock's ~4.5 ns insertion delay (no MMCM), it pads
## these nets by several ns to meet a hold check nothing depends on. The padding then slows CIIN and
## SRAM_BA, which share the nets. (A real hold, against the next cycle's change, has clocks to spare.)
set_false_path -hold -from [get_ports {cpu_a_hi[*] cpu_a_lo[*] cpu_fc[*] cpu_siz[*] cpu_rw cpu_d[*]}]
## Asynchronous inputs, each synchronised in the FPGA. (The zero input delay only keeps check_timing quiet.)
set async_in [get_ports {sys_reset_n exp_ack_n ide_iordy pwr_req irq_ide[*] irq_uart[*] irq_rtc_n irq_exp_n[*]}]
set_input_delay -clock cpu_clk 0.0 $async_in
set_false_path -from $async_in

## ---- Outputs to the CPU ---------------------------------------------------------------------------------
## DSACK (U_CPU, 74LVC244A) and BERR (U_MEM2, 74AHCT541) are sampled at the CPU's falling edge (#47A/#47B).
## When DSACK is first asserted, U_CPU's enable (nDSACK_OE) comes on at the same edge.
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_max  + $t47a + $uclk_max}] [get_ports {dsack_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $lvc_min  - $t47b + $uclk_min}] [get_ports {dsack_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_en   + $t47a + $uclk_max}] [get_ports dsack_oe_n]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $lvc_min  - $t47b + $uclk_min}] [get_ports dsack_oe_n]
set_output_delay -clock cpu_clk -clock_fall -max [expr {round(1000 * ($wire_max + $d_berr + $t47a + $uclk_max)) / 1000.0}] [get_ports berr_n]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $buf_min  - $t47b + $uclk_min}] [get_ports berr_n]
## Read data on D31-D24 (the FPGA's registers and interrupt vectors) must be at the CPU by its latch edge
## (F2) less #27. The data is registered in the I/O blocks at R2. U_DATA's enable and the FPGA's own
## output enable come on a clock earlier, at R1, so they get a clock and a half (the multicycle paths),
## and the data itself only pays U_DATA's propagation delay. The CPU's hold on this data is against AS
## negating (#29), which the design meets by construction: AS itself turns the drivers off (the
## pin-to-pin limit below).
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_max + $t27 + $uclk_max}] [get_ports {cpu_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -min 0.0 [get_ports {cpu_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_en + $t27 + $uclk_max}] [get_ports data_oe_n]
set_output_delay -clock cpu_clk -clock_fall -min 0.0 [get_ports data_oe_n]
set_multicycle_path -setup 2 -from [get_cells {u_bus/d_oe_q_reg u_bus/data_oe_q_reg}] -to [get_ports {cpu_d[*] data_oe_n}]
set_multicycle_path -hold  1 -from [get_cells {u_bus/d_oe_q_reg u_bus/data_oe_q_reg}] -to [get_ports {cpu_d[*] data_oe_n}]
## IPL is asynchronous to the CPU, which synchronises it (and wants it stable on two falling edges).
## (The zero output delay only keeps check_timing quiet.)
set_output_delay -clock cpu_clk 0.0 [get_ports {ipl_n[*]}]
set_false_path -to [get_ports {ipl_n[*]}]

## ---- Outputs to IOCTRL (direct): DEV_CODE and nDEV_EN, sampled on its falling CPU_CLK edge ---------------
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $ioc_su + $uclk_max}] [get_ports {dev_code[*] dev_en_n}]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min - $ioc_h  + $uclk_min}] [get_ports {dev_code[*] dev_en_n}]

## ---- SRAM, flash and card selects (U_MEM1/2) -----------------------------------------------------------
## Each select must be past its buffer, at its own load, within the half clock it is launched in. That is
## stricter than any access needs (section 16 has the memory budgets), but it keeps every edge where the
## design puts it. The card selects nEXP_VMEM_CS and nEXP_VIO_CS (U_MEM2) are registered at R1 the same way.
set_output_delay -clock cpu_clk -clock_fall -max [expr {round(1000 * ($wire_max + $d_srsel + $uclk_max)) / 1000.0}] [get_ports {sram_ce_n[*] be_n[*]}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {round(1000 * ($wire_max + $d_flash + $uclk_max)) / 1000.0}] [get_ports flash_ce_n]
set_output_delay -clock cpu_clk -clock_fall -max [expr {round(1000 * ($wire_max + $d_card + $uclk_max)) / 1000.0}] [get_ports {exp_vmem_cs_n exp_vio_cs_n}]
set_output_delay -clock cpu_clk -clock_fall -min 0.0 [get_ports {sram_ce_n[*] be_n[*] flash_ce_n exp_vmem_cs_n exp_vio_cs_n}]

## ---- DRAM (U_DRAM1-3, the address muxes and the 245s) -------------------------------------------------
## The FPGA's own window, as for the selects: RAS, CAS, WE and nDRAM_BUF_OE leave the FPGA in time to clear
## a 50 pF buffer within their half clock. win_max is the clock-to-out that allows, 13.6 ns.
set dram_out [get_ports {ras_n[*] cas_n[*] dram_we_n dram_buf_oe_n}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $buf50 + $uclk_max}] $dram_out
set_output_delay -clock cpu_clk -clock_fall -min 0.0 $dram_out
set win_max [expr {$tclk / 2.0 - $duty - ($wire_max + $buf50 + $uclk_max)}]
##
## Then the real loads, against the SIMMs' own limits. Most DRAM limits compare two edges the FPGA sends at
## different clock edges. Vivado times one pin at a time, so each check below times one edge and credits the
## other with a conservative figure:
##   - Both edges at the slow corner, with every part at its data-sheet maximum for its real load. (The SIMMs,
##     buffers and FPGA share one board's temperature and supply. Section 16 also works the fast corner and
##     mixed corners.) Edges through one package (U_DRAM2: DRAM_MUX and CAS) differ by the skew at most.
##   - The other edge's FPGA delay at fpga_floor: at the slow corner no output leaves sooner after its clock
##     edge (report_datasheet: 8.2 ns for the I/O-block registers, 9.4 for CAS, 9.5 for RAS).
##   - A deadline a clock or more away is written against the edge Vivado checks by default (the next rising
##     edge for the rising-edge output delays below), less the extra time.
set fpga_floor 8.0
##
## DRAM_MUX: launched at a falling edge, so these are against the next rising edge.
##   Rising (to the column), the larger of:
##   - tASC: the column at the DRAM (VIL) before CAS, launched at that rising edge, reaches VIL.
##   - tAA: read data reaches the CPU by its latch edge (F3, a clock and a half later) less #27.
##   Falling (back to the row, at F3 since dramctrl.v rev 2): tASR: the row at the DRAM before the next
##   access's RAS reaches VIL. That RAS is launched at R5 at the earliest, a clock and a half later, so the
##   check is a clock longer than Vivado's default half clock.
##   Hold (rising): tRAH: the row held 10 ns after RAS reaches VIL. Its worst case is the fast corner, which
##   Vivado's hold check uses: the row can't change sooner than the buffers' minimum delays after DRAM_MUX
##   leaves the pin. RAS is taken at the window's ceiling plus its line at a strong part (20 ohms).
##   Hold (falling): tCAH: the column held 15 ns after CAS, launched a clock and a half before (at R2) and
##   taken at the window's ceiling with its real load, reaches VIL. Vivado's hold edge is half a clock back.
set mux_asc [expr {$d_mux + $d_col + $wire_max - ($fpga_floor + $d_cas - $buf_skew + $wire_min)}]
set mux_aa  [expr {$d_mux + $d_col + $wire_max + $t_aa + $d_xcpu + $wire_max + $t27 + $uclk_max - 1.5 * $tclk}]
set mux_asr [expr {$d_mux + $d_col + $wire_max - ($fpga_floor + $d_ras + $wire_min) - $tclk}]
set ras_strong [expr {$buf_min + ($r_strobe + 20.0) * $c_ras / 1000.0 + $wire_min}]
set mux_rah [expr {($buf_min + $mux_min + $wire_min) - $t_rah - ($win_max + $ras_strong) - $duty}]
set mux_cah [expr {(1.5 * $tclk - $duty) + ($buf_min + $mux_min + $wire_min) - ($win_max + $d_cas + $wire_max) - $t_cah - 0.5 * $tclk}]
set_output_delay -clock cpu_clk -max -rise [expr {round(1000 * max($mux_asc, $mux_aa)) / 1000.0}] [get_ports dram_mux]
set_output_delay -clock cpu_clk -max -fall [expr {round(1000 * $mux_asr) / 1000.0}] [get_ports dram_mux]
set_output_delay -clock cpu_clk -min -rise [expr {round(1000 * $mux_rah) / 1000.0}] [get_ports dram_mux]
set_output_delay -clock cpu_clk -min -fall [expr {round(1000 * $mux_cah) / 1000.0}] [get_ports dram_mux]
##
## RAS, against the next rising edge (added to the window):
##   - tRAC: read data reaches the CPU by F3 less #27 (R2 is Vivado's edge; F3 is half a clock later, or
##     less by the duty cycle).
##   - Hold: tASR on an access after idle time: RAS must not reach VIL before the row address does. The row
##     comes from the CPU's address (#6 after S0, at its slowest) through the muxes; RAS at its fastest.
set ras_rac [expr {$d_ras + $wire_max + $t_rac + $d_xcpu + $wire_max + $t27 + $uclk_max - (1.5 * $tclk - $duty)}]
set row_at  [expr {-$tclk - $uclk_min + $t6 + $d_row + $wire_max}]
set ras_asr [expr {-($row_at - ($buf_min + 0.69 * $r_strobe * $c_ras / 1000.0 + $wire_min))}]
set_output_delay -clock cpu_clk -max -add_delay [expr {round(1000 * $ras_rac) / 1000.0}] [get_ports {ras_n[*]}]
set_output_delay -clock cpu_clk -min -add_delay [expr {round(1000 * $ras_asr) / 1000.0}] [get_ports {ras_n[*]}]
##
## CAS, against the next rising edge:
##   - tCAC: read data reaches the CPU by F3 less #27 (half a clock after R2, less the duty cycle).
##   - Hold: tDS: write data at the DRAM before CAS reaches VIL. It comes from the CPU (#23 after its R1)
##     through the 245s, which nDRAM_BUF_OE (taken at the window's ceiling) turns on at R1.
set cas_cac [expr {$d_cas + $wire_max + $t_cac + $d_xcpu + $wire_max + $t27 + $uclk_max + $duty - 0.5 * $tclk}]
set data_at [expr {max(-$tclk - $uclk_min + $t23 + $d_xdq + $wire_max, -$tclk + $win_max + $d_bufoe + $wire_max + $d_xen + $wire_max)}]
set cas_ds  [expr {-($data_at - ($buf_min + 0.69 * $r_strobe * $c_cas / 1000.0 + $wire_min))}]
set_output_delay -clock cpu_clk -max -add_delay [expr {round(1000 * $cas_cac) / 1000.0}] [get_ports {cas_n[*]}]
set_output_delay -clock cpu_clk -min -add_delay [expr {round(1000 * $cas_ds) / 1000.0}] [get_ports {cas_n[*]}]
##
## WE, against the next rising edge: tWCS: WE at VIL before CAS, launched a clock later, reaches VIL (both
## at the slow corner; WE_A/WE_B on U_DRAM3, CAS on U_DRAM2).
set we_wcs [expr {$d_we + $wire_max - ($fpga_floor + $d_cas - $buf_skew + $wire_min)}]
set_output_delay -clock cpu_clk -max -add_delay [expr {round(1000 * $we_wcs) / 1000.0}] [get_ports dram_we_n]
set_output_delay -clock cpu_clk -min -add_delay 0.0 [get_ports dram_we_n]

## Registers go into the I/O blocks where the output timing is tight: DSACK, the device codes, read data,
## and nDSACK_OE (an ODDR, which always is). Those pins sit next to AS (bank 35), so AS reaches them in
## time too. DRAM_MUX and IPL don't depend on AS. The memory selects and BERR stay in the fabric, between
## AS (which they need at R1) and their pins on the far side of the die; their output limits leave room.
set_property IOB TRUE [get_ports {dsack_d[*] dev_code[*] dev_en_n dram_mux ipl_n[*]}]
set_property IOB TRUE [get_cells {u_bus/d_out_reg[*]}]
## nDSACK_OE changes on a falling edge only when DSACK is released, half a clock after the edge that saw
## AS negated. The CPU no longer samples DSACK then (#28: it may negate any time after AS, from 0 ns), and
## the line doesn't move, as it is already driven high. So that edge has no hold requirement.
set_false_path -hold -fall_from [get_clocks cpu_clk] -to [get_ports dsack_oe_n]

## ---- Pin-to-pin paths, with no clock edge in between ------------------------------------------------------
## A set_max_delay between ports counts the ports' input and output delays too, so each limit below is the
## whole path; the comment gives the FPGA's share. Each has a set_min_delay 0 beside it, which checks
## nothing (-datapath_only already drops the hold check): without it, check_timing reports every source
## port as having only a max input delay ("partial_input_delay"), which would hide a real one.
set in_as   [expr {$t9 + $lvc_max + $wire_max - $uclk_min}]     ;# the AS input delay above
set out_d   [expr {$wire_max + $lvc_en + $t27 + $uclk_max}]      ;# the D31-D24 / nDATA_OE output delay above
set out_win [expr {$wire_max + $buf50 + $uclk_max}]             ;# the DRAM window's output delay above
set out_card [expr {$wire_max + $d_card + $uclk_max}]           ;# the card selects' output delay above
## AS negating turns off U_DATA, the FPGA's D31-D24 drivers and the DRAM 245s. #29A wants the data bus
## released within 50 ns of AS at the CPU: 6.9 (U_IN5 and wiring) + the FPGA + 8 (U_DATA), or + U_DRAM2 and
## the 245s turning off. FPGA share: 12 ns for U_DATA; for the 245s, whatever the 50 ns leaves (16.6 ns with
## 74AHCT245s; with 74HCT245s, which take up to 50 ns to turn off, nothing is left). AS also ends the card
## selects, held to 12 ns so a card sees its select go about when it sees AS go.
set as_245 [expr {50.0 - ($lvc_max + $wire_max) - ($d_bufoe + $wire_max + $x245_dis)}]
set_max_delay -datapath_only [expr {$in_as + 12.0 + $out_d}]    -from [get_ports cpu_as_n] -to [get_ports {data_oe_n cpu_d[*]}]
set_min_delay 0.0                                               -from [get_ports cpu_as_n] -to [get_ports {data_oe_n cpu_d[*]}]
set_max_delay -datapath_only [expr {round(1000 * ($in_as + $as_245 + $out_win)) / 1000.0}] -from [get_ports cpu_as_n] -to [get_ports dram_buf_oe_n]
set_min_delay 0.0                                               -from [get_ports cpu_as_n] -to [get_ports dram_buf_oe_n]
set_max_delay -datapath_only [expr {round(1000 * ($in_as + 12.0 + $out_card)) / 1000.0}] -from [get_ports cpu_as_n] -to [get_ports {exp_vmem_cs_n exp_vio_cs_n}]
set_min_delay 0.0                                               -from [get_ports cpu_as_n] -to [get_ports {exp_vmem_cs_n exp_vio_cs_n}]
## (The register-read multiplexer, A3-A0 to D31-D24, is no longer one of these: the read data is
## registered in the I/O blocks, so the address input delay above covers it.)
## CIIN (U_CPU) must be at the CPU #60 before the first rising edge of the cycle, R1 (data sheet note 13).
## Counted from S0's rising edge at the FPGA pin, as the address input delay is.
set_max_delay -datapath_only [expr {$tclk - $uclk_max - $t60 - $lvc_max - $wire_max}] -from [get_ports {cpu_a_hi[*]}] -to [get_ports ciin_n]
set_min_delay 0.0 -from [get_ports {cpu_a_hi[*]}] -to [get_ports ciin_n]
## (The 68882's select is no longer the FPGA's: a 74AHCT138 on the 5V side makes it, as section 16 explains.)
## SRAM_BA (U_MEM2, U_DRAM2) is the SRAM's A1:A0, needed with the rest of the address once CE falls after R1.
set_max_delay -datapath_only [expr {round(1000 * ($tclk - $uclk_max - $d_sba - $wire_max)) / 1000.0}] -from [get_ports {cpu_a_lo[*]}] -to [get_ports {sram_ba[*]}]
set_min_delay 0.0 -from [get_ports {cpu_a_lo[*]}] -to [get_ports {sram_ba[*]}]
