module RCServo(clk, RxD, RCServo_pulse);
input clk, RxD;
output RCServo_pulse;

////////////////////////////////////////////////////////////////////////////
// use the serial port to control the servo

wire RxD_data_ready;
wire [7:0] RxD_data;
async_receiver deserialer(.clk(clk), .RxD(RxD), .RxD_data_ready(RxD_data_ready), .RxD_data(RxD_data));  

reg [7:0] RxD_data_reg;
always @(posedge clk) if(RxD_data_ready) RxD_data_reg <= RxD_data;

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
always @(posedge clk) if(PulseCount==0) RCServo_position <= RxD_data_reg;

reg RCServo_pulse;
always @(posedge clk) RCServo_pulse <= (PulseCount < {4'b0001, RCServo_position});

endmodule