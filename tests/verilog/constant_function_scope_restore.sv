module top(input [7:0] a, output [7:0] y, output [7:0] constants, output ok);
  localparam integer OFFSET = 3;
  localparam integer i = 11;

  function automatic [7:0] nested(input integer count);
    integer i;
    begin
      nested = 0;
      for (i = 0; i < count; i = i + 1)
        nested = nested + i + OFFSET;
    end
  endfunction

  function automatic [7:0] layered(input integer count);
    integer i;
    begin
      layered = 0;
      for (i = 0; i < count; i = i + 1) begin: outer
        integer j;
        for (j = 0; j < 2; j = j + 1) begin: inner
          localparam integer EXTRA = 2;
          layered = layered + nested(j + 1) + EXTRA;
        end
      end
    end
  endfunction

  function automatic integer recurse(input integer depth);
    begin
      if (depth == 0)
        recurse = OFFSET;
      else
        recurse = recurse(depth - 1) + 1;
    end
  endfunction

  function automatic integer loops(input integer count);
    begin
      loops = 0;
      repeat (count)
        loops = loops + 1;
      case (count)
        0: loops = loops + 11;
        default: loops = loops + 3;
      endcase
    end
  endfunction

  // Constant arguments do not make this call constant. Its speculative
  // evaluation must restore the scope before falling back to RTL inlining.
  function automatic [7:0] dynamic(input [7:0] value);
    begin
      dynamic = a + value;
    end
  endfunction

  localparam [7:0] FIRST = layered(3);
  localparam [7:0] SECOND = layered(1);
  assign y = dynamic(8'd7);
  assign constants = layered(1) + layered(3) + loops(0) + recurse(3) + nested(2);

  wire [3:0] generated_ok;
  for (genvar g = 0; g < 4; g = g + 1) begin: generated
    localparam integer WIDTH = nested(g + 1);
    wire [WIDTH:0] value = 0;
    assign generated_ok[g] = WIDTH == (g * (g + 1)) / 2 + OFFSET * (g + 1)
                           && $bits(value) == WIDTH + 1;
  end

  assign ok = FIRST == 42 && SECOND == 14 && i == 11 && OFFSET == 3
           && constants == 8'd80 && y == a + 8'd7
           && recurse(3) == 6 && loops(3) == 6 && (&generated_ok);
endmodule
