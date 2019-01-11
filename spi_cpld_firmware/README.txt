clk=40MHz
spi_clk=clk/2

PINOUT
IN: 
- opto 5
- resistors 8

OUT:
- high 10
- low 8

LED -1

Protocol:
pos0  > 4b
pos1  > 4b
pos2  > 4b
pos3  > 4b
din   > 4b
spicnt> 1b

vel0 < 2b
vel1 < 2b
vel2 < 2b
vel3 < 2b
dout < 2b
tap,steptime,spolarity,dirtime < 2b


dtparam=spi=on
dtoverlay=spi1-3cs  #3 chip select
on /boot/config.txt file. 
