## roscoe_timing.xdc - I/O timing constraints for the Roscoe FPGA (hand-written, 22 Nov 2026)
##
## Read after roscoe_busctrl_au.xdc, which gen-xdc.ps1 writes from the workbook and which creates cpu_clk
## (62.5 ns) at the FPGA's clock pin. gen-xdc.ps1 never touches this file. The design and its rules are
## in Roscoe_FPGA_Redesign.md, sections 10, 12 and 16.
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
##   Buffers: data-sheet maxima for the parts named in section 9, at 50 pF and full temperature,
##   from the TI/Nexperia sheets; none of those sheets is in datasheets/ yet, so recheck them too.

## ---- Part and board timing, ns ---------------------------------------------------------------------
set tclk   62.5   ;# CPU_CLK period: must match create_clock in roscoe_busctrl_au.xdc
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
set ahct_min 1.0  ;# 74AHCT244, 5 V: propagation min
set ahct_max 8.5  ;#                  propagation max
set ahct_skew 1.0 ;#                  output-to-output skew within one package (DRAM_MUX and CAS share U_DRAM2)
set mux157  11.0  ;# SN74AHC157 (DRAM address mux): select to output max
set uclk_min 1.0  ;# U_CLK (74LVC1G34) plus its 33R: how much later the FPGA's clock is than the CPU's
set uclk_max 4.0
set wire_min 0.2  ;# headers and traces, one way
set wire_max 1.0
set ioc_su   7.0  ;# IOCTRL (ATF1504AS-10): input setup to its CLK
set ioc_h    0.0  ;#                         input hold

## ---- Clock: Y1's duty cycle, 45-55 %, shortens the half-clock paths by up to 3.1 ns ---------------------
set_clock_uncertainty -setup 3.125 -rise_from [get_clocks cpu_clk] -fall_to [get_clocks cpu_clk]
set_clock_uncertainty -setup 3.125 -fall_from [get_clocks cpu_clk] -rise_to [get_clocks cpu_clk]

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
## DSACK (U_CPU, 74LVC244A) and BERR (U_MEM2, 74AHCT244) are sampled at the CPU's falling edge (#47A/#47B).
## When DSACK is first asserted, U_CPU's enable (nDSACK_OE) comes on at the same edge.
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_max  + $t47a + $uclk_max}] [get_ports {dsack_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $lvc_min  - $t47b + $uclk_min}] [get_ports {dsack_d[*]}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $lvc_en   + $t47a + $uclk_max}] [get_ports dsack_oe_n]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $lvc_min  - $t47b + $uclk_min}] [get_ports dsack_oe_n]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $ahct_max + $t47a + $uclk_max}] [get_ports berr_n]
set_output_delay -clock cpu_clk -clock_fall -min [expr {$wire_min + $ahct_min - $t47b + $uclk_min}] [get_ports berr_n]
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

## ---- Outputs to memory (74AHCT244) ------------------------------------------------------------------------
## Each select must be past its buffer within the half clock it is launched in. That is stricter than any
## access needs (section 16 has the memory budgets), but it keeps every edge where the design puts it.
set mem_sel [get_ports {ras_n[*] cas_n[*] dram_we_n dram_buf_oe_n sram_ce_n[*] be_n[*] flash_ce_n}]
set_output_delay -clock cpu_clk -clock_fall -max [expr {$wire_max + $ahct_max + $uclk_max}] $mem_sel
set_output_delay -clock cpu_clk -clock_fall -min 0.0 $mem_sel
## DRAM_MUX is launched at a falling edge, and the column address must be at the DRAM before CAS falls at
## the next rising edge (tASC = 0). MUX and CAS go through the same 74AHCT244 (U_DRAM2), so only that
## package's output skew counts against MUX, plus the AHC157 and the wiring; CAS's own FPGA delay is taken
## as zero.
set_output_delay -clock cpu_clk -max [expr {$wire_max - $wire_min + $ahct_skew + $mux157}] [get_ports dram_mux]
set_output_delay -clock cpu_clk -min 0.0 [get_ports dram_mux]

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
set out_mem [expr {$wire_max + $ahct_max + $uclk_max}]           ;# the memory-select output delay above
## AS negating turns off U_DATA, the FPGA's D31-D24 drivers and the DRAM 245s. #29A wants the data bus
## released within 50 ns of AS at the CPU: 6.9 (U_IN5 and wiring) + the FPGA + 8 (U_DATA) or 9.5 (AHCT)
## + ~25 (HCT245). FPGA share: 12 ns.
set_max_delay -datapath_only [expr {$in_as + 12.0 + $out_d}]   -from [get_ports cpu_as_n] -to [get_ports {data_oe_n cpu_d[*]}]
set_min_delay 0.0                                              -from [get_ports cpu_as_n] -to [get_ports {data_oe_n cpu_d[*]}]
set_max_delay -datapath_only [expr {$in_as + 12.0 + $out_mem}] -from [get_ports cpu_as_n] -to [get_ports dram_buf_oe_n]
set_min_delay 0.0                                              -from [get_ports cpu_as_n] -to [get_ports dram_buf_oe_n]
## (The register-read multiplexer, A3-A0 to D31-D24, is no longer one of these: the read data is
## registered in the I/O blocks, so the address input delay above covers it.)
## CIIN (U_CPU) must be at the CPU #60 before the first rising edge of the cycle, R1 (data sheet note 13).
## Counted from S0's rising edge at the FPGA pin, as the address input delay is.
set_max_delay -datapath_only [expr {$tclk - $uclk_max - $t60 - $lvc_max - $wire_max}] -from [get_ports {cpu_a_hi[*]}] -to [get_ports ciin_n]
set_min_delay 0.0 -from [get_ports {cpu_a_hi[*]}] -to [get_ports ciin_n]
## (The 68882's select is no longer the FPGA's: a 74AHCT138 on the 5V side makes it, as section 16 explains.)
## SRAM_BA (U_MEM2, U_DRAM2) is the SRAM's A1:A0, needed with the rest of the address once CE falls after R1.
set_max_delay -datapath_only [expr {$tclk - $uclk_max - $ahct_max - $wire_max}] -from [get_ports {cpu_a_lo[*]}] -to [get_ports {sram_ba[*]}]
set_min_delay 0.0 -from [get_ports {cpu_a_lo[*]}] -to [get_ports {sram_ba[*]}]
