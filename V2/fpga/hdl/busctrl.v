// busctrl.v - Roscoe 68030 bus controller: address decode, cycle termination and the device schedule
//
// Revision 1, 22 Nov 2026. Ported from BUSCTRL rev 31 and DEVMAP rev 24 (V2/plds), with IOCTRL's code
// schedule from fpga/Roscoe_FPGA_Redesign.md section 10. The DRAM controller is dramctrl.v and the
// interrupt controller intctrl.v. Checked in simulation by tb_roscoe_top.v; not yet run on hardware.
//
// Clocking (agreed 22 Nov 2026): everything runs on CPU_CLK, on both edges, as the CPLDs did. The
// rising edge that first sees AS asserted is R1; Fk is the falling edge after Rk, so F1 ends S2 and
// is the CPU's first DSACK sample. A registered output that should be seen at Fk changes at Rk. The
// R1 outputs come straight from the AS and address pins, which the 68030 holds stable at that edge.
// If AS arrives too late for R1, the whole cycle simply runs a clock later.
//
// Memory map (as BUSCTRL rev 31 and DEVMAP rev 24; port width in brackets)
//   0x0000_0000-0x07FF_FFFF  boot flash [8]: CE from R1; reads end at FLASH_RD_WS wait states, writes
//                            take 1 and end CE at R2 (CE#-controlled, section 12).
//   0x0800_0000-0x0FFF_FFFF  16-bit flash window [16]: nothing fitted, acknowledged at once as today.
//   0x1000_0000-0x1FFF_FFFF  SRAM [32], 0 wait states; banks 1-4 at 0x1000_0000 + n MB. A write ends
//                            CE at R2, a clock before the cycle ends. (STERM and bursts come later.)
//   0x2000_0000-0x2FFF_FFFF  bus error at F1.
//   0x3000_0000-0x3000_08FF  8-bit on-board devices, 256 bytes each:
//                              0x000 UART A (code 7)   0x100 UART B (8)   0x200 FPGA registers
//                              0x300 RTC (5)           0x400 the new PTC (until it exists, acknowledged
//                              0x600 7-seg A (9)             at once)
//                              0x700 7-seg B (10)      0x800 status LEDs (11)
//                            Reads of the latches are acknowledged at once.
//   The rest of 0x3xxx_xxxx  8-bit card space [8], ended by nEXP_ACK (agreed 22 Nov 2026). Its VIO
//                            window, 0x3000_0500-0x3000_05FF, also drives nEXP_VIO_CS (connector A30)
//                            for cards that don't decode the address themselves (the VGA card).
//   0x4000_0000-0x40FF_FFFF  16-bit on-board [16]: IDE at 0x000-0x3FF (A9 port, A8 CS0/CS1, codes
//                            1-4) and the fast window at 0x800-0xBFF. Other windows, including the
//                            old PTC window 0x400, are acknowledged at once.
//   0x4100_0000-0x4FFF_FFFF  16-bit card space [16].
//   0x5000_0000-0x5FFF_FFFF  32-bit card space [32]. Its first 16 MB, 0x5000_0000-0x50FF_FFFF, is the
//                            VMEM window and also drives nEXP_VMEM_CS (connector A21; 2 MB before).
//                            Both selects are registered at R1 and cut by the AS pin, like U_DATA's.
//   0x6000_0000-0x7FFF_FFFF  no answer: bus error from the bus timeout.
//   0x8000_0000-0xFFFF_FFFF  DRAM [32], 1 wait state (dramctrl.v).
//   CPU space: interrupt acknowledge [8] at 0 wait states with the vector from intctrl. FPU cycles
//   are the 68882's: its select comes from a 74AHCT138 on the 5V side (FC0-2, A17, A19, A16), not
//   from the FPGA, and it ends them itself. Like every other CPU-space cycle the FPGA doesn't answer,
//   they end in the bus timeout if nothing does, so a missing FPU gives the F-line exception.
//   CIIN is asserted for 0x3-0x5, decoded from the address alone: it is a synchronous input of the
//   68030 and must be stable at every rising edge while AS is asserted (data sheet note 13).
//
// FPGA registers, 0x3000_0200-0x3000_020F (A4-A7 don't reach the FPGA, so they repeat every 16 bytes;
// agreed 22 Nov 2026). All 0 wait states except 0x202 writes; reset clears every writable bit.
//   0x200, 0x201  interrupt masks (intctrl)                     0x204, 0x205  interrupt requests (read)
//   0x202         bit 0: power-off request, bit 1: software reset. A write sends IOCTRL code 12 or 13
//                 for bit 0, or code 14 when bit 1 is set, at F2, and takes 1 wait state. A read
//                 returns the power-off bit. Reset clears it, as it clears IOCTRL's copy.
//   0x203         FPGA design version (read only): FPGA_VERSION
//   0x206         control: bit 0 SRAM synchronous cycles (STERM), bit 1 SRAM burst fills (CBACK). Both
//                 start at 0, so the SRAM runs DSACK cycles until software turns them on (the SRAM step
//                 isn't written yet, so they have no effect so far).
//   0x207         scratch: read/write, no effect (a D31-D24 bus test)
//   0x208-0x20F   read 0, writes ignored
//
// Device cycles (IOCTRL codes): "code a-b" means IOCTRL sees the code from Fa to F(b-1); "strobe"
// is nDEV_EN low; "ack k" means DSACK is seen at Fk.
//   IDE read, base       code 1-10, strobe 3-9, ack 8     IDE write, base    code 1-9, strobe 3-8, ack 8
//   IDE read, fast       code 1-5,  strobe 2-4, ack 3     IDE write, fast    code 1-5, strobe 2-4, ack 4
//   RTC read             code 1-6,  strobe 2-5, ack 4     RTC write          code 1-6, strobe 2-5, ack 5
//   UART read            code 1-3 (ends at AS), ack 1     UART write         code 1-2, ack 2
//   latch write          code 1-2, ack 2                  register commands  code and strobe at F2, ack 2
//   In the base IDE window the schedule stops while IORDY is low, checked from R7 (the strobe has
//   been on for a clock by then, past ATA's tA) until DSACK.
//
// Ending a cycle: DSACK is driven asserted from its edge until the rising edge that sees AS negated,
// then driven negated for half a clock, then three-stated (the 68882 shares it). BERR is asserted
// for 0x2 at R1, or by the bus timeout at the TIMEOUT_CLKS-th rising edge after R1 if nothing has
// acknowledged, and is held until AS negates. Card cycles take nEXP_ACK through one flip-flop, so
// DSACK follows a clock after the card's acknowledge: 1 wait state at best.
//
// Data bus (D31-D24 through U_DATA, whose DIR is R/W): for reads of its own registers and for interrupt
// acknowledge the FPGA opens U_DATA and drives the bus from R1, with the data registered in the I/O
// blocks at R2 (the CPU latches it at F2); for writes to its registers it opens U_DATA from R1 and takes
// the data at R2. Both end as soon as AS negates (the AS pin gates them directly), the 68030's hold
// rule #29.
//
// Timing (fpga/roscoe_timing.xdc): AS is the late input, so every register that must act on it at R1
// gets it through one final gate (`go` below); nDSACK_OE comes from an ODDR, and the registered outputs
// sit in the I/O blocks.

