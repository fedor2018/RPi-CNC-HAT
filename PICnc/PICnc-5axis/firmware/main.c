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
 *
 *    22OCT2014 PJS started changes to support the PICnc 5 axis hardware
 *	  07NOV2014 PJS started adding hardware PWM support
 */
 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <plib.h>
#include "hardware.h"
#include "stepgen.h"
	

static volatile uint32_t rxBuf[BUFSIZE], txBuf[BUFSIZE];
static volatile int spi_data_ready;

static void map_peripherals(){
	/* unlock PPS sequence */
	SYSKEY = 0x0;			/* make sure it is locked */
	SYSKEY = 0xAA996655;		/* Key 1 */
	SYSKEY = 0x556699AA;		/* Key 2 */
	CFGCONbits.IOLOCK=0;		/* now it is unlocked */

	/* map SPI pins */
	PPSInput(4, SS2, RPB14);	/* CS */
	PPSInput(3, SDI2, RPB13);	/* MOSI */
	PPSOutput(2, RPB11, SDO2);	/* MISO */
	/* SPI clock pin placement is fixed and cannot be changed */

	/* map SPI and PWM pins */
	// TODO add code to enable the hardware PWM outputs if used
	PPSOutput(1, RPA0, OC1);	/* PWM (OUT4 laser power) */
	PPSOutput(2, RPA1, OC2);	/* PWM (OUT5 lights) */
	PPSOutput(3, RPB2, OC4);	/* PWM (OUT8) */
	PPSOutput(4, RPB0, OC3);	/* PWM (OUT6 air valve) */

	/* lock PPS sequence */
	CFGCONbits.IOLOCK=1;		/* now it is locked */
	SYSKEY = 0x0;			/* lock register access */
}

static void init_io_ports(){
	/* disable all analog pins */
	ANSELA = 0x0;
	ANSELB = 0x0;
	ANSELC = 0x0;

	/* configure inputs */
	TRISBSET = BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_13 | BIT_14 | BIT_15;
	TRISCSET = BIT_4 | BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9;

	/* configure_outputs */
	TRISACLR = BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4 | BIT_7 | BIT_8 | BIT_9 | BIT_10;
	TRISBCLR = BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4 | BIT_11 | BIT_12;
	TRISCCLR = BIT_0 | BIT_1 | BIT_2 | BIT_3;

}

static void init_spi(){
	int i;

	SPI2CON = 0;			/* stop SPI 2, set Slave mode, 8 bits, std buffer */
	i = SPI2BUF;			/* clear rcv buffer */
	SPI2CON = 1<<8 | 0<<6;	/* Clock Edge */
	SPI2CONSET = 1<<15;		/* start SPI 2 */
}

static void init_dma(){
	/* open and configure the DMA channels
	     DMA 0 is for SPI -> buffer
	     DMA 1 is for buffer -> SPI */
	DmaChnOpen(DMA_CHANNEL0, DMA_CHN_PRI3, DMA_OPEN_AUTO);
	DmaChnOpen(DMA_CHANNEL1, DMA_CHN_PRI3, DMA_OPEN_AUTO);

	/* DMA channels trigger on SPI RX/TX */
	DmaChnSetEventControl(DMA_CHANNEL0, DMA_EV_START_IRQ(_SPI2_RX_IRQ));
	DmaChnSetEventControl(DMA_CHANNEL1, DMA_EV_START_IRQ(_SPI2_TX_IRQ));

	/* transfer 8bits at a time */
	DmaChnSetTxfer(DMA_CHANNEL0, (void *)&SPI2BUF, (void *)rxBuf, 1, SPIBUFSIZE, 1);
	DmaChnSetTxfer(DMA_CHANNEL1, (void *)txBuf, (void *)&SPI2BUF, SPIBUFSIZE, 1, 1);

	/* start DMA 0 */
	DmaChnEnable(0);
	DmaChnEnable(1);
}

static inline uint32_t read_inputs(){
	uint32_t x;
	
	#ifdef LED_TOGGLE
		uint32_t y;
		LED_TOGGLE0;	/* push LED state */
		LED_TOGGLE1;	/* set LED pin high */
		LED_TOGGLE2;	/* set LED pin as input */
		LED_TOGGLE3;		/* pop LED state */
	#endif /* LED_TOGGLE */

	x  = (IO_IN1 ? 1 : 0) << 0;
	x |= (IO_IN2 ? 1 : 0) << 1;
	x |= (IO_IN3 ? 1 : 0) << 2;
	x |= (IO_IN4 ? 1 : 0) << 3;
	x |= (IO_IN5 ? 1 : 0) << 4;
	x |= (IO_IN6 ? 1 : 0) << 5;
	x |= (IO_IN7 ? 1 : 0) << 6;
	x |= (IO_IN8 ? 1 : 0) << 7;
	x |= (IO_IN9 ? 1 : 0) << 8;
	x |= (IO_IN10 ? 1 : 0) << 9;
	x |= (IO_IN11 ? 1 : 0) << 10;
	x |= (IO_IN12 ? 1 : 0) << 11;

	#ifdef LED_TOGGLE
		LED_TOGGLE4;	/* set LED pin as output */
	#endif /* LED_TOGGLE */

	return x;
}

