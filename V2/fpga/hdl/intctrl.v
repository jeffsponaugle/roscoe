// intctrl.v - Roscoe 68030 interrupt controller, for the FPGA that replaces the core CPLDs
//
// Revision 1, 22 Nov 2026. Ported from INTCTRL rev 17 (V2/plds/INTCTRLV2/INTCTRL2_REV17.PLD), with the
// assignment agreed in fpga/Roscoe_FPGA_Redesign.md, section 11. Checked in simulation by tb_intctrl.v
// (check-hdl.ps1 runs it); not yet run on hardware.
//
// Today's software sees the same levels, vectors and mask bits as on rev 17:
//
//   mask bit  source             level  vector  request
//   15        unused (was INT7A, the debug button)
//   14        PTC timer 1          6     0xE1   rising edge of tmr_irq[0], latched
//   13        PTC timer 2          6     0xE2   rising edge of tmr_irq[1], latched
//   12        unused (was INT5A)
//   11        IDE1 INTRQ           5     0xD2   irq_ide[0], level, active high
//   10        IDE2 INTRQ           5     0xD4   irq_ide[1], level, active high
//    9        unused (was INT5D)
//    8        UART A               4     0xC1   irq_uart[0], level, active high
//    7        UART B               4     0xC2   irq_uart[1], level, active high
//    6        unused (was INT4C)
//    5        expansion line 1     3     0xB1   irq_exp_n[0], level, active low (connector B21, was INT3A)
//    4        expansion line 2     3     0xB2   irq_exp_n[1], level, active low (connector A23, was INT3B)
//    3        expansion line 3     3     0xB4   irq_exp_n[2], level, active low (connector A24, was INT2A)
//             (each line is bussed to every slot, so one card may use more than one)
//    2        unused (was INT2B)
//    1        RTC                  1     0x91   irq_rtc_n, level, active low
//    0        power controller     1     0x92   a high pulse on pwr_req, filtered, latched
//
// The vector is built as on rev 17: 1, the level (3 bits), then the source's one-hot letter (A = 0001,
// B = 0010, C = 0100, D = 1000). Within a level A wins, then B, C and D. An acknowledge at a level where
// nothing is requesting gets 1LLL0000, which software sends to its spurious-interrupt handler.
//
// Registers, 8 bits on D31-D24. reg_addr is A2-A0; the decoder puts the block at 0x3000_0200, where
// software finds the masks today.
//   0    MASK_HI  read/write  mask bits 15-8 (0x3000_0200)  1 enables the source; reset clears both.
//   1    MASK_LO  read/write  mask bits 7-0  (0x3000_0201)  Unused bits read 0 and ignore writes.
//   2-3           read 0      0x3000_0202 is the power-off/software-reset register, kept elsewhere.
//   4    REQ_HI   read only   request bits 15-8             1 = requesting: a level source's line,
//   5    REQ_LO   read only   request bits 7-0              whatever its mask bit, or an edge's latch.
//   6-7           read 0
// The controller presents REQ AND MASK. Reads have no side effects.
//
// Changes from rev 17, none of which today's software relies on:
//   - The masks and the request bits can be read.
//   - Rev 17's line-430 slip (INT2A's priority term used INT3A) is not carried over.
//   - An edge is latched only while its mask bit is set, as on rev 17, and clearing the bit now drops a
//     latched edge as well. On rev 17 a latched edge still interrupted after its mask bit was cleared.
//   - The vector is captured as the acknowledge starts and held to the end of the cycle; rev 17 decoded
//     it live during the cycle.
//   - The power controller's request can be enabled (rev 17 forced its mask bit to 0). It is filtered,
//     and ignored while the CPU itself drives the line (pwr_drive).
//
// Clocking: everything runs on clk, and only the power filter depends on its frequency (CLK_HZ). The
// FPGA's clock plan is still open. If clk is CPU_CLK, ipl_n changes just after a rising CPU_CLK edge,
// half a clock from where the 68030 samples IPL, as on rev 17. If clk becomes a multiple of CPU_CLK,
// give the IPL register an enable at that phase. ipl_n follows a board line on the third rising clk
// edge after the line changes (the fourth if the first synchroniser flop just misses it), and a timer
// edge on the second. iack_vector is valid from the clock after iack_stb.