`timescale 1ns / 1ps

module busctrl #(
    parameter integer TIMEOUT_CLKS = 240,   // bus error after this many clocks of AS with no answer
    parameter integer FLASH_RD_WS  = 1,     // boot-flash read wait states (rev 31 used 0)
    parameter [7:0]   FPGA_VERSION = 8'h01  // read at 0x3000_0203; bump it with each released bitstream
) (
    input  wire        clk,
    input  wire        rst,                 // synchronised nSYS_RESET, active high

    // CPU bus, from the pins
    input  wire [31:8] a_hi,
    input  wire [3:0]  a_lo,
    input  wire [1:0]  siz,
    input  wire [2:0]  fc,
    input  wire        rw,
    input  wire        as_n,
    input  wire [7:0]  d_in,                // D31-D24
    input  wire        exp_ack_n,
    input  wire        ide_iordy,

    // Cycle termination
    output reg  [1:0]  dsack_d    = 2'b11,  // {DSACK1, DSACK0}, through U_CPU group 1
    output wire        dsack_oe_d1_n,       // nDSACK_OE for the first and second halves of the coming
    output wire        dsack_oe_d2_n,       //   clock: roscoe_top's ODDR drives the pin from these
    output reg         berr_n     = 1'b1,
    output wire        ciin_n,              // decoded from the address alone (see below)

    // Memory selects
    output reg  [3:0]  sram_ce_n  = 4'hF,
    output reg  [3:0]  be_n       = 4'hF,
    output reg         flash_ce_n = 1'b1,

    // Expansion selects, for cards that don't decode the address themselves (connector A21, A30)
    output wire        exp_vmem_cs_n,       // 0x5000_0000-0x50FF_FFFF
    output wire        exp_vio_cs_n,        // 0x3000_0500-0x3000_05FF

    // IOCTRL
    output reg  [3:0]  dev_code   = 4'd0,
    output reg         dev_en_n   = 1'b1,

    // D31-D24 through U_DATA
    output reg  [7:0]  d_out = 8'h00,       // registered at every rising edge (in the I/O blocks)
    output wire        d_oe,
    output wire        data_oe_n,

    // DRAM controller
    output wire        dram_cyc,
    output wire [3:0]  lanes,
    input  wire        dram_ack_a,          // the DRAM access may end, if AS is asserted at this edge

    // Interrupt controller
    output wire        reg_wr,
    output wire [2:0]  reg_addr,
    output wire [7:0]  reg_wdata,
    input  wire [7:0]  intc_rdata,
    output wire        iack_stb,
    output wire [2:0]  iack_level,
    input  wire [7:0]  iack_vector,
    input  wire [7:0]  iack_vec_nxt,        // the vector intctrl latches at the end of iack_stb
    output wire        pwr_drive
);

    // ---- Cycle types, port widths, devices ----------------------------------------------------------
    localparam [3:0] T_NONE = 4'd0, T_BERR = 4'd1, T_ACK = 4'd2, T_FLASH = 4'd3, T_SRAM = 4'd4,
                     T_DRAM = 4'd5, T_REG  = 4'd6, T_IACK = 4'd7, T_DEV = 4'd8,  T_CARD = 4'd9;
    localparam [1:0] W8 = 2'b10, W16 = 2'b01, W32 = 2'b00;        // {DSACK1, DSACK0}, 0 = asserted
    localparam [1:0] K_UART = 2'd0, K_RTC = 2'd1, K_IDE = 2'd2, K_LATCH = 2'd3;

    wire as_now    = !as_n;
    wire cpu_space = &fc;

    // ---- Address decode (settles long before AS) ---------------------------------------------------
    reg [3:0] d_typ;  reg [1:0] d_w;  reg [1:0] d_kind;  reg [3:0] d_code;  reg d_fast;
    always @* begin
        d_typ = T_NONE; d_w = W32; d_kind = K_UART; d_code = 4'd0; d_fast = 1'b0;
        if (cpu_space) begin
            if (a_hi[19:16] == 4'hF) begin d_typ = T_IACK; d_w = W8; end
        end else case (a_hi[31:28])
            4'h0: if (!a_hi[27]) begin d_typ = T_FLASH; d_w = W8;  end
                  else           begin d_typ = T_ACK;   d_w = W16; end
            4'h1: d_typ = T_SRAM;
            4'h2: d_typ = T_BERR;
            4'h3: begin
                d_w = W8;
                if (a_hi[27:24] != 4'h0) d_typ = T_CARD;
                else case (a_hi[23:8])
                    16'h0000, 16'h0001: begin d_typ = T_DEV; d_kind = K_UART; d_code = a_hi[8] ? 4'd8 : 4'd7; end
                    16'h0002:           d_typ = T_REG;
                    16'h0003:           begin d_typ = T_DEV; d_kind = K_RTC; d_code = 4'd5; end
                    16'h0005:           d_typ = T_CARD;
                    16'h0004:           d_typ = T_ACK;    // the new PTC's window, until it exists
                    16'h0006, 16'h0007, 16'h0008:
                        if (rw) d_typ = T_ACK;            // a read emits no code, so it can't clock the latch
                        else begin d_typ = T_DEV; d_kind = K_LATCH; d_code = 4'd3 + a_hi[11:8]; end
                    default:            d_typ = T_CARD;   // the rest of 0x3 is 8-bit card space (0x500 = VIO)
                endcase
            end
            4'h4: begin
                d_w = W16;
                if (a_hi[27:24] != 4'h0) d_typ = T_CARD;
                else if (a_hi[23:12] == 12'h000 && !a_hi[10]) begin
                    d_typ = T_DEV; d_kind = K_IDE; d_code = 4'd1 + {2'b00, a_hi[9:8]}; d_fast = a_hi[11];
                end else d_typ = T_ACK;
            end
            4'h5: d_typ = T_CARD;
            4'h6, 4'h7: d_typ = T_NONE;
            default: d_typ = T_DRAM;
        endcase
    end

    assign dram_cyc = !cpu_space && a_hi[31];
    // The two windows with a board select, for cards that don't decode the address themselves
    wire d_vio  = !cpu_space && (a_hi[31:8] == 24'h3000_05);     // 0x3000_0500-0x3000_05FF
    wire d_vmem = !cpu_space && (a_hi[31:24] == 8'h50);          // 0x5000_0000-0x50FF_FFFF (16 MB)
    // CIIN is one of the 68030's synchronous inputs: it must be stable at every rising edge while AS is
    // asserted (data sheet note 13), so it comes from A31-A28 alone and settles before R1. No FC term is
    // needed: no CPU-space cycle has 3, 4 or 5 there (interrupt acknowledge puts all ones on the address,
    // breakpoint and coprocessor cycles use 0x0000_xxxx-0x0002_xxxx; BUSCTRL rev 30 relies on the same).
    assign ciin_n   = !(a_hi[31:28] == 4'h3 || a_hi[31:28] == 4'h4 || a_hi[31:28] == 4'h5);

    // Byte lanes a write touches (BUSCTRL rev 31's BA32 terms): [0] = D31-D24 ... [3] = D7-D0.
    wire ll = !a_lo[0] && !a_lo[1];
    wire lh = !(a_lo[1] || (!a_lo[0] && siz[0] && !siz[1]));
    wire ul = !((!a_lo[1] && siz[0] && !siz[1]) || (a_lo[1] && a_lo[0]) || (!a_lo[0] && !a_lo[1] && !siz[0] && siz[1]));
    wire uh = (a_lo[1] && siz[1]) || (!siz[1] && !siz[0]) || (a_lo[0] && siz[1] && siz[0]) || (a_lo[0] && a_lo[1]);
    assign lanes = {uh, ul, lh, ll};

    wire       sram_bank_ok = (a_hi[27:22] == 6'd0);
    wire [3:0] sram_bank_1h = 4'd1 << a_hi[21:20];

    // ---- Cycle state ----------------------------------------------------------------------------------
    reg        cyc    = 1'b0;     // AS has been seen; the cycle is under way
    reg  [7:0] pos    = 8'd1;     // schedule position: 1 at R1, frozen while IORDY holds the IDE
    reg  [7:0] tmo    = 8'd1;     // rising edges since R1, for the bus timeout
    reg  [3:0] typ    = T_NONE;   // between cycles these follow the decode; in a cycle they hold it
    reg  [1:0] wid    = W32;
    reg  [1:0] kind   = K_UART;
    reg  [3:0] code   = 4'd0;
    reg        fast   = 1'b0;
    reg        acked  = 1'b0;     // DSACK asserted this cycle (never retracted)
    reg        berred = 1'b0;
    reg        pwr_off = 1'b0;
    reg  [1:0] ctrl    = 2'b00;   // 0x206
    reg  [7:0] scratch = 8'h00;   // 0x207
    reg        vio_q = 1'b0, vmem_q = 1'b0;   // the expansion selects, from R1 until AS negates
    reg        d_oe_q = 1'b0, data_oe_q = 1'b0;
    (* ASYNC_REG = "TRUE" *) reg ack_s    = 1'b0;      // nEXP_ACK, first stage (DSACK is the second)
    (* ASYNC_REG = "TRUE" *) reg iordy_s1 = 1'b1;
    (* ASYNC_REG = "TRUE" *) reg iordy_s2 = 1'b1;

    // IORDY holds the base IDE schedule at positions 6 and 7 (the steps to edges 7 and 8).
    wire hold = cyc && typ == T_DEV && kind == K_IDE && !fast && (pos == 8'd6 || pos == 8'd7) && !iordy_s2;

    // ---- The coming edge, worked out as if AS were asserted at it ------------------------------------
    // AS is the late input: the CPU asserts it up to 20 ns into S1, and the FPGA must act on it at R1,
    // half a clock later. So everything is first worked out from the address and the registered state
    // as if AS were asserted, and AS (with reset) gates each register's input at the very end, through
    // `go`. At R1 (cyc still 0) the decode is taken; later edges keep it and step the schedule.
    wire       go     = as_now && !rst;
    wire [7:0] a_pos  = !cyc ? 8'd1 : (!hold && pos != 8'hFF) ? pos + 8'd1 : pos;
    wire [7:0] a_tmo  = !cyc ? 8'd1 : (tmo != 8'hFF) ? tmo + 8'd1 : tmo;
    wire [3:0] a_typ  = cyc ? typ  : d_typ;
    wire [1:0] a_wid  = cyc ? wid  : d_w;
    wire [1:0] a_kind = cyc ? kind : d_kind;
    wire [3:0] a_code = cyc ? code : d_code;
    wire       a_fast = cyc ? fast : d_fast;

    // ---- Device schedule at the coming edge -----------------------------------------------------------
    reg s_code, s_en, s_ack;
    always @* begin
        s_code = 1'b0; s_en = 1'b0; s_ack = 1'b0;
        case (a_kind)
            K_IDE: if (!a_fast) begin
                       s_code = rw ? (a_pos < 8'd10) : (a_pos < 8'd9);
                       s_en   = (a_pos >= 8'd3) && (rw ? (a_pos < 8'd9) : (a_pos < 8'd8));
                       s_ack  = (a_pos >= 8'd8);
                   end else begin
                       s_code = (a_pos < 8'd5);
                       s_en   = (a_pos >= 8'd2) && (a_pos < 8'd4);
                       s_ack  = rw ? (a_pos >= 8'd3) : (a_pos >= 8'd4);
                   end
            K_RTC: begin
                       s_code = (a_pos < 8'd6);
                       s_en   = (a_pos >= 8'd2) && (a_pos < 8'd5);
                       s_ack  = rw ? (a_pos >= 8'd4) : (a_pos >= 8'd5);
                   end
            K_UART: begin
                       s_code = rw ? (a_pos < 8'd3) : (a_pos < 8'd2);
                       s_ack  = rw ? (a_pos >= 8'd1) : (a_pos >= 8'd2);
                   end
            default: begin                             // K_LATCH (writes only)
                       s_code = (a_pos < 8'd2);
                       s_ack  = (a_pos >= 8'd2);
                   end
        endcase
    end

    // The power-off/software-reset register (0x3000_0202): a write is a command cycle.
    wire       cmd_cyc  = (a_typ == T_REG) && !rw && (a_lo == 4'd2);
    wire [3:0] cmd_code = d_in[1] ? 4'd14 : (d_in[0] ? 4'd12 : 4'd13);

    // ---- Acknowledge at the coming edge ---------------------------------------------------------------
    reg ack_now;
    always @* begin
        case (a_typ)
            T_ACK, T_SRAM, T_IACK: ack_now = 1'b1;
            T_FLASH: ack_now = rw ? (a_pos >= 8'd1 + FLASH_RD_WS) : (a_pos >= 8'd2);
            T_REG:   ack_now = cmd_cyc ? (a_pos >= 8'd2) : 1'b1;
            T_DEV:   ack_now = s_ack;
            T_CARD:  ack_now = (a_pos >= 8'd2) && ack_s;
            T_DRAM:  ack_now = dram_ack_a;
            default: ack_now = 1'b0;                   // T_NONE, T_BERR
        endcase
    end
    wire a_acked = acked || ack_now;                   // acked is 0 at R1: it clears as AS negates
    wire a_berr  = !a_acked && (berred || a_typ == T_BERR || a_tmo >= TIMEOUT_CLKS);
    wire a_rd    = (a_typ == T_REG || a_typ == T_IACK) && rw;
    wire a_wr    = (a_typ == T_REG) && !rw;

    // The FPGA's registers as A3-A0 select them (A4-A7 don't reach the FPGA). intctrl answers 0 and 1
    // (masks) and 4 and 5 (requests), and reads 0 for the others.
    wire [7:0] reg_rd = a_lo[3] ? 8'h00
                      : (intc_rdata | ((a_lo[2:0] == 3'd2) ? {7'd0, pwr_off} : 8'h00)
                                    | ((a_lo[2:0] == 3'd3) ? FPGA_VERSION    : 8'h00)
                                    | ((a_lo[2:0] == 3'd6) ? {6'd0, ctrl}    : 8'h00)
                                    | ((a_lo[2:0] == 3'd7) ? scratch         : 8'h00));

    // ---- Registered outputs: AS reaches each through `go` ------------------------------------------
    always @(posedge clk) begin
        cyc  <= go;
        pos  <= a_pos;  tmo  <= a_tmo;  typ  <= a_typ;  wid  <= a_wid;
        kind <= a_kind; code <= a_code; fast <= a_fast;
        acked  <= go && a_acked;
        berred <= go && a_berr;

        dsack_d <= (go && a_acked) ? a_wid : 2'b11;
        berr_n  <= !(go && a_berr);

        dev_code <= !go ? 4'd0
                  : (a_typ == T_DEV && s_code) ? a_code
                  : (cmd_cyc && a_pos == 8'd2) ? cmd_code : 4'd0;
        dev_en_n <= !(go && ((a_typ == T_DEV && s_en) || (cmd_cyc && a_pos == 8'd2)));

        sram_ce_n  <= (go && a_typ == T_SRAM && sram_bank_ok && (rw || a_pos < 8'd2)) ? ~sram_bank_1h : 4'hF;
        be_n       <= (go && a_typ == T_SRAM) ? (rw ? 4'h0 : ~lanes) : 4'hF;
        flash_ce_n <= !(go && a_typ == T_FLASH && (rw || a_pos < 8'd2));

        // The FPGA drives D31-D24, and opens U_DATA, from R1 for its reads; U_DATA also opens from R1
        // for writes to its registers. Nothing else drives the bus then, and the CPU latches at F2.
        d_oe_q    <= go && a_rd;
        data_oe_q <= go && (a_rd || a_wr);

        if (rst)                                              pwr_off <= 1'b0;
        else if (go && cmd_cyc && a_pos == 8'd2 && !d_in[1]) pwr_off <= d_in[0];

        // 0x206 and 0x207, written at R2 like the masks (reg_wr below)
        if (rst) begin
            ctrl    <= 2'b00;
            scratch <= 8'h00;
        end else if (reg_wr) begin
            if (a_lo[2:0] == 3'd6) ctrl    <= d_in[1:0];
            if (a_lo[2:0] == 3'd7) scratch <= d_in;
        end

        // The expansion selects: decided at R1, then held for the cycle; AS cuts them (below).
        vio_q  <= go && (cyc ? vio_q  : d_vio);
        vmem_q <= go && (cyc ? vmem_q : d_vmem);

        // Read data, registered in the I/O blocks at every rising edge: for an acknowledge, the vector
        // intctrl latches at R2 (and then the latched one); otherwise the register A3-A0 select.
        d_out <= (typ == T_IACK) ? (iack_stb ? iack_vec_nxt : iack_vector) : reg_rd;

        ack_s    <= !exp_ack_n;
        iordy_s1 <= ide_iordy;
        iordy_s2 <= iordy_s1;
    end

    // nDSACK_OE, through roscoe_top's ODDR: on (low) for the whole clock from DSACK's edge; at the edge
    // that sees AS negated, on for the first half only. So DSACK is driven negated for half a clock, then
    // released, as the 68882 shares it.
    assign dsack_oe_d1_n = !((go && a_acked) || acked);
    assign dsack_oe_d2_n = !(go && a_acked);

    // ---- Data bus and the interrupt controller -------------------------------------------------------
    assign d_oe      = d_oe_q && as_now;
    assign data_oe_n = !(data_oe_q && as_now);

    // ---- Expansion selects: on from R1, off as soon as the AS pin negates ------------------------------
    assign exp_vio_cs_n  = !(vio_q  && as_now);
    assign exp_vmem_cs_n = !(vmem_q && as_now);

    assign reg_wr     = cyc && typ == T_REG && !rw && pos == 8'd1 && !a_lo[3];   // data taken at R2
    assign reg_addr   = a_lo[2:0];
    assign reg_wdata  = d_in;
    assign iack_stb   = cyc && typ == T_IACK && pos == 8'd1;                       // vector latched at R2
    assign iack_level = a_lo[3:1];
    assign pwr_drive  = pwr_off;

endmodule