static inline void update_outputs(uint32_t x){
	if (x & (1 << 0)){
		IO_OUT1_HI;
	}else{
		IO_OUT1_LO;
	}
	if (x & (1 << 1)){
		IO_OUT2_HI;
	}else{
		IO_OUT2_LO;
	}
	if (x & (1 << 2)){
		IO_OUT3_HI;
	}else{
		IO_OUT3_LO;
	}
	if (x & (1 << 3)){
		IO_OUT4_HI;
	}else{
		IO_OUT4_LO;
	}
// TODO add code here to deal with when the hardware PWM is turned on (if ever used) 	
	if (x & (1 << 4)){
		IO_OUT5_HI;
	}else{
		IO_OUT5_LO;
	}
// TODO add code here to deal with when the hardware PWM is turned on (if ever used) 	
	if (x & (1 << 5)){
		IO_OUT6_HI;
	}else{
		IO_OUT6_LO;
	}
// TODO add code here to deal with when the hardware PWM is turned on (if ever used) 	
	if (x & (1 << 6)){
		IO_OUT7_HI;
	}else{
		IO_OUT7_LO;
	}
	if (x & (1 << 7)){
		IO_OUT8_HI;
	}else{
		IO_OUT8_LO;
	}
// TODO add code here to deal with when the hardware PWM is turned on (if ever used) 	
	if (x & (1 << 8)){
		IO_OUT9_HI;
	}else{
		IO_OUT9_LO;
	}
	#if MAXGEN < 5 /* CONFIGURE_B_AXIS */
		if (x & (1 << 9)){
			IO_OUT10_HI;
		}else{
			IO_OUT10_LO;
		}
		if (x & (1 << 10)){
			IO_OUT11_HI;
		}else{
			IO_OUT11_LO;
		}
	#endif 
	#if MAXGEN < 4 /* CONFIGURE_A_AXIS */
		if (x & (1 << 11)){
			IO_OUT12_HI;
		}else{
			IO_OUT12_LO;
		}
		if (x & (1 << 12)){
			IO_OUT13_HI;
		}else{
			IO_OUT13_LO;
		}
	#endif 
	#if MAXGEN < 3 /* CONFIGURE_Z_AXIS */
		if (x & (1 << 13)){
			IO_OUT14_HI;
		}else{
			IO_OUT14_LO;
		}
		if (x & (1 << 14)){
			IO_OUT15_HI;
		}else{
			IO_OUT15_LO;
		}
	#endif 
	#if MAXGEN < 2 /* CONFIGURE_Y_AXIS */
		if (x & (1 << 15)){
			IO_OUT16_HI;
		}else{
			IO_OUT16_LO;
		}
		if (x & (1 << 16)){
			IO_OUT17_HI;
		}else{
			IO_OUT17_LO;
		}
	#endif 
	#if MAXGEN < 1 /* CONFIGURE_X_AXIS */
		if (x & (1 << 17)){
			IO_OUT18_HI;
		}else{
			IO_OUT18_LO;
		}
		if (x & (1 << 18)){
			IO_OUT19_HI;
		}else{
			IO_OUT19_LO;
		}
	#endif /* CONFIGURE_X_AXIS */
}


// this function sets up PWM
static inline void init_PWM(){
	// timer 2 setup 

	// PWM engine 1
	OC1CON = 0x0000;	// Turn off OCx while doing setup.
	OC1R = 0x0000;		// Init comp reg
	OC1RS = 0x0000;		// Init secondary reg
	OC1CON = 0x0006;	// configure OC to PWM mode no fault pin

	// PWM engine 2
	OC2CON = 0x0000;	// Turn off OCx while doing setup.
	OC2R = 0x0000;		// Init comp reg
	OC2RS = 0x0000;		// Init secondary reg
	OC2CON = 0x0006;	// configure OC to PWM mode no fault pin

	// PWM engine 3
	OC3CON = 0x0000;	// Turn off OCx while doing setup.
	OC3R = 0x0000;		// Init comp reg
	OC3RS = 0x0000;		// Init secondary reg
	OC3CON = 0x0006;	// configure OC to PWM mode no fault pin

	// PWM engine 4
	OC4CON = 0x0000;	// Turn off OCx while doing setup.
	OC4R = 0x0000;		// Init comp reg
	OC4RS = 0x0000;		// Init secondary reg
	OC4CON = 0x0006;	// configure OC to PWM mode no fault pin
	
	// turn on timer 2
	T2CONSET = 0x8000;
	
}


// this function updates and enables the PWM channel 
static inline void update_PWM(uint channel, uint32_t value){
	switch (channel){
		case 1:	
			OC1RS = value;
			OC1CONSET = 0x8000;
			break;		
		case 2:	
			OC2RS = value;
			OC2CONSET = 0x8000;			
			break;		
		case 3:	
			OC3RS = value;
			OC3CONSET = 0x8000;			
			break;		
		case 4:	
			OC4RS = value;
			OC4CONSET = 0x8000;			
			break;		
	}
}

