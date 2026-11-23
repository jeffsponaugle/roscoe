// roscoe_top.v - top level of the Roscoe FPGA (Alchitry Au V2 on the Br Wide)
//
// Revision 1, 22 Nov 2026. The ports are the 101 signals of fpga/roscoe_busctrl_au.xdc, named as there.
// Everything runs on cpu_clk, on both edges (fpga/Roscoe_FPGA_Redesign.md, section 15). Checked in
// simulation by tb_roscoe_top.v; not yet run on hardware.
//
//   busctrl.v   address decode, DSACK/BERR/CIIN, the bus timeout, flash and SRAM selects, IOCTRL's
//               device codes, card cycles, the FPGA's own registers on D31-D24 (the 68882's select is
//               a 74AHCT138 on the 5V side, outside the FPGA)
//   dramctrl.v  DRAM accesses and CAS-before-RAS refresh
//   intctrl.v   interrupts: IPL and vectors
//
// Not yet here: SRAM synchronous cycles and bursts (sterm_n, cback_n and sram_ba stay idle; sram_ba
// follows A3:A2, and cbreq_n is unused), and the PTC, which is being redesigned (its two timer
// requests into intctrl are tied off).

`timescale 1ns / 1ps

module roscoe_top #(
    parameter integer CLK_HZ        = 16000000, // CPU_CLK (Y1)
    parameter integer REFRESH_NS    = 15600,    // DRAM refresh interval (CBR)
    parameter integer RANK_64MB     = 1,        // DRAM rank windows: 1 = 64 MB (J3 = 1-2), 0 = 16 MB
    parameter integer TIMEOUT_CLKS  = 240,      // bus timeout, in clocks (at most 255)
    parameter integer FLASH_RD_WS   = 1,        // boot-flash read wait states
    parameter integer PWR_FILTER_US = 1000      // power-controller request filter
) (
    input  wire         cpu_clk,
    input  wire [3:0]   cpu_a_lo,      // A3-A0
    input  wire [31:8]  cpu_a_hi,      // A31-A8 (A7-A4 don't reach the FPGA)
    input  wire [1:0]   cpu_siz,
    input  wire [2:0]   cpu_fc,
    input  wire         cpu_rw,
    input  wire         cpu_as_n,
    input  wire         sys_reset_n,   // from IOCTRL
    input  wire         exp_ack_n,
    input  wire         ide_iordy,
    input  wire         pwr_req,
    input  wire         cbreq_n,
    input  wire [1:0]   irq_ide,
    input  wire [1:0]   irq_uart,
    input  wire         irq_rtc_n,
    input  wire [2:0]   irq_exp_n,

    inout  wire [31:24] cpu_d,         // through U_DATA (74LVC245A, DIR = R/W)
    output wire         data_oe_n,

    output wire [2:0]   ipl_n,
    output wire [1:0]   dsack_d,
    output wire         dsack_oe_n,
    output wire         sterm_n,
    output wire         ciin_n,
    output wire         cback_n,
    output wire         berr_n,

    output wire [7:0]   ras_n,
    output wire [3:0]   cas_n,
    output wire         dram_we_n,
    output wire         dram_mux,
    output wire         dram_buf_oe_n,

    output wire [3:0]   dev_code,
    output wire         dev_en_n,

    output wire [3:0]   sram_ce_n,
    output wire [3:0]   be_n,
    output wire         flash_ce_n,
    output wire [3:2]   sram_ba,

    output wire         exp_vmem_cs_n, // connector A21, through U_MEM2 ch3
    output wire         exp_vio_cs_n   // connector A30, through U_MEM2 ch8
);

    // ---- Reset: nSYS_RESET from IOCTRL, synchronised; the FPGA starts in reset ---------------------
    (* ASYNC_REG = "TRUE" *) reg [1:0] rst_sync = 2'b11;
    always @(posedge cpu_clk) rst_sync <= {rst_sync[0], !sys_reset_n};
    wire rst = rst_sync[1];

    // ---- Blocks -------------------------------------------------------------------------------------
    wire [7:0] d_out, intc_rdata, reg_wdata, iack_vector, iack_vec_nxt;
    wire [2:0] reg_addr, iack_level;
    wire [3:0] lanes;
    wire       d_oe, reg_wr, iack_stb, pwr_drive, dram_cyc, dram_ack_a, dsack_oe_d1_n, dsack_oe_d2_n;

    busctrl #(.TIMEOUT_CLKS(TIMEOUT_CLKS), .FLASH_RD_WS(FLASH_RD_WS)) u_bus (
        .clk(cpu_clk), .rst(rst),
        .a_hi(cpu_a_hi), .a_lo(cpu_a_lo), .siz(cpu_siz), .fc(cpu_fc), .rw(cpu_rw), .as_n(cpu_as_n),
        .d_in(cpu_d), .exp_ack_n(exp_ack_n), .ide_iordy(ide_iordy),
        .dsack_d(dsack_d), .dsack_oe_d1_n(dsack_oe_d1_n), .dsack_oe_d2_n(dsack_oe_d2_n),
        .berr_n(berr_n), .ciin_n(ciin_n),
        .sram_ce_n(sram_ce_n), .be_n(be_n), .flash_ce_n(flash_ce_n),
        .exp_vmem_cs_n(exp_vmem_cs_n), .exp_vio_cs_n(exp_vio_cs_n),
        .dev_code(dev_code), .dev_en_n(dev_en_n),
        .d_out(d_out), .d_oe(d_oe), .data_oe_n(data_oe_n),
        .dram_cyc(dram_cyc), .lanes(lanes), .dram_ack_a(dram_ack_a),
        .reg_wr(reg_wr), .reg_addr(reg_addr), .reg_wdata(reg_wdata), .intc_rdata(intc_rdata),
        .iack_stb(iack_stb), .iack_level(iack_level), .iack_vector(iack_vector), .iack_vec_nxt(iack_vec_nxt),
        .pwr_drive(pwr_drive));

    dramctrl #(.CLK_HZ(CLK_HZ), .REFRESH_NS(REFRESH_NS), .RANK_64MB(RANK_64MB)) u_dram (
        .clk(cpu_clk), .rst(rst),
        .as_now(!cpu_as_n), .dram_cyc(dram_cyc), .rw(cpu_rw), .lanes(lanes), .a(cpu_a_hi[31:24]),
        .ras_n(ras_n), .cas_n(cas_n), .we_n(dram_we_n), .mux(dram_mux), .buf_oe_n(dram_buf_oe_n),
        .ack_a(dram_ack_a));

    intctrl #(.CLK_HZ(CLK_HZ), .PWR_FILTER_US(PWR_FILTER_US)) u_int (
        .clk(cpu_clk), .rst(rst),
        .irq_ide(irq_ide), .irq_uart(irq_uart), .irq_exp_n(irq_exp_n), .irq_rtc_n(irq_rtc_n),
        .pwr_req(pwr_req),
        .tmr_irq(2'b00),                      // the PTC isn't written yet
        .pwr_drive(pwr_drive),
        .reg_wr(reg_wr), .reg_addr(reg_addr), .reg_wdata(reg_wdata), .reg_rdata(intc_rdata),
        .iack_stb(iack_stb), .iack_level(iack_level), .iack_vector(iack_vector), .iack_vec_nxt(iack_vec_nxt),
        .ipl_n(ipl_n));

    // ---- Pins -----------------------------------------------------------------------------------------
    // nDSACK_OE changes on both clock edges (on at DSACK's rising edge, off half a clock after AS), so it
    // comes from an ODDR in its I/O block: D1 is driven for the first half of the clock, D2 for the second.
    ODDR #(.DDR_CLK_EDGE("SAME_EDGE"), .INIT(1'b1), .SRTYPE("SYNC")) u_dsack_oe (
        .Q(dsack_oe_n), .C(cpu_clk), .CE(1'b1), .D1(dsack_oe_d1_n), .D2(dsack_oe_d2_n), .R(1'b0), .S(1'b0));

    assign cpu_d   = d_oe ? d_out : 8'bz;
    assign sterm_n = 1'b1;                    // SRAM synchronous cycles: a later step
    assign cback_n = 1'b1;
    assign sram_ba = cpu_a_lo[3:2];           // the burst counter replaces this later

endmodule
