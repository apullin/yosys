module \top
  wire width 4 input 1 \in
  wire width 4 output 2 \x_out
  wire width 4 \x_alias
  attribute \keep 1
  wire width 4 \kept_x
  wire width 4 \unused_x
  wire width 4 \unused_plain

  attribute \init 4'xxxx
  wire width 4 output 3 \all_x_port
  attribute \init 4'xxxx
  wire width 4 \all_x_unused
  attribute \init 4'1010
  wire width 4 output 4 \defined_port
  attribute \init 2'10
  wire width 4 output 5 \short_port
  attribute \init 4'1011
  wire width 2 output 6 \long_port
  wire width 0 output 7 \zero_port
  wire width 0 \zero_unused
  attribute \keep 1
  wire width 0 \zero_kept

  wire width 4 output 8 \logic_out
  cell $not \driver0
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [0]
    connect \Y \logic_out [0]
  end
  cell $not \driver1
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [1]
    connect \Y \logic_out [1]
  end
  cell $not \driver2
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [2]
    connect \Y \logic_out [2]
  end
  cell $not \driver3
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [3]
    connect \Y \logic_out [3]
  end

  connect \x_out \x_alias
  connect \x_alias 4'xxxx
  connect \kept_x 4'xxxx
  connect \unused_x 4'xxxx
end