void reset_board(){
	stepgen_reset();
	update_outputs(0); 		/* all outputs low */
}

int main(void){
	int i, spi_timeout;
	unsigned long counter;

	/* Disable JTAG port so we get our I/O pins back */
	DDPCONbits.JTAGEN = 0;
	/* Enable optimal performance */
	SYSTEMConfigPerformance(GetSystemClock());
	/* Use 1:1 CPU Core:Peripheral clocks */
	OSCSetPBDIV(OSC_PB_DIV_1);

	/* configure the core timer roll-over rate */
	OpenCoreTimer(CORE_TICK_RATE);

	/* set up the core timer interrupt */
	mConfigIntCoreTimer((CT_INT_ON | CT_INT_PRIOR_6 | CT_INT_SUB_PRIOR_0));

	/* enable multi vector interrupts */
	INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);
	INTEnableInterrupts();

	map_peripherals();
	init_io_ports();
	init_PWM();
	init_spi();
	init_dma();

	/* wait until tx buffer is filled up */
	while (!SPI2STATbits.SPITBF);

	reset_board();
	spi_data_ready = 0;
	spi_timeout = 0;
	counter = 0;

	/* enable watchdog */
	WDTCONSET = 0x8000;

	/* main loop */
	while (1) {
		if (spi_data_ready) {
			spi_data_ready = 0;

			/* the first element received is a command string */
			switch (rxBuf[0]) {
			case 0x5453523E:	/* >RST */
				reset_board();
				break;
			case 0x314D433E:	/* >CM1 */
				stepgen_update_input((const void *)&rxBuf[1]);
				stepgen_get_position((void *)&txBuf[1]);
				break;
			case 0x324D433E:	/* >CM2 */
				update_outputs(rxBuf[1]);
				txBuf[1] = read_inputs();
				break;
			case 0x334D433E:	/* >CM3 PWM1*/
				update_PWM(1, rxBuf[1]);
				txBuf[1] = rxBuf[1] ^ ~0; /* input buffer exclsive or (^) with all 1's (~0) */
				break;
			case 0x344D433E:	/* >CM3 PWM2*/
				update_PWM(2, rxBuf[1]);
				txBuf[1] = rxBuf[1] ^ ~0; /* input buffer exclsive or (^) with all 1's (~0) */
				break;
			case 0x354D433E:	/* >CM3 PWM3*/
				update_PWM(3, rxBuf[1]);
				txBuf[1] = rxBuf[1] ^ ~0; /* input buffer exclsive or (^) with all 1's (~0) */
				break;
			case 0x364D433E:	/* >CM3 PWM4*/
				update_PWM(4, rxBuf[1]);
				txBuf[1] = rxBuf[1] ^ ~0; /* input buffer exclsive or (^) with all 1's (~0) */
				break;
			case 0x4746433E:	/* >CFG */
				stepgen_update_stepwidth(rxBuf[1]);
				stepgen_reset();
				break;
			case 0x5453543E:	/* >TST */
				for (i=0; i<BUFSIZE; i++)
					txBuf[i] = rxBuf[i] ^ ~0; /* input buffer exclsive or (^) with all 1's (~0) */
				break;
			}
		}

		/* if rx buffer is half-full, update the integrity check.
		   There isn't enough time if we wait for complete transfer */
		if (DCH0INTbits.CHDHIF) {
			DCH0INTCLR = 1<<4;		/* clear flag */
			txBuf[0] = rxBuf[0] ^ ~0;
		}

		/* if rx buffer is full, data from spi bus is ready */
		if (DCH0INTbits.CHBCIF) {
			DCH0INTCLR = 1<<3;		/* clear flag */
			spi_data_ready = 1;
			spi_timeout = SPI_TIMEOUT;
		}

		/* dec the SPI timeout timer */
		if (spi_timeout){
			spi_timeout--;
		}

		/* reset the board if there is no SPI activity */
		if (spi_timeout == 1) {				
			DCH0ECONSET=BIT_6;	/* abort DMA transfers */
			DCH1ECONSET=BIT_6;
		
			init_spi();
			init_dma();
			reset_board();

			/* wait until tx buffer is filled up */
			while (!SPI2STATbits.SPITBF);
		}

		#ifdef LED_TOGGLE
			/* blink onboard led */
			if (!(counter++ % (spi_timeout ? 0x10000 : 0x40000))) {
				LED_TOGGLE;
			}
		#endif /* LED_TOGGLE */
		
		/* keep watchdog alive */
		WDTCONSET = 0x01;
	}
	return 0;
}

void __ISR(_CORE_TIMER_VECTOR, ipl6) CoreTimerHandler(void){
	/* update the period */
	UpdateCoreTimer(CORE_TICK_RATE);

	/* do repetitive tasks here */
	stepgen();

	/* clear the interrupt flag */
	mCTClearIntFlag();
}


