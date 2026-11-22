// tb_intctrl.v - self-checking testbench for intctrl.v; check-hdl.ps1 runs it in Vivado's xsim.
// It prints each failure and ends with "TB PASS: n checks" or "TB FAIL: e of n checks failed".
//
// The source table here is copied from fpga/Roscoe_FPGA_Redesign.md section 11, not taken from
// intctrl.v, so it also checks the module's default levels and letters. Directed tests cover each
// source alone, priorities across and within levels, spurious acknowledges, the edge-latch rules, the
// vector hold, the power-request filter and reset. A random sequence then runs against a model.
//
// Inputs change just after falling clk edges and the DUT samples on rising ones.

`timescale 1ns / 1ps

module tb_intctrl;

    localparam integer CLK_HZ   = 16000000;          // 16 MHz, as CPU_CLK runs today
    localparam integer PWR_US   = 1;                 // a short power filter for simulation...
    localparam integer PWR_CLKS = CLK_HZ / 1000000 * PWR_US;   // ...of 16 clocks
    localparam integer RANDOM_STEPS = 4000;

    // ---- the agreed table ---------------------------------------------------------------------------
    localparam integer NSRC = 11;
    localparam integer T1 = 0, T2 = 1, IDE1 = 2, IDE2 = 3, UA = 4, UB = 5, S1 = 6, S2 = 7, S3 = 8,
                       RTC = 9, PWR = 10;
    localparam [15:0] ALL = 16'h6DBB;                // every mask bit that has a source
    reg [3:0]  s_bit  [0:NSRC-1];
    reg [2:0]  s_lvl  [0:NSRC-1];
    reg [7:0]  s_vec  [0:NSRC-1];
    reg        s_edge [0:NSRC-1];
    reg [63:0] s_name [0:NSRC-1];
    initial begin
        s_bit[T1]   = 14; s_lvl[T1]   = 6; s_vec[T1]   = 8'hE1; s_edge[T1]   = 1; s_name[T1]   = "timer 1";
        s_bit[T2]   = 13; s_lvl[T2]   = 6; s_vec[T2]   = 8'hE2; s_edge[T2]   = 1; s_name[T2]   = "timer 2";
        s_bit[IDE1] = 11; s_lvl[IDE1] = 5; s_vec[IDE1] = 8'hD2; s_edge[IDE1] = 0; s_name[IDE1] = "IDE1";
        s_bit[IDE2] = 10; s_lvl[IDE2] = 5; s_vec[IDE2] = 8'hD4; s_edge[IDE2] = 0; s_name[IDE2] = "IDE2";
        s_bit[UA]   =  8; s_lvl[UA]   = 4; s_vec[UA]   = 8'hC1; s_edge[UA]   = 0; s_name[UA]   = "UART A";
        s_bit[UB]   =  7; s_lvl[UB]   = 4; s_vec[UB]   = 8'hC2; s_edge[UB]   = 0; s_name[UB]   = "UART B";
        s_bit[S1]   =  5; s_lvl[S1]   = 3; s_vec[S1]   = 8'hB1; s_edge[S1]   = 0; s_name[S1]   = "slot 1";
        s_bit[S2]   =  4; s_lvl[S2]   = 3; s_vec[S2]   = 8'hB2; s_edge[S2]   = 0; s_name[S2]   = "slot 2";
        s_bit[S3]   =  3; s_lvl[S3]   = 3; s_vec[S3]   = 8'hB4; s_edge[S3]   = 0; s_name[S3]   = "slot 3";
        s_bit[RTC]  =  1; s_lvl[RTC]  = 1; s_vec[RTC]  = 8'h91; s_edge[RTC]  = 0; s_name[RTC]  = "RTC";
        s_bit[PWR]  =  0; s_lvl[PWR]  = 1; s_vec[PWR]  = 8'h92; s_edge[PWR]  = 1; s_name[PWR]  = "power";
    end

    // ---- the DUT ----------------------------------------------------------------------------------------
    reg        clk = 1'b0;
    always #31.25 clk = ~clk;
    reg        rst = 1'b1;
    reg  [1:0] irq_ide = 2'b00, irq_uart = 2'b00;
    reg  [2:0] irq_exp_n = 3'b111;
    reg        irq_rtc_n = 1'b1, pwr_req = 1'b0, pwr_drive = 1'b0;
    reg  [1:0] tmr_irq = 2'b00;
    reg        reg_wr = 1'b0;
    reg  [2:0] reg_addr = 3'd0;
    reg  [7:0] reg_wdata = 8'h00;
    reg        iack_stb = 1'b0;
    reg  [2:0] iack_level = 3'd0;
    wire [7:0] reg_rdata, iack_vector, iack_vec_nxt;
    wire [2:0] ipl_n;
    wire [2:0] ipl_level = ~ipl_n;

    intctrl #(.CLK_HZ(CLK_HZ), .PWR_FILTER_US(PWR_US)) dut (
        .clk(clk), .rst(rst),
        .irq_ide(irq_ide), .irq_uart(irq_uart), .irq_exp_n(irq_exp_n), .irq_rtc_n(irq_rtc_n), .pwr_req(pwr_req),
        .tmr_irq(tmr_irq), .pwr_drive(pwr_drive),
        .reg_wr(reg_wr), .reg_addr(reg_addr), .reg_wdata(reg_wdata), .reg_rdata(reg_rdata),
        .iack_stb(iack_stb), .iack_level(iack_level), .iack_vector(iack_vector), .iack_vec_nxt(iack_vec_nxt),
        .ipl_n(ipl_n));

    // ---- checks ---------------------------------------------------------------------------------------
    integer errors = 0, checks = 0;
    reg [8*24-1:0] ctx = "start";

    task ipl_is(input [2:0] want);
        begin
            checks = checks + 1;
            if (ipl_level !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t,%0s: IPL level %0d, expected %0d", $time, ctx, ipl_level, want);
            end
        end
    endtask

    task vec_is(input [7:0] want);
        begin
            checks = checks + 1;
            if (iack_vector !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t,%0s: vector %h, expected %h", $time, ctx, iack_vector, want);
            end
        end
    endtask

    // ---- bus actions ------------------------------------------------------------------------------------
    task clocks(input integer n); begin repeat (n) @(negedge clk); end endtask
    task settle; clocks(4); endtask       // synchroniser (2) and IPL register (1), plus one

    task wr(input [2:0] a, input [7:0] d);
        begin
            @(negedge clk); reg_wr = 1'b1; reg_addr = a; reg_wdata = d;
            @(negedge clk); reg_wr = 1'b0;
        end
    endtask

    task rd(input [2:0] a, input [7:0] want);
        begin
            @(negedge clk); reg_addr = a; #1;
            checks = checks + 1;
            if (reg_rdata !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t,%0s: register %0d reads %h, expected %h", $time, ctx, a, reg_rdata, want);
            end
        end
    endtask

    task set_mask(input [15:0] m); begin wr(3'd0, m[15:8]); wr(3'd1, m[7:0]); end endtask
    task mask_is(input [15:0] want); begin rd(3'd0, want[15:8]); rd(3'd1, want[7:0]); end endtask
    task req_is(input [15:0] want); begin rd(3'd4, want[15:8]); rd(3'd5, want[7:0]); end endtask

    task iack(input [2:0] lvl, input [7:0] want);
        begin
            @(negedge clk); iack_stb = 1'b1; iack_level = lvl;
            @(posedge clk);                                 // the vector about to be latched, just before
            checks = checks + 1;
            if (iack_vec_nxt !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t, %0s: iack_vec_nxt at level %0d is %h, expected %h", $time, ctx, lvl, iack_vec_nxt, want);
            end
            @(negedge clk); iack_stb = 1'b0;
            checks = checks + 1;
            if (iack_vector !== want) begin
                errors = errors + 1;
                $display("FAIL at %0t,%0s: acknowledge at level %0d gives vector %h, expected %h",
                         $time, ctx, lvl, iack_vector, want);
            end
        end
    endtask

    function [7:0] spurious(input [2:0] lvl); spurious = {1'b1, lvl, 4'h0}; endfunction
    function [15:0] bit_of(input integer s); bit_of = 16'h0001 << s_bit[s]; endfunction

    // ---- sources ----------------------------------------------------------------------------------------
    // A level source's line: 1 = requesting (active-low lines are inverted here).
    task line(input integer s, input v);
        begin
            @(negedge clk);
            case (s)
                IDE1: irq_ide[0]   = v;
                IDE2: irq_ide[1]   = v;
                UA:   irq_uart[0]  = v;
                UB:   irq_uart[1]  = v;
                S1:   irq_exp_n[0] = ~v;
                S2:   irq_exp_n[1] = ~v;
                S3:   irq_exp_n[2] = ~v;
                RTC:  irq_rtc_n    = ~v;
                default: begin errors = errors + 1; $display("TB BUG: line() on source %0d", s); end
            endcase
        end
    endtask

    // pwr_req held at v for n clocks, from the next falling edge.
    task pwr_hold(input v, input integer n);
        begin
            @(negedge clk); pwr_req = v;
            repeat (n - 1) @(negedge clk);
        end
    endtask

    // An edge source: a one-clock timer pulse, or a power pulse long enough to count, then low long
    // enough for the filter to re-arm.
    task fire(input integer s);
        begin
            case (s)
                T1:  begin @(negedge clk); tmr_irq[0] = 1'b1; @(negedge clk); tmr_irq[0] = 1'b0; end
                T2:  begin @(negedge clk); tmr_irq[1] = 1'b1; @(negedge clk); tmr_irq[1] = 1'b0; end
                PWR: begin pwr_hold(1'b1, PWR_CLKS + 2); pwr_hold(1'b0, PWR_CLKS + 6); end
                default: begin errors = errors + 1; $display("TB BUG: fire() on source %0d", s); end
            endcase
        end
    endtask

    // ---- model, for the random sequence -----------------------------------------------------------------
    reg [15:0] m_mask;
    reg        m_line  [0:NSRC-1];
    reg        m_latch [0:NSRC-1];

    function m_on(input integer q);
        m_on = m_mask[s_bit[q]] && (s_edge[q] ? m_latch[q] : m_line[q]);
    endfunction

    function [2:0] m_ipl(input dummy);
        integer q;
        begin
            m_ipl = 3'd0;
            for (q = 0; q < NSRC; q = q + 1)
                if (m_on(q) && s_lvl[q] > m_ipl) m_ipl = s_lvl[q];
        end
    endfunction

    // At a level, the active source with the smallest vector wins: A before B before C before D.
    function [7:0] m_vec(input [2:0] lvl);
        integer q;
        begin
            m_vec = spurious(lvl);
            for (q = 0; q < NSRC; q = q + 1)
                if (m_on(q) && s_lvl[q] == lvl && (m_vec[3:0] == 4'h0 || s_vec[q] < m_vec)) m_vec = s_vec[q];
        end
    endfunction

    function [15:0] m_req(input dummy);
        integer q;
        begin
            m_req = 16'h0000;
            for (q = 0; q < NSRC; q = q + 1)
                if (s_edge[q] ? m_latch[q] : m_line[q]) m_req[s_bit[q]] = 1'b1;
        end
    endfunction

    task m_iack(input [2:0] lvl);
        reg [7:0] w;
        integer q;
        begin
            w = m_vec(lvl);
            iack(lvl, w);
            for (q = 0; q < NSRC; q = q + 1)
                if (s_edge[q] && s_lvl[q] == lvl && s_vec[q] == w) m_latch[q] = 1'b0;
        end
    endtask

    // ---- the tests ----------------------------------------------------------------------------------------
    integer s, l, n, k, r, seed;
    reg       v;
    reg [7:0] d;
    reg [2:0] lv;

    initial begin
        $timeformat(-9, 1, " ns", 1);
        // ---- reset
        ctx = "reset";
        clocks(4);
        ipl_is(0);
        rst = 1'b0;
        clocks(PWR_CLKS + 4);               // the power filter sees the line low, and arms
        ipl_is(0); mask_is(16'h0000); req_is(16'h0000);
        rd(3'd2, 8'h00); rd(3'd3, 8'h00); rd(3'd6, 8'h00); rd(3'd7, 8'h00);

        // ---- mask register: only bits with a source can be set
        ctx = "mask register";
        set_mask(16'hFFFF); mask_is(ALL);
        wr(3'd0, 8'h00); mask_is(ALL & 16'h00FF);
        wr(3'd1, 8'h00); mask_is(16'h0000);
        wr(3'd0, 8'h40); mask_is(16'h4000);
        wr(3'd1, 8'h22); mask_is(16'h4022);
        wr(3'd4, 8'hFF); wr(3'd5, 8'hFF); wr(3'd2, 8'hFF); wr(3'd7, 8'hFF);   // nothing else is writable
        mask_is(16'h4022); req_is(16'h0000);
        set_mask(16'h0000);

        // ---- each source on its own
        for (s = 0; s < NSRC; s = s + 1) begin
            ctx = s_name[s];
            if (s_edge[s]) begin
                fire(s); settle;                        // masked: dropped, not kept
                ipl_is(0); req_is(16'h0000);
                set_mask(bit_of(s)); settle;
                ipl_is(0); req_is(16'h0000);
                fire(s); settle;
                ipl_is(s_lvl[s]); req_is(bit_of(s));
                iack(s_lvl[s], s_vec[s]); settle;       // its acknowledge clears it
                ipl_is(0); req_is(16'h0000);
                iack(s_lvl[s], spurious(s_lvl[s]));
            end else begin
                line(s, 1'b1); settle;                  // masked: shows in REQ but not on IPL
                ipl_is(0); req_is(bit_of(s));
                set_mask(bit_of(s)); settle;
                ipl_is(s_lvl[s]);
                iack(s_lvl[s], s_vec[s]); settle;       // a level stays until the device lets go
                ipl_is(s_lvl[s]);
                iack(s_lvl[s], s_vec[s]);
                line(s, 1'b0); settle;
                ipl_is(0); req_is(16'h0000);
                iack(s_lvl[s], spurious(s_lvl[s]));
            end
            set_mask(16'h0000);
        end

        // ---- everything at once: the levels in order, and the letters within each level
        ctx = "all sources";
        set_mask(ALL);
        for (s = IDE1; s <= RTC; s = s + 1) line(s, 1'b1);
        fire(T1); fire(T2); fire(PWR); settle;
        req_is(ALL); ipl_is(6);
        iack(6, 8'hE1); settle; ipl_is(6);              // timer 2 still waits
        iack(6, 8'hE2); settle; ipl_is(5);
        iack(5, 8'hD2);                                 // IDE1 before IDE2
        line(IDE1, 1'b0); settle; ipl_is(5);
        iack(5, 8'hD4);
        line(IDE2, 1'b0); settle; ipl_is(4);
        iack(4, 8'hC1);                                 // UART A before UART B
        line(UA, 1'b0); settle; iack(4, 8'hC2);
        line(UB, 1'b0); settle; ipl_is(3);
        iack(3, 8'hB1);                                 // slot 1, then 2, then 3
        line(S1, 1'b0); settle; iack(3, 8'hB2);
        line(S3, 1'b0); settle; iack(3, 8'hB2);         // slot 2 alone
        line(S3, 1'b1); line(S2, 1'b0); settle; iack(3, 8'hB4);
        line(S1, 1'b1); settle; iack(3, 8'hB1);         // slot 1 beats slot 3
        line(S1, 1'b0); line(S3, 1'b0); settle; ipl_is(1);
        iack(1, 8'h91);                                 // the RTC before the power request
        line(RTC, 1'b0); settle; ipl_is(1);
        iack(1, 8'h92); settle; ipl_is(0);
        req_is(16'h0000);

        // ---- spurious acknowledges: 1LLL0000
        ctx = "spurious";
        for (l = 1; l < 8; l = l + 1) iack(l[2:0], spurious(l[2:0]));
        line(IDE1, 1'b1); settle;                       // a request at another level doesn't matter
        iack(2, 8'hA0); iack(6, 8'hE0); iack(7, 8'hF0); iack(4, 8'hC0); iack(5, 8'hD2);
        line(IDE1, 1'b0); settle; iack(5, 8'hD0);
        set_mask(16'h0000);

        // ---- edge latches
        ctx = "edge latches";
        set_mask(bit_of(T1));
        fire(T1); settle; ipl_is(6);
        set_mask(16'h0000); settle; ipl_is(0); req_is(16'h0000);   // clearing the mask bit drops it...
        set_mask(bit_of(T1)); settle; ipl_is(0);                   // ...and it doesn't come back
        fire(T1); settle; ipl_is(6);
        wr(3'd0, 8'h40); settle; ipl_is(6);                        // rewriting the byte keeps it
        @(negedge clk); iack_stb = 1'b1; iack_level = 3'd6; tmr_irq[0] = 1'b1;   // a new edge in the
        @(negedge clk); iack_stb = 1'b0; tmr_irq[0] = 1'b0;                      // acknowledge's clock
        vec_is(8'hE1); settle; ipl_is(6);                          // is kept
        iack(6, 8'hE1); settle; ipl_is(0);
        set_mask(bit_of(T1) | bit_of(T2));
        fire(T1); fire(T2); settle;
        iack(6, 8'hE1); settle; ipl_is(6); req_is(bit_of(T2));     // each acknowledge clears only its own
        iack(6, 8'hE2); settle; ipl_is(0); req_is(16'h0000);
        @(negedge clk); tmr_irq[0] = 1'b1; settle;                 // a timer line held high is one edge
        iack(6, 8'hE1); settle; ipl_is(0);
        clocks(10); ipl_is(0);
        @(negedge clk); tmr_irq[0] = 1'b0; settle; ipl_is(0);
        set_mask(16'h0000);

        // ---- the vector holds for the rest of the cycle
        ctx = "vector hold";
        set_mask(bit_of(IDE1) | bit_of(IDE2));
        line(IDE2, 1'b1); settle;
        iack(5, 8'hD4);
        line(IDE1, 1'b1); settle; vec_is(8'hD4);       // IDE1 arrives mid-cycle: no change
        line(IDE2, 1'b0); settle; vec_is(8'hD4);
        iack(5, 8'hD2);                                // the next acknowledge sees it
        line(IDE1, 1'b0); settle;
        set_mask(16'h0000);

        // ---- the power controller's request
        ctx = "power filter";
        set_mask(bit_of(PWR));
        pwr_hold(1'b1, PWR_CLKS - 1); pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0);    // a clock short: ignored
        pwr_hold(1'b1, PWR_CLKS);     pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(1);    // just long enough
        iack(1, 8'h92); settle; ipl_is(0);
        pwr_hold(1'b1, PWR_CLKS + 6); ipl_is(1);                                   // held high: one request
        iack(1, 8'h92); settle; ipl_is(0);
        pwr_hold(1'b1, 4 * PWR_CLKS); ipl_is(0);
        pwr_hold(1'b0, PWR_CLKS - 1);                                              // low a clock short of
        pwr_hold(1'b1, 2 * PWR_CLKS); ipl_is(0);                                   // the filter: not re-armed
        pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0);
        pwr_hold(1'b1, PWR_CLKS); pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(1);         // re-armed
        iack(1, 8'h92); settle; ipl_is(0);
        // The CPU's own drive: code 12, IOCTRL drives the line, code 13, and the line falls slowly,
        // chattering on the way down.
        @(negedge clk); pwr_drive = 1'b1;
        pwr_hold(1'b1, 5 * PWR_CLKS);
        @(negedge clk); pwr_drive = 1'b0;
        pwr_hold(1'b1, 3); pwr_hold(1'b0, 2); pwr_hold(1'b1, 3); pwr_hold(1'b0, 5); pwr_hold(1'b1, 2);
        pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0); req_is(16'h0000);
        // Released while the line stays high for longer than the filter: still nothing.
        @(negedge clk); pwr_drive = 1'b1;
        pwr_hold(1'b1, 2 * PWR_CLKS);
        @(negedge clk); pwr_drive = 1'b0;
        pwr_hold(1'b1, 3 * PWR_CLKS); pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0);
        // A pulse while the CPU drives is ignored too.
        @(negedge clk); pwr_drive = 1'b1;
        pwr_hold(1'b1, 2 * PWR_CLKS); pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0);
        @(negedge clk); pwr_drive = 1'b0;
        pwr_hold(1'b0, PWR_CLKS + 6);
        fire(PWR); ipl_is(1);                                                      // then the AVR's pulse counts
        iack(1, 8'h92); settle; ipl_is(0);
        // Masked: the pulse is dropped, and setting the mask bit later finds nothing.
        set_mask(16'h0000);
        fire(PWR); ipl_is(0); req_is(16'h0000);
        set_mask(bit_of(PWR)); settle; ipl_is(0);
        // After a reset with the line high, nothing counts until the line has been low for the filter.
        @(negedge clk); pwr_req = 1'b1; rst = 1'b1;
        clocks(3); rst = 1'b0;
        set_mask(bit_of(PWR)); clocks(3 * PWR_CLKS); ipl_is(0);
        pwr_hold(1'b0, PWR_CLKS + 6); ipl_is(0);
        fire(PWR); ipl_is(1);
        iack(1, 8'h92); settle; ipl_is(0);
        set_mask(16'h0000);

        // ---- reset clears the masks and the latches
        ctx = "reset again";
        set_mask(ALL); fire(T1); line(IDE1, 1'b1); settle; ipl_is(6);
        @(negedge clk); rst = 1'b1;
        clocks(2); ipl_is(0);
        rst = 1'b0; settle;
        ipl_is(0); mask_is(16'h0000); req_is(bit_of(IDE1));    // the IDE1 line still shows; timer 1 is gone
        set_mask(ALL); settle; ipl_is(5);
        line(IDE1, 1'b0); settle; ipl_is(0);
        set_mask(16'h0000);

        // ---- random sequence against the model
        ctx = "random";
        @(negedge clk); rst = 1'b1;
        clocks(2); rst = 1'b0;
        clocks(PWR_CLKS + 4);
        m_mask = 16'h0000;
        for (k = 0; k < NSRC; k = k + 1) begin m_line[k] = 1'b0; m_latch[k] = 1'b0; end
        seed = 2026;
        for (n = 0; n < RANDOM_STEPS; n = n + 1) begin
            r = {$random(seed)} % 10;
            case (r)
                0, 1, 2: begin                                 // a level line changes
                    s = IDE1 + {$random(seed)} % 8;
                    v = $random(seed);
                    line(s, v); m_line[s] = v;
                end
                3: begin                                       // a timer fires
                    s = {$random(seed)} % 2;
                    fire(s);
                    if (m_mask[s_bit[s]]) m_latch[s] = 1'b1;
                end
                4: begin                                       // a mask byte is written
                    d = $random(seed);
                    if ($random(seed) & 1) begin wr(3'd0, d); m_mask[15:8] = d & ALL[15:8]; end
                    else                   begin wr(3'd1, d); m_mask[7:0]  = d & ALL[7:0];  end
                    for (k = 0; k < NSRC; k = k + 1) if (s_edge[k] && !m_mask[s_bit[k]]) m_latch[k] = 1'b0;
                end
                5, 6: begin                                    // the CPU acknowledges what IPL shows
                    settle; lv = m_ipl(0); ipl_is(lv);
                    if (lv == 3'd0) lv = 1 + {$random(seed)} % 7;
                    m_iack(lv);
                end
                7: begin                                       // an acknowledge at any level
                    settle; lv = 1 + {$random(seed)} % 7;
                    m_iack(lv);
                end
                8: begin                                       // read everything back
                    settle; ipl_is(m_ipl(0)); req_is(m_req(0)); mask_is(m_mask);
                end
                default: clocks(1 + {$random(seed)} % 4);
            endcase
        end
        settle; ipl_is(m_ipl(0)); req_is(m_req(0)); mask_is(m_mask);

        if (errors == 0) $display("TB PASS: %0d checks", checks);
        else             $display("TB FAIL: %0d of %0d checks failed", errors, checks);
        $finish;
    end

    initial begin
        #20_000_000;
        $display("TB FAIL: timeout");
        $finish;
    end

endmodule
