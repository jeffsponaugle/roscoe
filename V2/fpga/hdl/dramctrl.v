// dramctrl.v - Roscoe DRAM controller: access cycles and CAS-before-RAS refresh
//
// Revision 1, 22 Nov 2026. Ported from BUSCTRL rev 31 (V2/plds/BUSCTRLV2/BUSCTRL2_REV31.PLD) and
// docs/DRAM.md, for the FPGA that replaces the core CPLDs (fpga/Roscoe_FPGA_Redesign.md). Checked in
// simulation by tb_roscoe_top.v; not yet run on hardware.
// Revision 2, 23 Nov 2026 (Jeff's go-ahead): DRAM_MUX returns to the row address at F(s+2), when RAS has
// risen, instead of after AS negates. With four SIMMs on the address lines the row needs that time to
// settle before a back-to-back access's RAS (Roscoe_FPGA_Redesign.md section 16).
//
// Everything runs on CPU_CLK. Rising edges are numbered as in the design notes: R1 is the first rising
// edge at which AS is seen asserted, and the CPU samples DSACK on the falling edge after each one.
//
// Access (4 clocks, 1 wait state, as rev 31)
//   R(s)    RAS falls on the addressed rank, WE falls for a write, the 245s are enabled.
//   F(s)    DRAM_MUX switches to the column address (half a clock of row hold, tRAH).
//   R(s+1)  CAS falls: all four lanes on a read, the written lanes on a write. DSACK (32-bit) is asserted,
//           so the CPU sees it at F(s+1) and latches read data at F(s+2).
//   R(s+2)  RAS rises (2 clocks low). CAS stays low until AS negates, as FPM and EDO parts need.
//   F(s+2)  DRAM_MUX returns to the row address. CAS latched the column when it fell (the TMS417400's
//           column latch closes on CAS's falling edge), so the column has been held a clock and a half
//           (tCAH), and the next access's RAS is at least a clock and a half away (tASR).
//   AS negates: CAS and WE rise at the next rising edge; the 245s turn off at once.
// s is 1 (R1) unless a refresh is under way, in which case the access starts at the first rising edge
// after the refresh's RAS has been high for a clock. Rank windows follow docs/DRAM.md section 2:
// 64 MB per rank (J3 = 1-2, RANK_64MB = 1) or 16 MB (J3 = 2-3, RANK_64MB = 0). ras_n[0..7] are ranks
// 1A, 2A, 3A, 4A, 1B, 2B, 3B, 4B in address order. Outside the eight windows the cycle runs and is
// acknowledged with no RAS, as today.
//
// Refresh (CBR, every REFRESH_NS)
//   The timer requests a refresh every REFRESH_NS (15.6 us: 249 clocks at 16 MHz). Rev 31 asked every
//   128 clocks (8 us at 16 MHz), because its timer counted CPU_CLK/8 rather than the CPU_CLK/16 its
//   comments describe. A request starts at a rising edge where no DRAM cycle is in progress: between
//   bus cycles, or during any cycle that can't be a DRAM cycle (CS1). It never cuts into a DRAM cycle.
//     r0  start (CAS has been high since at least r0, so tCPN is a clock)
//     r1  every CAS falls
//     r2  every RAS falls (tCSR: 1 clock)
//     r3  every CAS rises (tCHR: 1 clock)
//     r4  every RAS rises (tRAS: 2 clocks); an access waiting for DRAM starts at r5 or later (tRP: 1 clock)
//   WE stays high and the 245s stay off. The timer and refresh keep running through system resets
//   (including the 68030 RESET instruction), so DRAM keeps its contents across them.

