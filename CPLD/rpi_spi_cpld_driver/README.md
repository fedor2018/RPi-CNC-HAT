
Read registers
==============

The "read" registers (data transfer from pluto to PC) are each 4 bytes
long and start at address 0.  Beginning a read within a register (e.g.,
at address 2) results in undefined behavior.  Bits without an
interpretation are undefined.

Address		Bits	Interpretation
0..3		0..20	Stepgen 0 position in 10.10 fixed-point notation
4..7		0..20	Stepgen 1 position in 10.10 fixed-point notation
8..11		0..20	Stepgen 2 position in 10.10 fixed-point notation
12..15		0..20	Stepgen 3 position in 10.10 fixed-point notation
16..17		0..15	Digital inputs 0..15
18..19		0..15	RPM


Write registers
===============

The "write" registers (data transfer from pluto to PC) are each 2 bytes
long and start at address 0.  Beginning a read within a register (e.g.,
at address 1) results in undefined behavior.  Bits without an
interpretation should be sent as 0.

Address		Bits	Interpretation
0..1		0..11	Stepgen 0 velocity in +-.10 fixed-point notation
2..3		0..11	Stepgen 1 velocity in +-.10 fixed-point notation
4..5		0..11	Stepgen 2 velocity in +-.10 fixed-point notation
6..7		0..11	Stepgen 3 velocity in +-.10 fixed-point notation
8..9		0..13	Digital outputs 0..13
10..11		0..3	Step time in units of 1600ns
		8..11	Direction setup/hold time in units of 1600ns
		14	write 1 to enable watchdog and reset WDT
		15	step polarity for stepgen 0..3
12..15		0..7	pwm

#|   out      |    in
-------------------------0
0  pos0[7:0]   vel0[7:0]
1  pos0[15:8]  vel0[11:8]
2  pos0[20:16] vel1[7:0]
3  0           vel1[11:8]
-------------------------1
4  pos1[7:0]   vel2[7:0]
5  pos1[15:8]  vel2[11:8]
6  pos1[20:16] vel3[7:0]
7  0           vel3[11:8]
-------------------------2
8  pos2[7:0]   real_dout[7:0]
9  pos2[15:8]  real_dout[13:8]
               do_enable_wdt
10 pos2[20:16] dirtime[3:0]
               Spolarity[7]
11 0           steptime[3:0]
               tap[1:0]
-------------------------3
12 pos3[7:0]   in_pwm[7:0]
13 pos3[15:8]
14 pos3[20:16]
15 0
-------------------------4
16 din[7:0]
17 din[15:8]
18 rpm[7:0]
19 rpm[15:8]
-------------------------5
20 spibytecnt[4:0]

Notes
=====

Watchdog
========
The watchdog consists of a 7-bit counter and a 1-bit enable which gives a
watchdog period of approximately 6.5ms.  At power-up the enable is FALSE.  When
bit 14 of register 8..9 is written TRUE, the counter is reset to 0 and enable
is set to TRUE. (there is no provision for resetting the watchdog enable to
FALSE) Every time the PWM counter overflows (approximately 51.2us) the watchdog
counter is incremented if it is not already at its maximum.  When the watchdog
counter is at its maximum, the dedicated digital outputs and PWM outputs are
tristated.

--------------
speedrange [default: 0]
Selects one of four speed ranges:
    0: Top speed 312.5kHz; minimum speed 610Hz
    1: Top speed 156.25kHz; minimum speed 305Hz
    2: Top speed 78.125kHz; minimum speed 153Hz
    3: Top speed 39.06kHz; minimum speed 76Hz
Choosing the smallest maximum speed that is above the maximum for any one 
axis may give improved step regularity at low step speeds.
