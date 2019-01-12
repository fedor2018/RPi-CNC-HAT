
module pwm(in,clk,out,inv);
input[7:0] in;
input clk;
input inv;
output out;

reg[7:0] counter;
reg out;

always@(posedge clk)
begin
if(counter<=in) 
    out <=inv?1'b0:1'b1;
else 
    out <=inv?1'b1:1'b0;
counter<=counter+8'b1;
end

endmodule
