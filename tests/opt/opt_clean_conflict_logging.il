module \top
  wire width 2 input 1 \in
  wire output 2 \used_out
  wire output 3 \consumer_out
  wire \used_alias
  wire \used_wire
  attribute \keep 1
  wire \kept_alias
  wire \kept_wire
  wire \live_alias
  wire \live_wire
  wire \unused_alias
  wire \unused_wire
  wire \opaque_alias
  wire \opaque_wire

  cell $not \duplicate_driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 2
    parameter \Y_WIDTH 2
    connect \A \in
    connect \Y { \used_wire \used_wire }
  end
  cell $not \kept_driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [0]
    connect \Y \kept_wire
  end
  cell $not \live_input_driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [0]
    connect \Y \live_wire
  end
  cell $not \unused_driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [0]
    connect \Y \unused_wire
  end
  cell $not \opaque_driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in [0]
    connect \Y \opaque_wire
  end
  cell $not \live_consumer
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \live_alias
    connect \Y \consumer_out
  end
  attribute \keep 1
  cell \opaque \opaque_keeper
    connect \P \opaque_alias
  end

  connect \used_out \used_alias
  connect \used_alias \used_wire
  connect \used_wire 1'0
  connect \kept_alias \kept_wire
  connect \kept_wire 1'0
  connect \live_alias \live_wire
  connect \live_wire 1'0
  connect \unused_alias \unused_wire
  connect \unused_wire 1'0
  connect \opaque_alias \opaque_wire
  connect \opaque_wire 1'0
end

module \no_conflicts
  wire input 1 \in
  wire output 2 \out
  wire \alias
  cell $not \driver
    parameter \A_SIGNED 0
    parameter \A_WIDTH 1
    parameter \Y_WIDTH 1
    connect \A \in
    connect \Y \alias
  end
  connect \out \alias
end
