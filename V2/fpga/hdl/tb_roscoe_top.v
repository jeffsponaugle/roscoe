// tb_roscoe_top.v - self-checking testbench for roscoe_top.v (busctrl, dramctrl and intctrl together)
// check-hdl.ps1 runs it in Vivado's xsim. It prints each failure and ends with "TB PASS: n checks" or
// "TB FAIL: e of n checks failed".
//
// A 68030 bus model runs real cycles: address 15 ns into S0, AS 20 ns into S1, write data 20 ns into
// S2, DSACK and BERR sampled on falling edges, read data latched on the falling edge after. Each
// cycle records the FPGA's outputs at every falling edge F1, F2, ..., which is where the CPU and
// IOCTRL sample them. The expected schedules are copied from fpga/Roscoe_FPGA_Redesign.md section 10
// and the memory map from BUSCTRL rev 31 / DEVMAP rev 24, not taken from the HDL.
//
// Monitors, all through the run:
//   - DRAM: every RAS line low at least 2 clocks and high at least 1 between lows; every CAS line high
//     at least 1 clock between lows; RAS lines fall one at a time (an access) or all together (refresh);
//     DRAM_MUX already high at the falling edge before an access's CAS falls (tASC).
//   - Refresh: CAS before RAS (CAS low a clock before RAS falls, high a clock before RAS rises), WE
//     high and the 245s off, every request served within REFRESH_LATENCY clocks.
//   - D31-D24: the FPGA never drives against U_DATA, and U_DATA never opens on a read the FPGA doesn't
//     drive.
//   - DSACK: three-stated during FPU cycles; STERM and CBACK stay negated.

