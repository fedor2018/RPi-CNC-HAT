
module pwm(in,clk,out);
input[7:0] in;
input clk;

output out;

reg[7:0] counter;
reg out;

always@(posedge clk)
begin
if(counter<=in) 
    out <=1'b1;
else 
    out <=1'b0;
counter<=counter+8'b1;
end

endmodule
