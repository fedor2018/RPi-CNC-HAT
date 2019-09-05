/*    Copyright (C) 2014 GP Orcullo
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * 	  22OCT2014 PJS updated to match the PICnc 5 axis hardware settings
 *    24OCT2014 PJS updated core freq to 40MHz from 48MHz to not overclock the PIC32s I am using (max freq 40MHz special order parts can go up to 50MHz) and moved in MAXGEN from stepgen.h
 */

#ifndef __HARDWARE_H__
	#define __HARDWARE_H__
	
	// #####################################
	// Start user configuration defs
	// #####################################
	// Update the code to decide which channels to configure for stepgen based off this define
	// Set the number of active stepgen channels starting with X -> Y -> Z -> A -> B (note 5 is the max defined) 
	// this can be set up via -D options in the make file 
	#ifndef MAXGEN
		#define MAXGEN		4	
	#endif
	
	// PJS 22OCT2014 removing the status led for now may make it a compile option in the future... 
	//#define LED_TOGGLE	(LATCINV = BIT_4)
	//#define LED_TOGGLE0	(y = BIT_4 & ~LATC)	
	//#define LED_TOGGLE1	(LATCSET = BIT_4)	
	//#define LED_TOGGLE2	(TRISCSET = BIT_4)	
	//#define LED_TOGGLE3	(LATCCLR = y)	
	//#define LED_TOGGLE4	(TRISCCLR = BIT_4)	
	
	// #####################################
	// End user configuration defs
	// #####################################
	
	// set the chip fuses
	#ifndef __NO_FUSE__
		#pragma config POSCMOD = OFF		/* Primary Oscillator disabled */
		#pragma config FNOSC = FRCPLL		/* Fast RC Osc w/Div-by-N */
		#pragma config FPLLODIV = DIV_2		/* PLL configured for 40MHz clock */
		#pragma config FPLLMUL = MUL_20
		#pragma config FPLLIDIV = DIV_2
		#pragma config FPBDIV = DIV_1		/* Peripheral Clock Divisor */
		#pragma config IESO = ON			/* Internal/External Switch Over enabled */
		#pragma config FSOSCEN = OFF		/* Secondary Oscillator disabled */
		#pragma config CP = OFF				/* Code Protect Disabled */
		#pragma config FWDTEN = ON			/* Watchdog Timer Enable */
		#pragma config WDTPS = PS4096		/* Watchdog Timer Postscaler */
		#pragma config PMDL1WAY = OFF		/* Allow multiple PM configurations */
		#pragma config IOL1WAY = OFF		/* Allow multiple PPS configurations */
	#endif
	
	
	// preformance config 
	#define SYS_FREQ				(40000000ul)    /* 40 MHz */
	#define GetSystemClock()		(SYS_FREQ)
	#define	GetPeripheralClock()	(GetSystemClock())
	#define	GetInstructionClock()	(GetSystemClock())

	// timer config
	#define BASEFREQ				80000
	#define CORE_TICK_RATE 			(SYS_FREQ/2/BASEFREQ)
	#define SPIBUFSIZE				20
	#define BUFSIZE					(SPIBUFSIZE/4)

	#define SPI_TIMEOUT				1000L
	
	#define SPICHAN					2
	
	/*    PORT USAGE (pin numbers for a 44 quad flat part PICnc-5Axis)
	 *
	 *	Pin 	Port	Dir 	Signal-notes
	 *
	 *	19  	RA0  	OUT  	OUT4-PWM-Laser_power
	 *	20  	RA1		OUT		OUT5-PWM-Lights	
	 * 	30  	RA2  	OUT 	OUT13-Astep
	 * 	31  	RA3  	OUT 	OUT14-Zdir
	 * 	34  	RA4  	OUT 	OUT17-Ystep
	 * 	13  	RA7  	OUT 	OUT3-Laser_fire
	 *	32  	RA8  	OUT 	OUT15-Zstep
	 * 	35  	RA9  	OUT 	OUT18-Xdir
	 * 	12		RA10 	OUT 	OUT2-Beeper
	 *
	 * 	21  	RB0  	OUT 	OUT6-PWM-Air_valve
	 * 	22  	RB1  	OUT 	OUT7_PS_on
	 * 	23  	RB2  	OUT 	OUT8 (PWM supportable on this pin as well) 
	 * 	24  	RB3  	OUT 	OUT9
	 * 	33  	RB4  	OUT 	OUT16-Ydir
	 * 	41  	RB5  	IN   	IN3-I2C_fault
	 * 	42  	RB6  	IN   	IN4-Door
	 * 	43  	RB7  	IN   	IN5-Laser_off
	 * 	44  	RB8  	IN   	IN6-Ablock
	 * 	1    	RB9  	IN   	IN7
	 * 	8    	RB10  	IN   	IN12-Xhome
	 * 	9   	RB11	SPI 	MISO
	 * 	10  	RB12  	OUT   	OUT1-Enable
	 * 	11   	RB13	SPI 	MOSI
	 * 	14   	RB14	SPI 	CS
	 * 	15   	RB15	SPI 	Sclk
	 *
	 * 	25  	RC0  	OUT   	OUT10-Bdir
	 * 	26  	RC1  	OUT   	OUT11-Bstep
	 * 	27  	RC2  	OUT   	OUT12-Adir
	 * 	36  	RC3  	OUT   	OUT19-Xstep
	 * 	37    	RC4  	IN   	IN1-Stop
	 * 	38    	RC5  	IN   	IN2-EPO
	 * 	2    	RC6  	IN   	IN8-Bhome
	 * 	3    	RC7  	IN   	IN9-Ahome
	 * 	4    	RC8  	IN   	IN10-Zhome
	 * 	5    	RC9  	IN   	IN11-Yhome
	 *
	 */
	
	
	// input pin definitions 
	#define IO_IN1			(PORTCbits.RC4)
	#define IO_IN2			(PORTCbits.RC5)
	#define IO_IN3			(PORTBbits.RB5)
	#define IO_IN4			(PORTBbits.RB6)
	#define IO_IN5			(PORTBbits.RB7)
	#define IO_IN6			(PORTBbits.RB8)
	#define IO_IN7			(PORTBbits.RB9)
	#define IO_IN8			(PORTCbits.RC6)
	#define IO_IN9			(PORTCbits.RC7)
	#define IO_IN10			(PORTCbits.RC8)
	#define IO_IN11			(PORTCbits.RC9)
	#define IO_IN12			(PORTBbits.RB10)
	
	// output pin definitions
	#define IO_OUT1_LO		(LATBCLR = BIT_12)
	#define IO_OUT1_HI		(LATBSET = BIT_12)
	#define IO_OUT2_LO		(LATACLR = BIT_10)
	#define IO_OUT2_HI		(LATASET = BIT_10)
	#define IO_OUT3_LO		(LATACLR = BIT_7)
	#define IO_OUT3_HI		(LATASET = BIT_7)
	#define IO_OUT4_LO		(LATACLR = BIT_0)
	#define IO_OUT4_HI		(LATASET = BIT_0)
	#define IO_OUT5_LO		(LATACLR = BIT_1)
	#define IO_OUT5_HI		(LATASET = BIT_1)
	#define IO_OUT6_LO		(LATBCLR = BIT_0)
	#define IO_OUT6_HI		(LATBSET = BIT_0)
	#define IO_OUT7_LO		(LATBCLR = BIT_1)
	#define IO_OUT7_HI		(LATBSET = BIT_1)
	#define IO_OUT8_LO		(LATBCLR = BIT_2)
	#define IO_OUT8_HI		(LATBSET = BIT_2)
	#define IO_OUT9_LO		(LATBCLR = BIT_3)
	#define IO_OUT9_HI		(LATBSET = BIT_3)
	#if MAXGEN < 5
		#define IO_OUT10_LO		(LATCCLR = BIT_0)
		#define IO_OUT10_HI		(LATCSET = BIT_0)
		#define IO_OUT11_LO		(LATCCLR = BIT_1)
		#define IO_OUT11_HI		(LATCSET = BIT_1)
	#endif /* CONFIGURE_B_AXIS */
	#if MAXGEN < 4
		#define IO_OUT12_LO		(LATCCLR = BIT_2)
		#define IO_OUT12_HI		(LATCSET = BIT_2)
		#define IO_OUT13_LO		(LATACLR = BIT_2)
		#define IO_OUT13_HI		(LATASET = BIT_2)
	#endif /* CONFIGURE_A_AXIS */
	#if MAXGEN < 3
		#define IO_OUT14_LO		(LATACLR = BIT_3)
		#define IO_OUT14_HI		(LATASET = BIT_3)
		#define IO_OUT15_LO		(LATACLR = BIT_8)
		#define IO_OUT15_HI		(LATASET = BIT_8)
	#endif /* CONFIGURE_Z_AXIS */
	#if MAXGEN < 2
		#define IO_OUT16_LO		(LATBCLR = BIT_4)
		#define IO_OUT16_HI		(LATBSET = BIT_4)
		#define IO_OUT17_LO		(LATACLR = BIT_4)
		#define IO_OUT17_HI		(LATASET = BIT_4)
	#endif /* CONFIGURE_Y_AXIS */
	#if MAXGEN < 1
		#define IO_OUT18_LO		(LATACLR = BIT_9)
		#define IO_OUT18_HI		(LATASET = BIT_9)
		#define IO_OUT19_LO		(LATCCLR = BIT_3)
		#define IO_OUT19_HI		(LATCSET = BIT_3)
	#endif /* CONFIGURE_X_AXIS */

	
	// step/dir pin definitions
	#if MAXGEN >= 1
		#define STEP_X_LO		(LATCCLR = BIT_3)
		#define STEP_X_HI		(LATCSET = BIT_3)
		#define DIR_X_LO		(LATACLR = BIT_9)
		#define DIR_X_HI		(LATASET = BIT_9)
	#endif /* CONFIGURE_X_AXIS */
	
	#if MAXGEN >= 2
		#define STEP_Y_LO		(LATACLR = BIT_4)
		#define STEP_Y_HI		(LATASET = BIT_4)
		#define DIR_Y_LO		(LATBCLR = BIT_4)
		#define DIR_Y_HI		(LATBSET = BIT_4)
	#endif /* CONFIGURE_Y_AXIS */
	
	#if MAXGEN >= 3
		#define STEP_Z_LO		(LATACLR = BIT_8)
		#define STEP_Z_HI		(LATASET = BIT_8)
		#define DIR_Z_LO		(LATACLR = BIT_3)
		#define DIR_Z_HI		(LATASET = BIT_3)
	#endif /* CONFIGURE_Z_AXIS */
	
	#if MAXGEN >= 4
		#define STEP_A_LO		(LATACLR = BIT_2)
		#define STEP_A_HI		(LATASET = BIT_2)
		#define DIR_A_LO		(LATCCLR = BIT_2)
		#define DIR_A_HI		(LATCSET = BIT_2)
	#endif /* CONFIGURE_A_AXIS */
	
	#if MAXGEN >= 5
		#define STEP_B_LO		(LATCCLR = BIT_1)
		#define STEP_B_HI		(LATCSET = BIT_1)
		#define DIR_B_LO		(LATCCLR = BIT_0)
		#define DIR_B_HI		(LATCSET = BIT_0)
	#endif /* CONFIGURE_B_AXIS */

#endif /* __HARDWARE_H__ */