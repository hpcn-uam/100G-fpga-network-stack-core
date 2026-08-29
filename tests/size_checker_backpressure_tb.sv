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
  integer beat_id = 0;

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

  function automatic [63:0] keep_mask(input integer byte_count);
    begin
      if (byte_count == 64)
        keep_mask = {64{1'b1}};
      else
        keep_mask = (64'b1 << byte_count) - 1'b1;
    end
  endfunction

  task automatic send_beat(
    input integer valid_bytes,
    input reg last,
    input integer stall_cycles,
    input [63:0] expected_keep
  );
    integer cycle;
    begin
      @(negedge clk);
      s_axis_tdata = {448'b0, beat_id[31:0], valid_bytes[15:0], stall_cycles[15:0]};
      s_axis_tkeep = keep_mask(valid_bytes);
      s_axis_tlast = last;
      s_axis_tvalid = 1'b1;
      m_axis_tready = (stall_cycles == 0);
      #1;

      if (!m_axis_tvalid || m_axis_tkeep !== expected_keep)
        $fatal(1, "unexpected output mask for %0d-byte beat", valid_bytes);
      if (m_axis_tdata !== s_axis_tdata || m_axis_tlast !== last)
        $fatal(1, "output payload does not match the input beat");
      stalled_payload = output_payload();

      for (cycle = 0; cycle < stall_cycles; cycle = cycle + 1) begin
        @(posedge clk);
        #1;
        if (output_payload() !== stalled_payload)
          $fatal(1, "AXI output changed during stall cycle %0d", cycle);
      end

      if (stall_cycles != 0) begin
        @(negedge clk);
        m_axis_tready = 1'b1;
        #1;
        if (output_payload() !== stalled_payload)
          $fatal(1, "AXI output changed when TREADY was released");
      end

      @(posedge clk);
      @(negedge clk);
      s_axis_tvalid = 1'b0;
      s_axis_tlast = 1'b0;
      beat_id = beat_id + 1;
    end
  endtask

  task automatic send_packet(input integer byte_count, input integer stall_cycles);
    integer remaining;
    integer beat_bytes;
    begin
      remaining = byte_count;
      while (remaining > 64) begin
        send_beat(64, 1'b0, 0, keep_mask(64));
        remaining = remaining - 64;
      end

      beat_bytes = remaining;
      if (byte_count < 60)
        send_beat(beat_bytes, 1'b1, stall_cycles, keep_mask(60));
      else
        send_beat(beat_bytes, 1'b1, stall_cycles, keep_mask(beat_bytes));

      @(posedge clk);
    end
  endtask

  initial begin
    $dumpfile("size_checker_backpressure.vcd");
    $dumpvars(0, size_checker_backpressure_tb);

    repeat (2) @(posedge clk);
    @(negedge clk);
    rst_n = 1'b1;

    s_axis_tdest = 1'b1;
    s_axis_tuser = 1'b1;

    send_packet(60, 0);
    send_packet(62, 0);
    send_packet(50, 0);
    send_packet(133, 0);

    send_packet(60, 1);
    send_packet(62, 1);
    send_packet(50, 1);
    send_packet(133, 1);

    send_packet(60, 3);
    send_packet(62, 3);
    send_packet(50, 3);
    send_packet(133, 3);

    $display("PASS: all 12 size and backpressure cases are stable");
    $finish;
  end
endmodule