`timescale 1ns / 1ps

module tb_roscoe_top;

    localparam real    TCLK            = 62.5;          // 16 MHz
    localparam integer CLK_HZ          = 16000000;
    localparam integer REFRESH_NS      = 15600;
    localparam integer REFRESH_CLKS    = (CLK_HZ / 1000) * REFRESH_NS / 1000000;   // 249
    localparam integer TIMEOUT         = 40;            // short bus timeout for simulation
    localparam integer FLASH_WS        = 1;
    localparam integer REFRESH_LATENCY = 6;             // clocks from request to start, worst case
    localparam [2:0] FC_SD = 3'b101, FC_CPU = 3'b111;
    localparam [1:0] SZ_L = 2'b00, SZ_B = 2'b01, SZ_W = 2'b10, SZ_3 = 2'b11;

    // ---- clock, CPU side, board models ---------------------------------------------------------------
    reg clk = 1'b0;
    always #(TCLK / 2) clk = ~clk;

    reg  [31:0] A = 32'hFFFF_FFFF;
    reg  [2:0]  FC = FC_SD;
    reg  [1:0]  SIZ = SZ_L;
    reg         RW = 1'b1, AS_n = 1'b1;
    reg  [7:0]  cpu_wd = 8'h00;
    reg         cpu_wd_en = 1'b0;
    reg         sys_reset_n = 1'b0, exp_ack_n = 1'b1, ide_iordy = 1'b1, pwr_req = 1'b0, cbreq_n = 1'b1;
    reg  [1:0]  irq_ide = 2'b00, irq_uart = 2'b00;
    reg         irq_rtc_n = 1'b1;
    reg  [2:0]  irq_exp_n = 3'b111;

    wire [31:24] cpu_d;           // FPGA side of U_DATA
    wire [7:0]   dbus;            // CPU side, D31-D24
    wire        data_oe_n, dsack_oe_n, sterm_n, ciin_n, cback_n, berr_n, dram_we_n, dram_mux, dram_buf_oe_n;
    wire        dev_en_n, flash_ce_n;
    wire [2:0]  ipl_n;
    wire [1:0]  dsack_d;
    wire [7:0]  ras_n;
    wire [3:0]  cas_n, dev_code, sram_ce_n, be_n;
    wire [3:2]  sram_ba;

    // U_DATA (74LVC245A, DIR = R/W): on a read it drives the CPU side from the FPGA side, on a write the
    // FPGA side from the CPU side, and only while nDATA_OE is low.
    assign cpu_d = (!data_oe_n && !RW) ? dbus : 8'bz;
    assign dbus  = cpu_wd_en ? cpu_wd : ((!data_oe_n && RW) ? cpu_d : 8'bz);
    wire [1:0] dsack_cpu = dsack_oe_n ? 2'b11 : dsack_d;          // R51/R52 pull-ups

    roscoe_top #(.CLK_HZ(CLK_HZ), .REFRESH_NS(REFRESH_NS), .TIMEOUT_CLKS(TIMEOUT), .FLASH_RD_WS(FLASH_WS),
                 .PWR_FILTER_US(1)) dut (
        .cpu_clk(clk), .cpu_a_lo(A[3:0]), .cpu_a_hi(A[31:8]), .cpu_siz(SIZ), .cpu_fc(FC), .cpu_rw(RW),
        .cpu_as_n(AS_n), .sys_reset_n(sys_reset_n), .exp_ack_n(exp_ack_n), .ide_iordy(ide_iordy),
        .pwr_req(pwr_req), .cbreq_n(cbreq_n), .irq_ide(irq_ide), .irq_uart(irq_uart),
        .irq_rtc_n(irq_rtc_n), .irq_exp_n(irq_exp_n),
        .cpu_d(cpu_d), .data_oe_n(data_oe_n), .ipl_n(ipl_n), .dsack_d(dsack_d), .dsack_oe_n(dsack_oe_n),
        .sterm_n(sterm_n), .ciin_n(ciin_n), .cback_n(cback_n), .berr_n(berr_n),
        .ras_n(ras_n), .cas_n(cas_n), .dram_we_n(dram_we_n), .dram_mux(dram_mux), .dram_buf_oe_n(dram_buf_oe_n),
        .dev_code(dev_code), .dev_en_n(dev_en_n), .sram_ce_n(sram_ce_n), .be_n(be_n),
        .flash_ce_n(flash_ce_n), .sram_ba(sram_ba));

    // A card: pulls nEXP_ACK low card_delay ns after AS falls (no card if negative), lets go with AS.
    real card_delay = -1.0;
    always @(negedge AS_n) if (card_delay >= 0.0) begin
        #(card_delay);
        if (!AS_n) exp_ack_n = 1'b0;
    end
    always @(posedge AS_n) #5 exp_ack_n = 1'b1;

    // A drive that holds IORDY low for iordy_low ns, starting 20 ns after IOCTRL would assert its
    // strobe (just after the first falling edge that sees nDEV_EN low).
    real  iordy_low = 0.0;
    reg   en_prev = 1'b1;
    event strobe_on;
    always @(negedge clk) begin
        if (!dev_en_n && en_prev) -> strobe_on;
        en_prev = dev_en_n;
    end
    always @(strobe_on) if (iordy_low > 0.0) begin
        #20 ide_iordy = 1'b0;
        #(iordy_low) ide_iordy = 1'b1;
    end

    // ---- checks ---------------------------------------------------------------------------------------
    integer errors = 0, checks = 0;
    reg [8*32-1:0] ctx = "start";

    task chk(input ok, input [8*56-1:0] what);
        begin
            checks = checks + 1;
            if (ok !== 1'b1) begin
                errors = errors + 1;
                $display("FAIL at %0t, %0s: %0s", $realtime, ctx, what);
            end
        end
    endtask

    task chk_eq(input [31:0] got, input [31:0] want, input [8*40-1:0] what);
        begin
            checks = checks + 1;
            if (got !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t, %0s: %0s is 0x%0h, expected 0x%0h", $realtime, ctx, what, got, want);
            end
        end
    endtask

    // ---- the 68030 bus model --------------------------------------------------------------------------
    localparam integer MAXE = 600;
    reg [3:0] e_code [1:MAXE];  reg e_en   [1:MAXE];  reg [7:0] e_ras [1:MAXE];  reg [3:0] e_cas [1:MAXE];
    reg       e_we   [1:MAXE];  reg e_mux  [1:MAXE];  reg       e_boe [1:MAXE];  reg [3:0] e_sce [1:MAXE];
    reg [3:0] e_be   [1:MAXE];  reg e_fce  [1:MAXE];  reg       e_ciin[1:MAXE];
    reg       e_doe  [1:MAXE];

    task rec(input integer k);      // the outputs as the CPU and IOCTRL see them at this falling edge
        begin
            if (k <= MAXE) begin
                e_code[k] = dev_code;  e_en[k]  = dev_en_n;   e_ras[k] = ras_n;   e_cas[k] = cas_n;
                e_we[k]   = dram_we_n; e_mux[k] = dram_mux;   e_boe[k] = dram_buf_oe_n;
                e_sce[k]  = sram_ce_n; e_be[k]  = be_n;       e_fce[k] = flash_ce_n;
                e_ciin[k] = ciin_n;    e_doe[k] = data_oe_n;
            end
        end
    endtask

    integer   c_ws, c_edges;        // wait states (DSACK or BERR seen at F(c_ws+1)); edges recorded
    reg       c_berr;
    reg [1:0] c_dsack;
    reg [7:0] c_rd;
    real      t_as, t_r1;

    // One bus cycle. It starts at the next rising edge (S0) and returns just after AS negates, so
    // cycles run back to back; with tail = 1 it records one more falling edge and returns there.
    task cycle(input [31:0] addr, input [2:0] fc, input [1:0] siz, input rw, input [7:0] wd, input tail);
        integer k;
        reg done;
        begin
            @(posedge clk);                                   // S0
            #5  cpu_wd_en = 1'b0;                             // the last write's data goes
            #10 begin A = addr; FC = fc; SIZ = siz; RW = rw; end
            @(negedge clk);                                   // S1
            #20 AS_n = 1'b0;
            t_as = $realtime;
            @(posedge clk);                                   // S2 (R1)
            t_r1 = $realtime;
            if (!rw) begin #20 cpu_wd = wd; cpu_wd_en = 1'b1; end
            k = 0; done = 1'b0; c_berr = 1'b0; c_dsack = 2'b11;
            while (!done) begin
                @(negedge clk); k = k + 1;                    // Fk
                rec(k);
                if (!berr_n)                    begin c_berr = 1'b1; done = 1'b1; end
                else if (dsack_cpu != 2'b11)    begin c_dsack = dsack_cpu; done = 1'b1; end
                else if (k >= 500)              begin done = 1'b1; chk(1'b0, "cycle never ended"); end
            end
            c_ws = k - 1;
            @(negedge clk); k = k + 1;                        // read data latched here
            rec(k);
            c_rd = dbus;
            c_edges = k;
            #10 AS_n = 1'b1;
            if (tail) begin @(negedge clk); k = k + 1; rec(k); c_edges = k; end
        end
    endtask

    task rd(input [31:0] addr, input [1:0] siz);  begin cycle(addr, FC_SD, siz, 1'b1, 8'h00, 1'b1); end endtask
    task wr(input [31:0] addr, input [1:0] siz, input [7:0] d); begin cycle(addr, FC_SD, siz, 1'b0, d, 1'b1); end endtask

    task ends(input integer ws, input [1:0] dsack);   // the cycle ended by DSACK, this wide, this late
        begin
            chk(!c_berr, "ended by DSACK, not BERR");
            chk_eq(c_ws, ws, "wait states");
            chk_eq(c_dsack, dsack, "DSACK1:0");
        end
    endtask

    task bus_error(input integer ws);
        begin
            chk(c_berr, "ended by BERR");
            chk_eq(c_ws, ws, "falling edge of BERR - 1");
        end
    endtask

    // Device schedule: code from Fc_on to F(c_off - 1), nDEV_EN low from Fe_on to F(e_off - 1), DSACK
    // seen at F(ack); every other recorded edge must show code 0 and nDEV_EN high.
    task sched(input [3:0] code, input integer c_on, input integer c_off, input integer e_on, input integer e_off,
               input integer ack);
        integer j;
        reg [3:0] wc;
        reg       we;
        begin
            chk(!c_berr, "ended by DSACK, not BERR");
            chk_eq(c_ws, ack - 1, "wait states");
            for (j = 1; j <= c_edges; j = j + 1) begin
                wc = (j >= c_on && j < c_off) ? code : 4'd0;
                we = (j >= e_on && j < e_off) ? 1'b0 : 1'b1;
                checks = checks + 2;
                if (e_code[j] !== wc) begin
                    errors = errors + 1;
                    $display("FAIL, %0s: DEV_CODE at F%0d is %0d, expected %0d", ctx, j, e_code[j], wc);
                end
                if (e_en[j] !== we) begin
                    errors = errors + 1;
                    $display("FAIL, %0s: nDEV_EN at F%0d is %b, expected %b", ctx, j, e_en[j], we);
                end
            end
        end
    endtask

    // The IORDY hold, as the design notes define it: the schedule stops at positions 6 and 7 while
    // IORDY, sampled two rising edges earlier, is low. Gives the DSACK edge for a low pulse of h ns
    // that starts 20 ns after F3.
    function integer iordy_ack(input real h);
        integer p, j, m;
        reg     low;
        begin
            p = 6; j = 7;
            while (p < 8) begin
                m   = j - 2;                                  // IORDY sampled at R(m)
                low = (m >= 4) && ((m - 3) * TCLK < 51.25 + h);
                if (!low) p = p + 1;
                j = j + 1;
            end
            iordy_ack = j - 1;
        end
    endfunction

    // A card's DSACK edge: nEXP_ACK is taken at the first rising edge after it falls (R1 at the
    // earliest), and DSACK follows at the next rising edge.
    function integer card_ack(input real delay);
        integer c;
        begin
            c = 1;
            while (t_r1 + (c - 1) * TCLK <= t_as + delay) c = c + 1;
            card_ack = c + 1;
        end
    endfunction

    // ---- DRAM and refresh monitors (sampled 1 ns after each rising edge) -----------------------------
    reg  [7:0] m_ras = 8'hFF;
    reg  [3:0] m_cas = 4'hF;
    real       ras_edge [0:7];
    real       cas_edge [0:3];
    real       ref_times [0:255];
    integer    n_ref = 0, req_age = 0, max_req_age = 0, mi;
    initial for (mi = 0; mi < 8; mi = mi + 1) begin ras_edge[mi] = -1000.0; if (mi < 4) cas_edge[mi] = -1000.0; end

    reg m_mux_f = 1'b0;                                 // DRAM_MUX 1 ns after the last falling edge
    always @(negedge clk) begin #1 m_mux_f = dram_mux; end

    always @(posedge clk) begin
        #1;
        // an access's CAS falls (one RAS line low) only after the column address is through the mux
        if ((m_cas & ~cas_n) != 4'h0 && ras_n != 8'hFF && ras_n != 8'h00) begin
            checks = checks + 1;
            if (m_mux_f !== 1'b1) begin
                errors = errors + 1; $display("FAIL at %0t: CAS fell without DRAM_MUX high at the falling edge before", $realtime);
            end
        end
        for (mi = 0; mi < 8; mi = mi + 1) begin
            if (m_ras[mi] && !ras_n[mi]) begin
                checks = checks + 1;
                if ($realtime - ras_edge[mi] < TCLK - 1.0) begin
                    errors = errors + 1; $display("FAIL at %0t: RAS%0d precharge under a clock", $realtime, mi);
                end
                ras_edge[mi] = $realtime;
            end
            if (!m_ras[mi] && ras_n[mi]) begin
                checks = checks + 1;
                if ($realtime - ras_edge[mi] < 2.0 * TCLK - 1.0) begin
                    errors = errors + 1; $display("FAIL at %0t: RAS%0d low under 2 clocks", $realtime, mi);
                end
                ras_edge[mi] = $realtime;
            end
        end
        for (mi = 0; mi < 4; mi = mi + 1) begin
            if (m_cas[mi] && !cas_n[mi]) begin
                checks = checks + 1;
                if ($realtime - cas_edge[mi] < TCLK - 1.0) begin
                    errors = errors + 1; $display("FAIL at %0t: CAS%0d precharge under a clock", $realtime, mi);
                end
                cas_edge[mi] = $realtime;
            end
            if (!m_cas[mi] && cas_n[mi]) cas_edge[mi] = $realtime;
        end
        // RAS lines fall one at a time (an access) or all together (refresh)
        if (ras_n != 8'hFF && ras_n != 8'h00) begin
            checks = checks + 1;
            if ((~ras_n & (~ras_n - 8'd1)) != 8'h00) begin
                errors = errors + 1; $display("FAIL at %0t: RAS lines 0x%h low together", $realtime, ~ras_n);
            end
        end
        // ...and an access and a refresh never run into each other without every RAS going high first
        if ((m_ras == 8'h00 && ras_n != 8'h00 && ras_n != 8'hFF) || (m_ras != 8'hFF && m_ras != 8'h00 && ras_n == 8'h00)) begin
            errors = errors + 1; $display("FAIL at %0t: refresh and access RAS back to back (0x%h then 0x%h)", $realtime, m_ras, ras_n);
        end
        checks = checks + 1;
        // refresh: CAS before RAS, WE high, buffers off
        if (ras_n == 8'h00) begin
            checks = checks + 2;
            if (dram_we_n !== 1'b1 || dram_buf_oe_n !== 1'b1) begin
                errors = errors + 1; $display("FAIL at %0t: WE or the 245s active during refresh", $realtime);
            end
            if (m_ras == 8'hFF) begin                          // RAS just fell
                if (m_cas !== 4'h0 || cas_n !== 4'h0) begin
                    errors = errors + 1; $display("FAIL at %0t: refresh RAS fell without CAS low a clock before", $realtime);
                end
                if (n_ref < 256) ref_times[n_ref] = $realtime;
                n_ref = n_ref + 1;
            end
        end
        if (m_ras == 8'h00 && ras_n == 8'hFF) begin            // refresh RAS just rose
            checks = checks + 1;
            if (m_cas !== 4'hF) begin
                errors = errors + 1; $display("FAIL at %0t: refresh CAS still low a clock before RAS rose", $realtime);
            end
        end
        m_ras = ras_n;
        m_cas = cas_n;
        // how long the current refresh request has waited
        if (dut.u_dram.ref_req) begin
            req_age = req_age + 1;
            if (req_age > max_req_age) max_req_age = req_age;
        end else req_age = 0;
    end

    // ---- bus monitors ---------------------------------------------------------------------------------
    task bus_check;
        begin
            if (!data_oe_n && !RW && dut.d_oe) begin
                errors = errors + 1; $display("FAIL at %0t: the FPGA drives D31-D24 against U_DATA (write)", $realtime);
            end
            if (!data_oe_n && RW && !dut.d_oe) begin
                errors = errors + 1; $display("FAIL at %0t: U_DATA open on a read the FPGA doesn't drive", $realtime);
            end
            if (!AS_n && FC == FC_CPU && A[19:16] == 4'h2 && !dsack_oe_n) begin
                errors = errors + 1; $display("FAIL at %0t: DSACK driven during an FPU cycle", $realtime);
            end
            if (sterm_n !== 1'b1 || cback_n !== 1'b1) begin
                errors = errors + 1; $display("FAIL at %0t: STERM or CBACK asserted", $realtime);
            end
        end
    endtask
    always @(posedge clk)  begin #2 bus_check; end
    always @(negedge clk)  begin #2 bus_check; end
    always @(posedge AS_n) begin #2 bus_check; end      // the drivers must let go as AS negates

    // ---- the tests ----------------------------------------------------------------------------------------
    integer i, j, n, ws_max, r0;
    reg [7:0] exp_be;

    initial begin
        $timeformat(-9, 1, " ns", 1);
        repeat (10) @(posedge clk);
        sys_reset_n = 1'b1;
        repeat (4) @(posedge clk);

        // ---- SRAM: 0 wait states, banks, byte enables; writes end CE a clock early
        ctx = "SRAM";
        rd(32'h1000_0000, SZ_L);
        ends(0, 2'b00);
        chk_eq(e_sce[1], 4'b1110, "SRAM CE at F1"); chk_eq(e_sce[2], 4'b1110, "SRAM CE at F2");
        chk_eq(e_sce[3], 4'b1111, "SRAM CE after the cycle");
        chk_eq(e_be[1], 4'b0000, "BE on a read"); chk_eq(e_ciin[1], 1'b1, "CIIN");
        wr(32'h1030_0004, SZ_L, 8'h5A);
        ends(0, 2'b00);
        chk_eq(e_sce[1], 4'b0111, "SRAM CE at F1 (bank 4)"); chk_eq(e_sce[2], 4'b1111, "SRAM CE at F2 (write ended)");
        rd(32'h1040_0000, SZ_L);
        ends(0, 2'b00);
        chk_eq(e_sce[1], 4'b1111, "no bank above 4 MB");
        for (i = 0; i < 16; i = i + 1) begin           // size x offset, nBE0 = D31-D24 ... nBE3 = D7-D0
            case (i)
                0:  exp_be = 4'b0000;  1: exp_be = 4'b0001;  2: exp_be = 4'b0011;  3: exp_be = 4'b0111;   // long
                4:  exp_be = 4'b1110;  5: exp_be = 4'b1101;  6: exp_be = 4'b1011;  7: exp_be = 4'b0111;   // byte
                8:  exp_be = 4'b1100;  9: exp_be = 4'b1001; 10: exp_be = 4'b0011; 11: exp_be = 4'b0111;   // word
                12: exp_be = 4'b1000; 13: exp_be = 4'b0001; 14: exp_be = 4'b0011; default: exp_be = 4'b0111; // 3 bytes
            endcase
            wr(32'h1000_0010 + (i % 4), i / 4, 8'h00);
            chk_eq(e_be[1], exp_be, "BE on a write");
        end
        rd(32'h1000_000C, SZ_L);
        chk_eq(sram_ba, 2'b11, "SRAM_BA follows A3:A2");

        // ---- boot flash and the 16-bit flash window
        ctx = "boot flash";
        rd(32'h0000_0100, SZ_B);
        ends(FLASH_WS, 2'b10);
        for (j = 1; j <= FLASH_WS + 2; j = j + 1) chk_eq(e_fce[j], 1'b0, "flash CE during the read");
        chk_eq(e_fce[FLASH_WS + 3], 1'b1, "flash CE after the read");
        wr(32'h0000_0100, SZ_B, 8'hA5);
        ends(1, 2'b10);
        chk_eq(e_fce[1], 1'b0, "flash CE at F1 (write)"); chk_eq(e_fce[2], 1'b1, "flash CE ends at R2");
        ctx = "16-bit flash window";
        rd(32'h0800_0000, SZ_W);
        ends(0, 2'b01);
        chk_eq(e_fce[1], 1'b1, "no flash CE");

        // ---- bus errors
        ctx = "0x2 region";        rd(32'h2000_0000, SZ_L);  bus_error(0);
        ctx = "0x6 region";        rd(32'h6000_0000, SZ_L);  bus_error(TIMEOUT - 1);
        ctx = "0x7 region";        wr(32'h7FFF_FFFC, SZ_L, 8'h00); bus_error(TIMEOUT - 1);
        ctx = "FPU, none fitted";  cycle(32'h0002_2000, FC_CPU, SZ_L, 1'b1, 8'h00, 1'b1);
        bus_error(TIMEOUT - 1);    chk_eq(e_fce[1], 1'b1, "no flash CE");     // the 68882's select is outside the FPGA
        ctx = "breakpoint cycle";  cycle(32'h0000_0000, FC_CPU, SZ_B, 1'b1, 8'h00, 1'b1);
        bus_error(TIMEOUT - 1);    chk_eq(e_fce[1], 1'b1, "no flash CE");
        ctx = "card space, no card"; rd(32'h3100_0000, SZ_B); bus_error(TIMEOUT - 1);

        // ---- interrupt acknowledge and the FPGA registers
        ctx = "IACK, nothing pending";
        cycle(32'hFFFF_FFF9, FC_CPU, SZ_B, 1'b1, 8'h00, 1'b1);   // level 4
        ends(0, 2'b10);
        chk_eq(c_rd, 8'hC0, "spurious vector");
        chk_eq(e_doe[1], 1'b0, "U_DATA open at F1");   chk_eq(e_doe[2], 1'b0, "U_DATA open at F2");
        chk_eq(e_doe[3], 1'b1, "U_DATA closed after AS");
        ctx = "FPGA registers";
        wr(32'h3000_0200, SZ_B, 8'h01);                            // MASK_HI: UART A (mask bit 8)
        ends(0, 2'b10);
        chk_eq(e_doe[1], 1'b0, "U_DATA open at F1 (write)"); chk_eq(e_ciin[1], 1'b0, "CIIN");
        for (j = 1; j <= c_edges; j = j + 1) chk_eq(e_code[j], 4'd0, "no device code");
        rd(32'h3000_0200, SZ_B);  ends(0, 2'b10);  chk_eq(c_rd, 8'h01, "MASK_HI");
        wr(32'h3000_0201, SZ_B, 8'hFF);
        rd(32'h3000_0201, SZ_B);  chk_eq(c_rd, 8'hBB, "MASK_LO (only bits with a source)");
        rd(32'h3000_0204, SZ_B);  chk_eq(c_rd, 8'h00, "REQ_HI");
        rd(32'h3000_0208, SZ_B);  chk_eq(c_rd, 8'h00, "0x208 reads 0");
        wr(32'h3000_0208, SZ_B, 8'hFF);
        rd(32'h3000_0200, SZ_B);  chk_eq(c_rd, 8'h01, "0x208 is not a mask");
        rd(32'h3000_0210, SZ_B);  chk_eq(c_rd, 8'h01, "0x210 repeats 0x200 (no A4-A7)");

        ctx = "interrupt";
        irq_uart[0] = 1'b1;
        repeat (6) @(posedge clk);
        chk_eq(ipl_n, 3'b011, "IPL for UART A (level 4)");
        cycle(32'hFFFF_FFF9, FC_CPU, SZ_B, 1'b1, 8'h00, 1'b1);
        ends(0, 2'b10);
        chk_eq(c_rd, 8'hC1, "vector for UART A");
        rd(32'h3000_0204, SZ_B);  chk_eq(c_rd, 8'h01, "REQ_HI shows UART A");
        irq_uart[0] = 1'b0;
        repeat (6) @(posedge clk);
        chk_eq(ipl_n, 3'b111, "IPL idle");
        wr(32'h3000_0200, SZ_B, 8'h00);  wr(32'h3000_0201, SZ_B, 8'h00);

        ctx = "power register";
        wr(32'h3000_0202, SZ_B, 8'h01);  sched(4'd12, 2, 3, 2, 3, 2);
        rd(32'h3000_0202, SZ_B);  chk_eq(c_rd, 8'h01, "power-off bit set");
        chk_eq(dut.u_int.pwr_drive, 1'b1, "pwr_drive");
        wr(32'h3000_0202, SZ_B, 8'h00);  sched(4'd13, 2, 3, 2, 3, 2);
        rd(32'h3000_0202, SZ_B);  chk_eq(c_rd, 8'h00, "power-off bit clear");
        wr(32'h3000_0202, SZ_B, 8'h02);  sched(4'd14, 2, 3, 2, 3, 2);
        wr(32'h3000_0202, SZ_B, 8'h01);  wr(32'h3000_0202, SZ_B, 8'h03);  sched(4'd14, 2, 3, 2, 3, 2);
        rd(32'h3000_0202, SZ_B);  chk_eq(c_rd, 8'h01, "a reset command leaves the bit");
        wr(32'h3000_0202, SZ_B, 8'h00);

        // ---- IOCTRL device schedules (section 10)
        ctx = "UART A read";      rd(32'h3000_0000, SZ_B);         sched(4'd7,  1, 3, 0, 0, 1);
        chk_eq(e_ciin[1], 1'b0, "CIIN");
        ctx = "UART B write";     wr(32'h3000_0100, SZ_B, 8'h41);  sched(4'd8,  1, 2, 0, 0, 2);
        ctx = "RTC read";         rd(32'h3000_0300, SZ_B);         sched(4'd5,  1, 6, 2, 5, 4);
        ctx = "RTC write";        wr(32'h3000_0300, SZ_B, 8'h00);  sched(4'd5,  1, 6, 2, 5, 5);
        ctx = "7-seg A write";    wr(32'h3000_0600, SZ_B, 8'h3F);  sched(4'd9,  1, 2, 0, 0, 2);
        ctx = "7-seg B write";    wr(32'h3000_0700, SZ_B, 8'h06);  sched(4'd10, 1, 2, 0, 0, 2);
        ctx = "status LED write"; wr(32'h3000_0800, SZ_B, 8'hFF);  sched(4'd11, 1, 2, 0, 0, 2);
        ctx = "latch read";       rd(32'h3000_0600, SZ_B);         sched(4'd0,  0, 0, 0, 0, 1);
        ctx = "empty 8-bit window"; rd(32'h3000_0900, SZ_B);       sched(4'd0,  0, 0, 0, 0, 1);
        chk_eq(c_dsack, 2'b10, "8-bit");
        ctx = "IDE1 CS0 base read";  rd(32'h4000_0000, SZ_W);       sched(4'd1, 1, 10, 3, 9, 8);
        chk_eq(c_dsack, 2'b01, "16-bit");  chk_eq(e_ciin[1], 1'b0, "CIIN");
        ctx = "IDE1 CS1 base write"; wr(32'h4000_0100, SZ_W, 8'h00); sched(4'd2, 1, 9, 3, 8, 8);
        ctx = "IDE2 CS0 fast read";  rd(32'h4000_0A00, SZ_W);       sched(4'd3, 1, 5, 2, 4, 3);
        ctx = "IDE2 CS1 fast write"; wr(32'h4000_0B00, SZ_W, 8'h00); sched(4'd4, 1, 5, 2, 4, 4);
        ctx = "IDE2 CS1 base read";  rd(32'h4000_0300, SZ_W);       sched(4'd4, 1, 10, 3, 9, 8);
        ctx = "old PTC window";      rd(32'h4000_0400, SZ_W);       sched(4'd0, 0, 0, 0, 0, 1);
        chk_eq(c_dsack, 2'b01, "16-bit");

        ctx = "IORDY, short pulse";
        iordy_low = 30.0;  rd(32'h4000_0000, SZ_W);  sched(4'd1, 1, 10, 3, 9, 8);
        for (i = 1; i <= 6; i = i + 1) begin
            ctx = "IORDY hold";
            iordy_low = 150.0 * i;
            n = iordy_ack(iordy_low) - 8;                    // clocks the schedule stops
            rd(32'h4000_0100, SZ_W);  sched(4'd2, 1, 10 + n, 3, 9 + n, 8 + n);
            chk(n > 0, "IORDY held the cycle");
            iordy_low = 150.0 * i;
            wr(32'h4000_0200, SZ_W, 8'h00);  sched(4'd3, 1, 9 + n, 3, 8 + n, 8 + n);
        end
        iordy_low = 0.0;
        ctx = "IORDY ignored, fast window";
        iordy_low = 400.0;  rd(32'h4000_0800, SZ_W);  sched(4'd1, 1, 5, 2, 4, 3);  iordy_low = 0.0;
        repeat (12) @(posedge clk);

        // ---- expansion cards
        for (i = 0; i < 4; i = i + 1) begin
            card_delay = (i == 0) ? 5.0 : (i == 1) ? 60.0 : (i == 2) ? 100.0 : 300.0;
            ctx = "card, 8-bit";        rd(32'h3100_0000, SZ_B);  ends(card_ack(card_delay) - 1, 2'b10);
            chk_eq(e_ciin[1], 1'b0, "CIIN");
            ctx = "card I/O window";    wr(32'h3000_0500, SZ_B, 8'h00);  ends(card_ack(card_delay) - 1, 2'b10);
            ctx = "card, 16-bit";       rd(32'h4100_0000, SZ_W);  ends(card_ack(card_delay) - 1, 2'b01);
            chk_eq(e_ciin[1], 1'b0, "CIIN");
            ctx = "card, 32-bit (VMEM)"; wr(32'h5000_0000, SZ_L, 8'h00); ends(card_ack(card_delay) - 1, 2'b00);
            ctx = "card, 32-bit";       rd(32'h5FFF_FFFC, SZ_L);  ends(card_ack(card_delay) - 1, 2'b00);
            chk_eq(e_ciin[1], 1'b0, "CIIN");
            for (j = 1; j <= c_edges; j = j + 1) chk_eq(e_code[j], 4'd0, "no device code");
        end
        card_delay = -1.0;

        // ---- DRAM, starting just after a refresh so that none interferes
        ctx = "DRAM";
        wait (dut.u_dram.ref_t == 3'd4);
        @(posedge clk); @(posedge clk);
        rd(32'h8000_0000, SZ_L);
        ends(1, 2'b00);
        chk_eq(e_ras[1], 8'hFE, "RAS at F1");   chk_eq(e_ras[2], 8'hFE, "RAS at F2");
        chk_eq(e_ras[3], 8'hFF, "RAS high at F3");
        chk_eq(e_cas[1], 4'hF, "CAS at F1");    chk_eq(e_cas[2], 4'h0, "CAS at F2 (read: all)");
        chk_eq(e_cas[3], 4'h0, "CAS at F3");    chk_eq(e_cas[4], 4'hF, "CAS after the cycle");
        chk_eq(e_mux[1], 1'b0, "MUX row at F1"); chk_eq(e_mux[2], 1'b1, "MUX column at F2");
        chk_eq(e_boe[1], 1'b0, "245s on at F1"); chk_eq(e_boe[3], 1'b0, "245s on at F3");
        chk_eq(e_boe[4], 1'b1, "245s off after AS");
        for (j = 1; j <= 4; j = j + 1) chk_eq(e_we[j], 1'b1, "WE high on a read");
        chk_eq(e_ciin[1], 1'b1, "no CIIN");
        for (i = 0; i < 8; i = i + 1) begin
            rd(32'h8000_0000 + i * 32'h0400_0000, SZ_L);
            ends(1, 2'b00);
            exp_be = ~(8'd1 << i);                        // (an 8-bit copy: ~ inside chk_eq would be 32 bits wide)
            chk_eq(e_ras[1], exp_be, "RAS for the rank");
        end
        rd(32'hA000_0000, SZ_L);
        ends(1, 2'b00);
        chk_eq(e_ras[1], 8'hFF, "no RAS above 0x9FFF_FFFF"); chk_eq(e_cas[2], 4'h0, "CAS anyway");
        wr(32'h8400_0003, SZ_B, 8'h12);
        ends(1, 2'b00);
        chk_eq(e_ras[1], 8'hFD, "RAS for rank 2A"); chk_eq(e_we[1], 1'b0, "WE low at F1");
        chk_eq(e_cas[1], 4'hF, "CAS at F1");        chk_eq(e_cas[2], 4'b1110, "CAS0 only (D7-D0)");
        wr(32'h8000_0002, SZ_W, 8'h00);
        chk_eq(e_cas[2], 4'b1100, "CAS1:0 (D15-D0)");
        wr(32'h8000_0000, SZ_L, 8'h00);
        chk_eq(e_cas[2], 4'b0000, "all CAS on a long write");
        wr(32'h8000_0000, SZ_B, 8'h00);
        chk_eq(e_cas[2], 4'b0111, "CAS3 only (D31-D24)");

        // ---- refresh
        ctx = "refresh, idle";
        r0 = n_ref;
        repeat (4 * REFRESH_CLKS + 10) @(posedge clk);
        chk(n_ref - r0 >= 4, "refreshes while idle");
        for (i = r0 + 1; i < n_ref && i < 256; i = i + 1)
            chk_eq((ref_times[i] - ref_times[i - 1]) / TCLK, REFRESH_CLKS, "idle refresh interval, clocks");

        ctx = "refresh, DRAM back to back";
        r0 = n_ref; ws_max = 0; max_req_age = 0;
        for (i = 0; i < 3 * REFRESH_CLKS / 4; i = i + 1) begin
            cycle(32'h8000_0000 + (i % 8) * 32'h0400_0000 + (i * 4), FC_SD, SZ_L, i[0], 8'h00, 1'b0);
            chk(!c_berr && c_dsack == 2'b00, "DRAM cycle ended by DSACK");
            if (c_ws > ws_max) ws_max = c_ws;
        end
        chk(n_ref - r0 >= 2, "refreshes between DRAM cycles");
        chk(ws_max > 1, "an access waited for a refresh");
        chk(ws_max <= 6, "and not for long");
        chk(max_req_age <= REFRESH_LATENCY, "refresh request served in time");

        ctx = "refresh during long cycles";
        r0 = n_ref; max_req_age = 0;
        for (i = 0; i < 2 * REFRESH_CLKS / TIMEOUT + 1; i = i + 1) cycle(32'h6000_0000, FC_SD, SZ_L, 1'b1, 8'h00, 1'b0);
        chk(n_ref - r0 >= 2, "refreshes during bus-error cycles");
        chk(max_req_age <= 2, "refresh starts during a non-DRAM cycle");

        ctx = "refresh during reset";
        r0 = n_ref;
        sys_reset_n = 1'b0;
        repeat (2 * REFRESH_CLKS + 5) @(posedge clk);
        chk(n_ref - r0 >= 2, "refreshes during reset");
        sys_reset_n = 1'b1;
        repeat (4) @(posedge clk);

        ctx = "access during a refresh";
        wait (dut.u_dram.ref_t == 3'd1);
        rd(32'h8000_0000, SZ_L);
        chk(!c_berr && c_ws > 1, "the access waited");
        rd(32'h8000_0004, SZ_L);
        ends(1, 2'b00);

        // ---- reset clears the registers
        ctx = "reset";
        wr(32'h3000_0200, SZ_B, 8'h40);  wr(32'h3000_0202, SZ_B, 8'h01);
        sys_reset_n = 1'b0;  repeat (6) @(posedge clk);  sys_reset_n = 1'b1;  repeat (4) @(posedge clk);
        rd(32'h3000_0200, SZ_B);  chk_eq(c_rd, 8'h00, "masks cleared");
        rd(32'h3000_0202, SZ_B);  chk_eq(c_rd, 8'h00, "power-off bit cleared");

        // ---- the DSACK hand-off: driven negated for half a clock after AS, then released
        ctx = "DSACK hand-off";
        cycle(32'h1000_0000, FC_SD, SZ_L, 1'b1, 8'h00, 1'b0); // returns just after AS negates
        @(posedge clk); #1;
        chk_eq(dsack_oe_n, 1'b0, "still driven at the rising edge after AS");
        chk_eq(dsack_d, 2'b11, "driven negated");
        @(negedge clk); #1;
        chk_eq(dsack_oe_n, 1'b1, "released half a clock later");

        repeat (4) @(posedge clk);
        if (errors == 0) $display("TB PASS: %0d checks", checks);
        else             $display("TB FAIL: %0d of %0d checks failed", errors, checks);
        $finish;
    end

    initial begin
        #50_000_000;
        $display("TB FAIL: timeout");
        $finish;
    end

endmodule
