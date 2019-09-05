/*
in - метка таходатчика
*/
module rpm(in, clk, out);
input in,clk;
output [15:0] out;
reg[15:0] out;
reg [15:0] cnt;

reg prev_signal;
always @(posedge clk)
 prev_signal <= in;

wire front_edge;
assign front_edge = ~prev_signal & in;

always @(posedge clk) begin
	if(front_edge) begin
		out<=cnt;
		cnt<=16'd0;
	end else begin
		cnt<=cnt+~&cnt;//(&cnt)?cnt:cnt+1'b1;
	end
end

endmodule