`timescale 1ns / 1ps

module intctrl #(
    parameter integer CLK_HZ        = 16000000, // frequency of clk
    parameter integer PWR_FILTER_US = 1000,     // pwr_req must hold each level this long (the AVR's pulse is 50 ms)
    // Level (3 bits) and one-hot letter (4 bits) for each mask bit, bit 15 first. A level of 0 means
    // unused. Changing a level changes that source's vector, so software must follow.
    //                       15   14   13   12   11   10    9    8    7    6    5    4    3    2    1    0
    parameter [47:0] SRC_LEVEL  = {3'd0,3'd6,3'd6,3'd0,3'd5,3'd5,3'd0,3'd4,3'd4,3'd0,3'd3,3'd3,3'd3,3'd0,3'd1,3'd1},
    parameter [63:0] SRC_LETTER = {4'h0,4'h1,4'h2,4'h0,4'h2,4'h4,4'h0,4'h1,4'h2,4'h0,4'h1,4'h2,4'h4,4'h0,4'h1,4'h2}
) (
    input  wire       clk,
    input  wire       rst,             // synchronous, active high: the synchronised nSYS_RESET

    // Interrupt lines from the board, asynchronous to clk (synchronised here)
    input  wire [1:0] irq_ide,         // IDE1, IDE2 INTRQ, active high
    input  wire [1:0] irq_uart,        // UART A, B, active high
    input  wire [2:0] irq_exp_n,       // expansion lines 1-3 (bussed to every slot), active low
    input  wire       irq_rtc_n,       // RTC /IRQ, active low
    input  wire       pwr_req,         // the power controller's request (POWER_CONTROL_OUT net), high pulse

    // From elsewhere in the FPGA, synchronous to clk
    input  wire [1:0] tmr_irq,         // PTC timers 1 and 2: a rising edge requests an interrupt
    input  wire       pwr_drive,       // the power-off bit: 1 while IOCTRL drives POWER_CONTROL_OUT high

    // Register port, from the bus controller
    input  wire       reg_wr,          // one-clock write strobe, already decoded for this block
    input  wire [2:0] reg_addr,        // A2-A0
    input  wire [7:0] reg_wdata,       // D31-D24
    output reg  [7:0] reg_rdata,       // read data for reg_addr (combinational)

    // Interrupt acknowledge, from the bus controller
    input  wire       iack_stb,        // one-clock strobe as an acknowledge cycle starts
    input  wire [2:0] iack_level,      // A3-A1 of that cycle
    output reg  [7:0] iack_vector,     // valid from the clock after iack_stb until the next strobe
    output wire [7:0] iack_vec_nxt,    // the vector iack_vector takes at the end of the strobe (combinational)

    // To the CPU
    output reg  [2:0] ipl_n = 3'b111   // IPL2-IPL0, active low, all three from one register
);

    // ---- Where each source sits in the mask ---------------------------------------------------------
    localparam integer B_TMR1 = 14, B_TMR2 = 13, B_IDE1 = 11, B_IDE2 = 10, B_UARTA = 8, B_UARTB = 7,
                       B_EXP1 = 5, B_EXP2 = 4, B_EXP3 = 3, B_RTC = 1, B_PWR = 0;
    localparam [15:0] EDGE  = (16'd1 << B_TMR1) | (16'd1 << B_TMR2) | (16'd1 << B_PWR);
    localparam [15:0] WIRED = EDGE | (16'd1 << B_IDE1) | (16'd1 << B_IDE2) | (16'd1 << B_UARTA)
                            | (16'd1 << B_UARTB) | (16'd1 << B_EXP1) | (16'd1 << B_EXP2)
                            | (16'd1 << B_EXP3) | (16'd1 << B_RTC);            // 0x6DBB

    function [2:0] lvl_of(input integer b);
        lvl_of = WIRED[b] ? SRC_LEVEL[3*b +: 3] : 3'd0;
    endfunction
    function [3:0] ltr_of(input integer b);
        ltr_of = WIRED[b] ? SRC_LETTER[4*b +: 4] : 4'h0;
    endfunction

    // ---- Board lines: two flip-flops each, then active high -----------------------------------------
    // They start at their idle levels, so nothing shows as requesting while the chain fills.
    localparam [8:0] IDLE = 9'b0_1_111_00_00;   // pwr_req, irq_rtc_n, irq_exp_n[2:0], irq_uart, irq_ide
    (* ASYNC_REG = "TRUE" *) reg [8:0] sync1 = IDLE;
    (* ASYNC_REG = "TRUE" *) reg [8:0] sync2 = IDLE;
    always @(posedge clk) begin
        sync1 <= {pwr_req, irq_rtc_n, irq_exp_n, irq_uart, irq_ide};
        sync2 <= sync1;
    end
    wire [1:0] ide_s  =  sync2[1:0];
    wire [1:0] uart_s =  sync2[3:2];
    wire [2:0] exp_s = ~sync2[6:4];
    wire       rtc_s  = ~sync2[7];
    wire       pwr_s  =  sync2[8];

    // ---- PTC timers: rising edges -------------------------------------------------------------------
    reg  [1:0] tmr_d = 2'b00;
    always @(posedge clk) tmr_d <= tmr_irq;
    wire [1:0] tmr_rise = tmr_irq & ~tmr_d;

    // ---- Power controller request ---------------------------------------------------------------------
    // The AVR answers a power-button press with a 50 ms high pulse, on the line the CPU itself drives
    // high (through IOCTRL's power-off bit) to arm the AVR or power off. R9/R46 pull it low otherwise.
    // A request counts once the line has been low for PWR_CLKS clocks and then high for PWR_CLKS clocks,
    // all while the CPU isn't driving it. That ignores the CPU's own pulses, the line's slow fall after
    // IOCTRL lets go, and any glitch. pwr_drive is the FPGA's copy of the power-off bit, set as the FPGA
    // sends code 12. It may lag IOCTRL's drive by up to PWR_CLKS clocks, since the line must stay high
    // that long before it counts.
    localparam integer PWR_CLKS_RAW = (CLK_HZ / 1000000) * PWR_FILTER_US;
    localparam integer PWR_CLKS     = (PWR_CLKS_RAW < 1) ? 1 : PWR_CLKS_RAW;
    localparam integer PWR_W        = (PWR_CLKS < 2) ? 1 : $clog2(PWR_CLKS);
    reg [PWR_W-1:0] pwr_cnt  = 0;
    reg             pwr_hi   = 1'b0;   // 0: waiting for the line to stay low; 1: to stay high
    reg             pwr_rise = 1'b0;   // one clock: a request
    always @(posedge clk) begin
        pwr_rise <= 1'b0;
        if (rst || pwr_drive) begin
            pwr_cnt <= 0;
            pwr_hi  <= 1'b0;
        end else if (pwr_s != pwr_hi) begin
            pwr_cnt <= 0;                      // not at the level it waits for: start again
        end else if (pwr_cnt == PWR_CLKS - 1) begin
            pwr_cnt  <= 0;
            pwr_hi   <= ~pwr_hi;
            pwr_rise <= pwr_hi;                // it stayed high long enough
        end else begin
            pwr_cnt <= pwr_cnt + 1'b1;
        end
    end

    // ---- Mask register --------------------------------------------------------------------------------
    reg [15:0] mask = 16'h0000;
    always @(posedge clk) begin
        if (rst)                             mask       <= 16'h0000;
        else if (reg_wr && reg_addr == 3'd0) mask[15:8] <= reg_wdata & WIRED[15:8];
        else if (reg_wr && reg_addr == 3'd1) mask[7:0]  <= reg_wdata & WIRED[7:0];
    end

    // ---- Requests, by mask bit ------------------------------------------------------------------------
    reg [15:0] line_req;    // level sources: the line, active high
    reg [15:0] edge_set;    // edge sources: an edge this clock
    always @* begin
        line_req = 16'h0000;
        line_req[B_IDE1]  = ide_s[0];
        line_req[B_IDE2]  = ide_s[1];
        line_req[B_UARTA] = uart_s[0];
        line_req[B_UARTB] = uart_s[1];
        line_req[B_EXP1] = exp_s[0];
        line_req[B_EXP2] = exp_s[1];
        line_req[B_EXP3] = exp_s[2];
        line_req[B_RTC]   = rtc_s;
        edge_set = 16'h0000;
        edge_set[B_TMR1]  = tmr_rise[0];
        edge_set[B_TMR2]  = tmr_rise[1];
        edge_set[B_PWR]   = pwr_rise;
    end

    reg  [15:0] edge_q = 16'h0000;           // latched edges
    wire [15:0] req    = line_req | edge_q;  // the REQ register
    wire [15:0] active = req & mask;         // what the controller presents

    // ---- Priority -------------------------------------------------------------------------------------
    // top: the highest level with an enabled request, for IPL.
    // ack_win: at the level being acknowledged, the letter that wins; ack_bit: its mask bit.
    reg [7:0]  lvl_on;
    reg [2:0]  top;
    reg [3:0]  ack_ltrs, ack_win;
    reg [15:0] ack_bit;
    always @* begin : prio
        integer b;
        lvl_on = 8'h00;
        for (b = 0; b < 16; b = b + 1)
            if (active[b]) lvl_on[lvl_of(b)] = 1'b1;
        top = 3'd0;
        for (b = 1; b < 8; b = b + 1)
            if (lvl_on[b]) top = b[2:0];
    end
    always @* begin : pick
        integer b;
        ack_ltrs = 4'h0;
        for (b = 0; b < 16; b = b + 1)
            if (active[b] && lvl_of(b) == iack_level) ack_ltrs = ack_ltrs | ltr_of(b);
        ack_win = ack_ltrs & (~ack_ltrs + 4'd1);   // the lowest letter set: A, then B, C, D
        for (b = 0; b < 16; b = b + 1)
            ack_bit[b] = active[b] && lvl_of(b) == iack_level && (ltr_of(b) & ack_win) != 4'h0;
    end

    // ---- Edge latches ---------------------------------------------------------------------------------
    // Set by an edge while the mask bit is set; cleared by the source's own acknowledge, by clearing its
    // mask bit, or by reset. An edge in the same clock as the acknowledge is kept.
    always @(posedge clk) begin
        if (rst) edge_q <= 16'h0000;
        else     edge_q <= ((edge_q & ~(iack_stb ? ack_bit : 16'h0000)) | edge_set) & mask & EDGE;
    end

    // ---- Outputs --------------------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) ipl_n <= 3'b111;
        else     ipl_n <= ~top;
    end

    assign iack_vec_nxt = {1'b1, iack_level, ack_win};
    always @(posedge clk) begin
        if (rst)           iack_vector <= 8'h00;
        else if (iack_stb) iack_vector <= iack_vec_nxt;
    end

    always @* begin
        case (reg_addr)
            3'd0:    reg_rdata = mask[15:8];
            3'd1:    reg_rdata = mask[7:0];
            3'd4:    reg_rdata = req[15:8];
            3'd5:    reg_rdata = req[7:0];
            default: reg_rdata = 8'h00;
        endcase
    end

    // ---- Simulation only: check the source table ------------------------------------------------------
    // synthesis translate_off
    initial begin : config_check
        integer a, b;
        for (a = 0; a < 16; a = a + 1)
            if (WIRED[a]) begin
                if (SRC_LEVEL[3*a +: 3] == 3'd0)
                    $display("intctrl CONFIG ERROR: mask bit %0d has a source but level 0", a);
                case (SRC_LETTER[4*a +: 4])
                    4'h1, 4'h2, 4'h4, 4'h8: ;
                    default: $display("intctrl CONFIG ERROR: mask bit %0d's letter %b isn't one-hot", a, SRC_LETTER[4*a +: 4]);
                endcase
                for (b = a + 1; b < 16; b = b + 1)
                    if (WIRED[b] && SRC_LEVEL[3*b +: 3] == SRC_LEVEL[3*a +: 3] && SRC_LETTER[4*b +: 4] == SRC_LETTER[4*a +: 4])
                        $display("intctrl CONFIG ERROR: mask bits %0d and %0d share a level and letter", a, b);
            end
    end
    // synthesis translate_on

endmodule
