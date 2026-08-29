package scope_pkg;
  parameter integer WIDTH = 4;

  typedef enum logic [1:0] {
    IDLE = 0,
    RUN  = 1,
    DONE = 2
  } state_t;

endpackage

interface scope_if;
  localparam integer WIDTH = 2;
  logic [scope_pkg::WIDTH-1:0] data;
  logic [WIDTH-1:0] local_data;
  scope_pkg::state_t state;
endinterface

module shadow_scope(output logic ok);
  localparam integer WIDTH = 1;
  localparam integer BUS_WIDTH = 1;
  typedef enum logic {IDLE = 1, RUN = 0} state_t;

  assign ok = WIDTH == 1 && BUS_WIDTH == 1 && RUN == 0;
endmodule

import scope_pkg::*;

module top(
  input  logic [3:0] in,
  output logic [7:0] out,
  output logic ok
);
  function automatic integer doubled;
    input integer value;
    doubled = value * 2;
  endfunction

  localparam integer OUT_WIDTH = doubled(WIDTH);
  logic [OUT_WIDTH-1:0] extended;
  logic shadow_ok;

  shadow_scope shadow(shadow_ok);
  assign extended = {{WIDTH{1'b0}}, in};
  assign out = extended + RUN;
  assign ok = shadow_ok && out == {4'b0000, in} + 8'd1;
endmodule
