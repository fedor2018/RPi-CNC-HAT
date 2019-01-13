module rcservo(in, clk, out);
input [7:0] in;
input clk;
output out;

////////////////////////////////////////////////////////////////////////////
// divide the clock
parameter ClkDiv = 98;  // 25000000/1000/256 = 97.56

reg [6:0] ClkCount;
reg ClkTick;
always @(posedge clk) ClkTick <= (ClkCount==ClkDiv-2);
always @(posedge clk) if(ClkTick) ClkCount <= 0; else ClkCount <= ClkCount + 1;

////////////////////////////////////////////////////////////////////////////
reg [11:0] PulseCount;
always @(posedge clk) if(ClkTick) PulseCount <= PulseCount + 1;

// make sure the RCServo_position is stable while the pulse is generated
reg [7:0] RCServo_position;
always @(posedge clk) if(PulseCount==0) RCServo_position <= in;

reg out;
always @(posedge clk) out <= (PulseCount < {4'b0001, RCServo_position});

endmodule
