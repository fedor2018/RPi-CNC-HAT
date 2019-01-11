clk=40MHz
spi_clk=clk/2

PINOUT
IN: 
- opto 5
- resistors 8

OUT:
- high 10 (step*4, dir*4)
- low 8

LED -1
===================
#|   out    |    in
-------------------------
0  pos0[7:0]   vel0[7:0]
1  pos0[15:8]  vel0[11:8]
2  pos0[20:16] vel1[7:0]
3  0           vel1[11:8]
4  pos1[7:0]   vel2[7:0]
5  pos1[15:8]  vel2[11:8]
6  pos1[20:16] vel3[7:0]
7  0           vel3[11:8]
8  pos2[7:0]   real_dout[7:0]
9  pos2[15:8]  real_dout[13:8]
               do_enable_wdt
10 pos2[20:16] dirtime[3:0]
               Spolarity
11 0           steptime[3:0]
               tap[1:0]
12 pos3[7:0]
13 pos3[15:8]
14 pos3[20:16]
15 0
16 din[7:0]
17 din[15:8]
18 0
19 0
20 spibytecnt[4:0]

===================

dtparam=spi=on
dtoverlay=spi1-3cs  #3 chip select
on /boot/config.txt file. 
