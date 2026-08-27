`timescale 1ns/1ps

module size_checker_backpressure_tb;
  reg clk = 1'b0;
  reg rst_n = 1'b0;

  reg [511:0] s_axis_tdata = '0;
  reg [63:0] s_axis_tkeep = '0;
  reg s_axis_tlast = 1'b0;
  reg s_axis_tvalid = 1'b0;
  reg s_axis_tdest = 1'b0;
  reg s_axis_tuser = 1'b0;
  wire s_axis_tready;

  wire [511:0] m_axis_tdata;
  wire [63:0] m_axis_tkeep;
  wire m_axis_tlast;
  wire m_axis_tvalid;
  wire m_axis_tdest;
  wire m_axis_tuser;
  reg m_axis_tready = 1'b0;

  reg [579:0] stalled_payload;
  localparam [63:0] PADDED_KEEP = {4'b0000, {60{1'b1}}};

  always #5 clk = ~clk;

  size_checker dut (
    .CLK(clk),
    .RST_N(rst_n),
    .S_AXIS_TDATA(s_axis_tdata),
    .S_AXIS_TKEEP(s_axis_tkeep),
    .S_AXIS_TLAST(s_axis_tlast),
    .S_AXIS_TVALID(s_axis_tvalid),
    .S_AXIS_TDEST(s_axis_tdest),
    .S_AXIS_TUSER(s_axis_tuser),
    .S_AXIS_TREADY(s_axis_tready),
    .M_AXIS_TDATA(m_axis_tdata),
    .M_AXIS_TKEEP(m_axis_tkeep),
    .M_AXIS_TLAST(m_axis_tlast),
    .M_AXIS_TVALID(m_axis_tvalid),
    .M_AXIS_TDEST(m_axis_tdest),
    .M_AXIS_TUSER(m_axis_tuser),
    .M_AXIS_TREADY(m_axis_tready)
  );

  function automatic [579:0] output_payload;
    output_payload = {
      m_axis_tdata,
      m_axis_tkeep,
      m_axis_tlast,
      m_axis_tvalid,
      m_axis_tdest,
      m_axis_tuser
    };
  endfunction

  task automatic check_stable;
    begin
      @(posedge clk);
      #1;
      if (output_payload() !== stalled_payload)
        $fatal(1, "AXI output changed while TVALID was stalled");
    end
  endtask

  initial begin
    $dumpfile("size_checker_backpressure.vcd");
    $dumpvars(0, size_checker_backpressure_tb);

    repeat (2) @(posedge clk);
    @(negedge clk);
    rst_n = 1'b1;

    // A short single-beat frame must be padded before READY is asserted.
    s_axis_tdata = 512'h0123456789abcdef;
    s_axis_tkeep = {{44{1'b0}}, {20{1'b1}}};
    s_axis_tlast = 1'b1;
    s_axis_tvalid = 1'b1;
    s_axis_tdest = 1'b1;
    s_axis_tuser = 1'b1;
    m_axis_tready = 1'b0;

    @(posedge clk);
    #1;
    if (!m_axis_tvalid || m_axis_tkeep !== PADDED_KEEP)
      $fatal(1, "short single-beat frame was not padded while stalled");
    stalled_payload = output_payload();
    repeat (3) check_stable();

    @(negedge clk);
    m_axis_tready = 1'b1;
    @(posedge clk);
    #1;
    if (m_axis_tkeep !== PADDED_KEEP)
      $fatal(1, "padding changed on the acceptance cycle");

    // Only a single-beat frame is padded. A short final beat of a multi-beat
    // frame must keep the source mask stable during backpressure.
    @(negedge clk);
    s_axis_tdata = {512{1'b1}};
    s_axis_tkeep = {64{1'b1}};
    s_axis_tlast = 1'b0;
    @(posedge clk);

    @(negedge clk);
    s_axis_tdata = 512'hfedcba9876543210;
    s_axis_tkeep = {{54{1'b0}}, {10{1'b1}}};
    s_axis_tlast = 1'b1;
    m_axis_tready = 1'b0;
    @(posedge clk);
    #1;
    if (m_axis_tkeep !== {{54{1'b0}}, {10{1'b1}}})
      $fatal(1, "multi-beat final mask was incorrectly padded");
    stalled_payload = output_payload();
    repeat (3) check_stable();

    @(negedge clk);
    m_axis_tready = 1'b1;
    @(posedge clk);
    @(negedge clk);
    s_axis_tvalid = 1'b0;
    s_axis_tlast = 1'b0;

    $display("PASS: size_checker output is stable under backpressure");
    $finish;
  end
endmodule
