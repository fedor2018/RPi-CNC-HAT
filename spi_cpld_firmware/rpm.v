module rpm(in, clk, out);
input in,clk;
output [15:0] out;
reg[15:0] out;

reg [2:0] SCKr;
always @(posedge clk) SCKr <= {SCKr[1:0], in};
wire SCK_risingedge = (SCKr[2:1]==2'b01);  // now we can detect SCK rising edges
//wire SCK_fallingedge = (SCKr[2:1]==2'b10);  // and falling edges

always @(posedge clk) begin
	if(SCK_risingedge) begin
		out<=16'd0;
	end
end

endmodule