`timescale 1ns / 1ps

module dramctrl #(
    parameter integer CLK_HZ     = 16000000,  // frequency of clk
    parameter integer REFRESH_NS = 15600,     // CBR request interval
    parameter integer RANK_64MB  = 1          // 1: 64 MB rank windows (J3 = 1-2); 0: 16 MB (J3 = 2-3)
) (
    input  wire        clk,
    input  wire        rst,       // synchronised nSYS_RESET; stops accesses, not refresh
    input  wire        as_now,    // AS asserted on the pin (sampled at rising edges, used directly below)
    input  wire        dram_cyc,  // the address is DRAM space: A31 = 1 and not CPU space
    input  wire        rw,
    input  wire [3:0]  lanes,     // byte lanes a write touches, in nBE order: [0] = D31-D24 ... [3] = D7-D0
    input  wire [31:24] a,        // for the rank decode
    output reg  [7:0]  ras_n = 8'hFF,
    output reg  [3:0]  cas_n = 4'hF,   // [0] = D7-D0 ... [3] = D31-D24, as the SIMMs' CAS0-3
    output reg         we_n  = 1'b1,
    output reg         mux   = 1'b0,   // 0 = row address, 1 = column (falling-edge register)
    output wire        buf_oe_n,       // the 245s' /OE: on from the start of the access until AS negates
    output wire        ack_a           // DSACK (32-bit) may be asserted from this rising edge, if AS is
                                       //   asserted at it: busctrl gates it with AS
);

    localparam integer REFRESH_CLKS = (CLK_HZ / 1000) * REFRESH_NS / 1000000;
    localparam integer REF_W        = $clog2(REFRESH_CLKS);

    // ---- Rank decode -----------------------------------------------------------------------------------
    wire       rank_ok = RANK_64MB ? (a[31:29] == 3'b100) : (a[31:27] == 5'b10000);
    wire [2:0] rank    = RANK_64MB ? a[28:26] : a[26:24];
    wire [7:0] rank_1h = rank_ok ? (8'd1 << rank) : 8'd0;
    // CAS lines low for this access: all four on a read; on a write, CAS3 = D31-D24 ... CAS0 = D7-D0.
    wire [3:0] cas_on  = rw ? 4'hF : {lanes[0], lanes[1], lanes[2], lanes[3]};

    // ---- Refresh timer and sequence ----------------------------------------------------------------------
    reg [REF_W-1:0] ref_cnt = REFRESH_CLKS - 1;
    reg             ref_req = 1'b0;
    reg [2:0]       ref_t   = 3'd0;   // 0 idle, 1-4 the steps above

    // ---- Access sequence ---------------------------------------------------------------------------------
    // (Kept as written: left to itself, synthesis recodes acc one-hot and drops a bit, with a warning.)
    (* fsm_encoding = "none" *)
    reg [1:0]       acc     = 2'd0;   // 0 idle, 1 RAS low, 2 RAS and CAS low, 3 CAS low (RAS high again)

    // The access's next state is worked out as if AS were asserted at this edge, and AS gates it at the
    // end (`go`): AS is the late input, as busctrl.v explains.
    wire go        = as_now && !rst;
    wire ref_start = ref_req && (ref_t == 3'd0) && !(as_now && dram_cyc);

    reg [1:0] acc_a;
    reg [2:0] n_ref;
    always @* begin
        case (acc)
            2'd0:    acc_a = (dram_cyc && ref_t == 3'd0) ? 2'd1 : 2'd0;   // wait while a refresh runs
            2'd1:    acc_a = 2'd2;
            default: acc_a = 2'd3;
        endcase
        case (ref_t)
            3'd0:    n_ref = ref_start ? 3'd1 : 3'd0;
            3'd4:    n_ref = 3'd0;
            default: n_ref = ref_t + 3'd1;
        endcase
    end

    assign ack_a    = (acc_a >= 2'd2);
    assign buf_oe_n = !((acc != 2'd0) && as_now);

    always @(posedge clk) begin
        // the timer runs all the time; a new request wins over the one being started
        if (ref_start) ref_req <= 1'b0;
        if (ref_cnt == 0) begin
            ref_cnt <= REFRESH_CLKS - 1;
            ref_req <= 1'b1;
        end else begin
            ref_cnt <= ref_cnt - 1'b1;
        end

        acc   <= go ? acc_a : 2'd0;
        ref_t <= n_ref;

        ras_n <= ~(((go && (acc_a == 2'd1 || acc_a == 2'd2)) ? rank_1h : 8'h00)
                 | ((n_ref == 3'd3 || n_ref == 3'd4) ? 8'hFF : 8'h00));
        cas_n <= ~(((go && (acc_a == 2'd2 || acc_a == 2'd3)) ? cas_on : 4'h0)
                 | ((n_ref == 3'd2 || n_ref == 3'd3) ? 4'hF : 4'h0));
        we_n  <= !(go && acc_a != 2'd0 && !rw);
    end

    // the column while RAS is low (acc 1 and 2), the row from the falling edge after RAS rises (acc 3)
    always @(negedge clk) mux <= (acc == 2'd1 || acc == 2'd2);

endmodule
