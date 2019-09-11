// graphicdisplay, firmware for the lcd graphic display
// Copyright (C) 2009, Frank Tkalcevic, www.franksworkshop.com

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.



#include "compiler.h"
#include "preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <avr32/io.h>
#include "pm.h"
#include "gpio.h"
#include "delay.h"
#include "spi.h"
#include "intc.h"
#include "cycle_counter.h"
#include "flashc.h"

typedef unsigned char uint8_t;
typedef uint8_t byte;
typedef unsigned short uint16_t;
typedef byte bool;
#define true 1
#define false 0

#pragma pack(1)
#include "displaycommands.h"
#pragma pack(4)


#define PROGMEM
#define _BV(n)		(1<<(n))
#include "../fonts/Font6x13.h"
#include "../fonts/FontTypewriter.h"

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240

#define TEXT_WIDTH			(SCREEN_WIDTH/8-1)
#define TEXT_HEIGHT			(SCREEN_HEIGHT/8-1)

#define sign(x) ((x) < 0 ? -1 : 1 )

#define DATA_MASK			(~(0xFF<<6))
#define LCDD0 				AVR32_PIN_PA06
#define LCDD1 				AVR32_PIN_PA07
#define LCDD2 				AVR32_PIN_PA08
#define LCDD3 				AVR32_PIN_PA09
#define LCDD4 				AVR32_PIN_PA10
#define LCDD5 				AVR32_PIN_PA11
#define LCDD6 				AVR32_PIN_PA12
#define LCDD7 				AVR32_PIN_PA13
#define LCDReset 			AVR32_PIN_PA05
#define LCDDC 				AVR32_PIN_PA04
#define LCDRW 				AVR32_PIN_PA03
#define LCDE 				AVR32_PIN_PA27
#define LCDBacklightPWM		AVR32_PIN_PA26

#define FOSC0           12000000                              //!< Osc0 frequency: Hz.
#define OSC0_STARTUP    AVR32_PM_OSCCTRL0_STARTUP_2048_RCOSC  //!< Osc0 startup time: RCOsc periods.

struct SFontData
{
    byte nCharWidth;
    byte nCharHeight;
    byte nCharsPerLine;
    byte nLines;
    uint8_t * PROGMEM pFont;
    uint8_t * PROGMEM nFontOffsets[128];
};


static uint16_t g_BackgroundColour;
static uint16_t g_ForegroundColour;
static uint8_t g_TextX;
static uint8_t g_TextY;
static uint16_t g_PixelX;
static uint8_t g_PixelY;
static bool g_bPixelCursor;
static struct SFontData *g_pFont;

#define FIRST_CHAR  0x20
#define CHAR_COUNT  0x60


static struct SFontData  fontData[] =
{
/*45x17*/    { FONT6X13_WIDTH, FONT6X13_HEIGHT, SCREEN_WIDTH / FONT6X13_WIDTH, SCREEN_HEIGHT / FONT6X13_HEIGHT, Font6x13 },
/*22x9 */    { FONTTYPEWRITER_WIDTH, FONTTYPEWRITER_HEIGHT, SCREEN_WIDTH / FONTTYPEWRITER_WIDTH, SCREEN_HEIGHT / FONTTYPEWRITER_HEIGHT, FontTypewriter },
};

static inline unsigned short SwapBytes( unsigned short n )
{
	unsigned short temp = n;
	byte buf[2];
	buf[0] = *(((byte *)(&temp))+1);
	buf[1] = *(((byte *)(&temp))+0);
	return *(unsigned short *)buf;
}


void ioinit()
{
#define CLOCK 60000000
	// Set the clock to 60 MHz
	pm_switch_to_osc0(&AVR32_PM, FOSC0, OSC0_STARTUP);
	pm_pll_setup(&AVR32_PM, 0,  // pll.
	             14,  // mul.
	             2,   // div.
	             0,   // osc.
	             16); // lockcount.
	pm_pll_set_option(&AVR32_PM, 0, // pll.
	                  1,  // pll_freq.
	                  1,  // pll_div2.
	                  0); // pll_wbwdisable.
	pm_pll_enable(&AVR32_PM, 0);
	pm_wait_for_pll0_locked(&AVR32_PM);
	pm_cksel(&AVR32_PM,
	         1,   // pbadiv.
	         0,   // pbasel.
	         1,   // pbbdiv.
	         0,   // pbbsel.
	         1,   // hsbdiv.
	         0);  // hsbsel.
	pm_switch_to_clock(&AVR32_PM, AVR32_PM_MCCTRL_MCSEL_PLL0);

	flashc_set_wait_state(1);

	delay_init(CLOCK);

	// Set GPIO outputs
	// set PA06 to output
	gpio_local_init();

	gpio_enable_gpio_pin(LCDD0);
	gpio_enable_gpio_pin(LCDD1);
	gpio_enable_gpio_pin(LCDD2);
	gpio_enable_gpio_pin(LCDD3);
	gpio_enable_gpio_pin(LCDD4);
	gpio_enable_gpio_pin(LCDD5);
	gpio_enable_gpio_pin(LCDD6);
	gpio_enable_gpio_pin(LCDD7);
	gpio_enable_gpio_pin(LCDReset);
	gpio_enable_gpio_pin(LCDDC);
	gpio_enable_gpio_pin(LCDRW);
	gpio_enable_gpio_pin(LCDE);
	gpio_enable_gpio_pin(LCDBacklightPWM);

	gpio_local_enable_pin_output_driver(LCDD0);
	gpio_local_enable_pin_output_driver(LCDD1);
	gpio_local_enable_pin_output_driver(LCDD2);
	gpio_local_enable_pin_output_driver(LCDD3);
	gpio_local_enable_pin_output_driver(LCDD4);
	gpio_local_enable_pin_output_driver(LCDD5);
	gpio_local_enable_pin_output_driver(LCDD6);
	gpio_local_enable_pin_output_driver(LCDD7);
	gpio_local_enable_pin_output_driver(LCDReset);
	gpio_local_enable_pin_output_driver(LCDDC);
	gpio_local_enable_pin_output_driver(LCDRW);
	gpio_local_enable_pin_output_driver(LCDE);
	gpio_local_enable_pin_output_driver(LCDBacklightPWM);

	// We always write
    gpio_local_clr_gpio_pin(LCDRW);
}

/*
*********************************************************************************************************
*                         				LOW LEVEL COMMAND WRITE TO LCD
*
* Description : This function performs low level command write to LCD
* Arguments   : (uint8_t) 'cmd' 	is the command written to the LCD module
* Returns     : none
* Notes		  : Hardware specific.
*********************************************************************************************************
*/
static void cDispWrCmd(uint8_t cmd)
{
	AVR32_GPIO_LOCAL.port[0].ovr = (AVR32_GPIO_LOCAL.port[0].ovr & DATA_MASK)| (cmd <<6);
    gpio_local_clr_gpio_pin(LCDDC);
    gpio_local_set_gpio_pin(LCDE);
    gpio_local_clr_gpio_pin(LCDE);
}

/*
*********************************************************************************************************
*                         				LOW LEVEL DATA WRITE TO LCD
*
* Description : This function performs low level display data write to LCD
* Arguments   : (uint8_t) 'dat' 	is the data written to the LCD module
* Returns     : none
* Notes		  : Hardware specific.
*********************************************************************************************************
*/
static void cDispWrDat(uint8_t dat)
{
	AVR32_GPIO_LOCAL.port[0].ovr = (AVR32_GPIO_LOCAL.port[0].ovr & DATA_MASK) | (dat <<6);
    gpio_local_set_gpio_pin(LCDDC);
    gpio_local_set_gpio_pin(LCDE);
    gpio_local_clr_gpio_pin(LCDE);
}


// We work with a horizontal display and use xy values which make sense to the orientation.
// Therefore we map our xy to the device xy
static  void LCD_SetXY( int x, byte y ) // 0..239, 0..319
{
    cDispWrCmd(0x4F);	// set device Y
    cDispWrDat((x >> 8)&1);
    cDispWrDat(x & 0xFF);

    cDispWrCmd(0x4E);	// set device X
    cDispWrDat(0);
    cDispWrDat(y);
}

static  void LCD_SetWindow( uint16_t x1, byte y1, uint16_t x2, byte y2 )
{
    cDispWrCmd(0x44);	//Horizontal RAM address position start/end setup
    cDispWrDat(y2);
    cDispWrDat(y1);

    cDispWrCmd(0x45);	//Vertical RAM address start position setting
    cDispWrDat((x1>>8)&1);
    cDispWrDat(x1&0xFF);

    cDispWrCmd(0x46);	//Vertical RAM address end position setting
    cDispWrDat((x2>>8)&1);
    cDispWrDat(x2&0xFF);
}

static void LCD_SolidRect( int x, byte y, int nWidth, byte nHeight )
{
    uint16_t c = g_ForegroundColour;

    byte b1 = *(((uint8_t *)&c)+0);
    byte b2 = *(((uint8_t *)&c)+1);

    LCD_SetXY( x, y );
    LCD_SetWindow( x, y, x+nWidth-1, y+nHeight-1 );
    cDispWrCmd(0x22);
    unsigned int n = (int)nHeight * (int)nWidth;

    int port = AVR32_GPIO_LOCAL.port[0].ovr | _BV(LCDDC);
    AVR32_GPIO_LOCAL.port[0].ovr = port;
    int byte1 = (port & DATA_MASK) |(b1<<6);
    int byte2 = (port & DATA_MASK) |(b2<<6);

    while ( n-- )
    {
    	AVR32_GPIO_LOCAL.port[0].ovr = byte1;
	    AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
	    AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);

    	AVR32_GPIO_LOCAL.port[0].ovr = byte2;
	    AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
	    AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
    }
}

static void LCD_Rect( int x, byte y, int nWidth, byte nHeight )
{
    LCD_SolidRect( x, y, nWidth, 1 );
    LCD_SolidRect( x, y, 1, nHeight );
    LCD_SolidRect( x+nWidth-1, y, 1, nHeight );
    LCD_SolidRect( x, y+nHeight-1, nWidth, 1 );
}

// clear the screen.  We paint every pixel, so we don't
// have to worry about setting x,y to 0.  We just wrap.
static void LCD_ClearScreen( void )
{
    LCD_SetWindow( 0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1 );		// Set Window

    byte b1 = *((byte *)(&g_BackgroundColour));
    byte b2 = *((byte *)(&g_BackgroundColour) + 1);

    cDispWrCmd(0x22);											// data mode

    int port = AVR32_GPIO_LOCAL.port[0].ovr | _BV(LCDDC);
    if ( b1 == b2 )
    {
    	// Optimisation when we output the same byte twice
        AVR32_GPIO_LOCAL.port[0].ovr = (port & DATA_MASK) | (b1 <<6);
        int n =240*320;
        while ( n )
        {
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);		// byte 1
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
        	n--;
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);		// byte 2
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
        }
    }
    else
    {
    	int byte1 = (port & DATA_MASK) | (b1 <<6);
    	int byte2 = (port & DATA_MASK) | (b2 <<6);
        int n =240*320;
		while ( n )
		{
        	AVR32_GPIO_LOCAL.port[0].ovr = byte1;		// byte 1
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);

        	n--;
        	AVR32_GPIO_LOCAL.port[0].ovr = byte2;		// byte 2
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
        	AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
		}
    }
}


//static void DrawText( byte nLen, const char *s )
//{
//    uint16_t y = g_TextY;
//    y *= g_pFont->nCharHeight;
//    uint16_t x = g_TextX;
//    x *= g_pFont->nCharWidth;
//    uint16_t width = nLen;
//    width *= g_pFont->nCharWidth;
//
//    LCD_SetWindow( x, y, x+width-1, y+g_pFont->nCharHeight-1 );
//    LCD_SetXY( x, y );
//    cDispWrCmd(0x22);
//
//    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
//    CTRLOUT_PORT |= _BV(CTRLOUT_DC);
//
//    byte bHigh = CTRLOUT_PORT | _BV(CTRLOUT_E);
//    byte bLow = CTRLOUT_PORT & ~_BV(CTRLOUT_E);
//
//    byte bg2 = *((byte *)(&g_BackgroundColour));
//    byte bg1 = *((byte *)(&g_BackgroundColour) + 1);
//
//    byte fg2 = *((byte *)(&g_ForegroundColour));
//    byte fg1 = *((byte *)(&g_ForegroundColour) + 1);
//
//    byte nColMask = 7;
//    for ( byte row = 0; row < g_pFont->nCharHeight; row++ )
//    {
//	byte nFontOffset = (byte)row * (byte)((g_pFont->nCharWidth+7)/8);
//	const char *ptr = s;
//	byte nCount = nLen;
//	while ( nCount-- )
//	{
//	    char c = *ptr++;
//
//	    uint8_t * PROGMEM pFont = g_pFont->nFontOffsets[(byte)c] +  + nFontOffset;
//
//	    byte data, bit;
//	    for ( byte col = 0; col < g_pFont->nCharWidth; col++ )
//	    {
//		if ( (col & nColMask) == 0 )
//		{
//		    data = pgm_read_byte( pFont++ );
//		    bit = 0x80;
//		}
//		if ( data & bit )
//		{
//		    DATAOUT_PORT = fg1;
//		    CTRLOUT_PORT = bHigh;
//		    CTRLOUT_PORT = bLow;
//
//		    DATAOUT_PORT = fg2;
//		    CTRLOUT_PORT = bHigh;
//		    CTRLOUT_PORT = bLow;
//		}
//		else
//		{
//		    DATAOUT_PORT = bg1;
//		    CTRLOUT_PORT = bHigh;
//		    CTRLOUT_PORT = bLow;
//
//		    DATAOUT_PORT = bg2;
//		    CTRLOUT_PORT = bHigh;
//		    CTRLOUT_PORT = bLow;
//		}
//		bit >>= 1;
//	    }
//	}
//    }
//}
//
//
static void DrawChar( uint16_t x, uint8_t y, char c )
{
    LCD_SetWindow( x, y, x+g_pFont->nCharWidth-1, y+g_pFont->nCharHeight-1 );
    LCD_SetXY( x, y );
    cDispWrCmd(0x22);

    int port = AVR32_GPIO_LOCAL.port[0].ovr | _BV(LCDDC);
	AVR32_GPIO_LOCAL.port[0].ovr = port;

    int bg1 = (port & DATA_MASK) | ((*((byte *)(&g_BackgroundColour))) << 6);
    int bg2 = (port & DATA_MASK) | ((*((byte *)(&g_BackgroundColour)+1)) << 6);

    int fg1 = (port & DATA_MASK) | ((*((byte *)(&g_ForegroundColour))) << 6);
    int fg2 = (port & DATA_MASK) | ((*((byte *)(&g_ForegroundColour)+1)) << 6);

    byte nColMask = 7;
    if ( c < 32 || c > 127 )
    	c = '?';
    uint8_t *pFont = g_pFont->nFontOffsets[(int)c];
    byte row;
    for ( row = 0; row < g_pFont->nCharHeight; row++ )
    {
		byte col, data, bit;
		for ( col = 0; col < g_pFont->nCharWidth; col++ )
		{
			if ( !(col & nColMask) )
			{
				data = *(pFont++);
				bit = 0x80;
			}
			if ( data & bit )
			{
				AVR32_GPIO_LOCAL.port[0].ovr = fg1;
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);

				AVR32_GPIO_LOCAL.port[0].ovr = fg2;
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
			}
			else
			{
				AVR32_GPIO_LOCAL.port[0].ovr = bg1;
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);

				AVR32_GPIO_LOCAL.port[0].ovr = bg2;
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
				AVR32_GPIO_LOCAL.port[0].ovrt = _BV(LCDE);
			}
			bit >>= 1;
		}
    }
}

static  void LCD_SetBackgroundColour( uint16_t colour )
{
    g_BackgroundColour = colour;
}

static  void LCD_SetForegroundColour( uint16_t colour )
{
    g_ForegroundColour = colour;
}

static  void LCD_SetTextCursor( byte x, byte y )
{
    g_TextX = x;
    g_TextY = y;
    g_bPixelCursor = false;
}


static  void LCD_SetPixelCursor( uint16_t x, byte y )
{
    g_PixelX = x;
    g_PixelY = y;
    g_bPixelCursor = true;
}
static  void LCD_SetFont( byte fontId )
{
    g_pFont = fontData + (fontId&1);	// currently only font 0 and 1
}

static  void LCD_SetBacklight( byte intensity )
{
    if ( intensity != 0 )
        gpio_local_set_gpio_pin(LCDBacklightPWM);
    else
        gpio_local_clr_gpio_pin(LCDBacklightPWM);
}


 static void LCD_WriteChar( char c )
{
    if (g_bPixelCursor)
	{
		DrawChar(g_PixelX, g_PixelY, c);
		g_PixelX += g_pFont->nCharWidth;

		if (g_PixelX + g_pFont->nCharWidth >= SCREEN_WIDTH)
		{
			g_PixelX = 0;
			g_PixelY += g_pFont->nCharHeight;
			if (g_PixelY + g_pFont->nCharHeight > SCREEN_HEIGHT)
				g_PixelY = 0;
		}
	}
	else
	{
		uint8_t y = g_TextY;
		y *= g_pFont->nCharHeight;
		uint16_t x = g_TextX;
		x *= g_pFont->nCharWidth;

		DrawChar(x, y, c);

		g_TextX++;
		if (g_TextX >= g_pFont->nCharsPerLine)
		{
			g_TextX = 0;
			g_TextY++;
			if (g_TextY >= g_pFont->nLines)
				g_TextY = 0; // for now, just wrap.  Maybe scroll later.
		}
	}
}

 // Writing a string is a bit faster, especially in text cursor mode (no need to multiply the coordinates)
static void LCD_WriteString( byte nLen, const char *s )
{
	if (g_bPixelCursor)
	{
		while (nLen)
		{
			DrawChar(g_PixelX, g_PixelY, *s);
			s++;
			nLen--;

			g_PixelX += g_pFont->nCharWidth;

			if (g_PixelX + g_pFont->nCharWidth >= SCREEN_WIDTH)
			{
				g_PixelX = 0;
				g_PixelY += g_pFont->nCharHeight;
				if (g_PixelY + g_pFont->nCharHeight > SCREEN_HEIGHT)
					g_PixelY = 0;
			}
		}
	}
	else
	{
		uint8_t y = g_TextY;
		y *= g_pFont->nCharHeight;
		uint16_t x = g_TextX;
		x *= g_pFont->nCharWidth;

		while (nLen)
		{
			DrawChar(x, y, *s);
			s++;
			nLen--;

			g_TextX++;
			if (g_TextX >= g_pFont->nCharsPerLine)
			{
				g_TextX = 0;
				x = 0;

				g_TextY++;
				if (g_TextY >= g_pFont->nLines)
				{
					g_TextY = 0; // for now, just wrap.  Maybe scroll later.
					y = 0;
				}
				else
					y += g_pFont->nCharHeight;
			}
			else
				x += g_pFont->nCharWidth;
		}
	}
}

//void DrawColours(void)
//{
//#define NEW_FONT_WIDTH FONTTYPEWRITER_WIDTH
//#define NEW_FONT_HEIGHT FONTTYPEWRITER_HEIGHT
//
//	int x, x2;
//    for ( x = 0, x2=0; x < 320 - NEW_FONT_WIDTH; x += NEW_FONT_WIDTH, x2++ )
//    {
//    	int y, y2;
//		for ( y = 0, y2 = 0; y < 240 - NEW_FONT_HEIGHT; y += NEW_FONT_HEIGHT, y2++ )
//		{
//			uint16_t c;
//		    if ( y < 60 )
//		    	c = RGB( x*3/4, 0, 0);
//		    else if ( y < 120 )
//		    	c = RGB( 0, x*3/4, 0);
//		    else if ( y < 180 )
//		    	c = RGB( 0, 0, x*3/4);
//		    else
//		    	c = RGB( x*3/4, x*3/4, x*3/4);
//			//c = ((x2+y2) & 1) ? 0 : 0xFFFF;
//			g_ForegroundColour = c;
//
//			LCD_SolidRect( x, y, NEW_FONT_WIDTH, NEW_FONT_HEIGHT );
//		}
//    }
//
// //   for ( int x = 0; x < 320; x++ )
// //   {
//	////for ( int y = 0; y < 240; y += 10 )
//	//{
//	//    uint16_t c;
//	//    uint8_t x2 = 0;
//	//    for ( int i = 0; i < 8; i++ )
//	//	if ( x & (1<<i) )
//	//	    x2 |= 1<<(7-i);
//	//    x2 >> 3;
//	//    c = x2 << 5; //c = RGB( 0, x*3/4, 0);
//
//
//	//    LCD_SolidRect( x, 0, 2, 200, c );
//	//}
// //   }
//}



static void LCD_Init(void)
{
    cDispWrCmd(0x07);
    cDispWrDat(0x00);
    cDispWrDat(0x33);


    cDispWrCmd(0x28);
    cDispWrDat(0x00);
    cDispWrDat(0x06);

    cDispWrCmd(0x2F);
    cDispWrDat(0x12);
    cDispWrDat(0xAE);

    cDispWrCmd(0x00);	//Start Oscillation
    cDispWrDat(0x00);
    cDispWrDat(0x01);

    cDispWrCmd(0x03);	//Power Control (1)
    cDispWrDat(0xAA);
    cDispWrDat(0xAC);

    cDispWrCmd(0x0C);	//Power Control (2)
    cDispWrDat(0x00);
    cDispWrDat(0x02);

    cDispWrCmd(0x0D);	//Power Control (3)
    cDispWrDat(0x00);
    cDispWrDat(0x0A);

    cDispWrCmd(0x0E);	//Power Control (4)
    cDispWrDat(0x2C);
    cDispWrDat(0x00);

    cDispWrCmd(0x1E);	//Power Control (5)
    cDispWrDat(0x00);
    cDispWrDat(0xAA);

    cDispWrCmd(0x25);	//Frame Freq
    cDispWrDat(0x80);
    cDispWrDat(0x00);
    delay_ms(15);

    cDispWrCmd(0x01);	//Driver Output Control	(233f)
    cDispWrDat(0x29);
    cDispWrDat(0x3F);

    cDispWrCmd(0x02);	//LCD Drive AC Control
    cDispWrDat(0x06);
    cDispWrDat(0x00);

    cDispWrCmd(0x10);	//Exit Sleep Mode
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    cDispWrCmd(0x11);	//Set Display color mode for 262k color	? 64k
    cDispWrDat(0x48);
    cDispWrDat(0x38);

    delay_ms(20);

    cDispWrCmd(0x05);
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    cDispWrCmd(0x06);
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    cDispWrCmd(0x16);   // Horizontal Porch
    cDispWrDat(0xEF);
    cDispWrDat(0x1C);

    cDispWrCmd(0x17);   // Vertical Porch
    cDispWrDat(0x00);
    cDispWrDat(0x03);

    cDispWrCmd(0x07);	//Display Control
    cDispWrDat(0x02);
    cDispWrDat(0x33);

    cDispWrCmd(0x0B);	// Frame Cycle Control
    cDispWrDat(0x53);
    cDispWrDat(0x12);

    cDispWrCmd(0x0F);	// Gate Scan Start position
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    cDispWrCmd(0x23);
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    cDispWrCmd(0x24);
    cDispWrDat(0x00);
    cDispWrDat(0x00);

    delay_ms(30);		//delay 30ms

    cDispWrCmd(0x44);	//Horizontal RAM address position start/end setup
    cDispWrDat(0xEF);	//dec 239
    cDispWrDat(0x00);	//dec 0, i.e. horizontal ranges from 0 -> 239
    //POR value is 0xEF00 anyway. This address must be set before RAM write

    cDispWrCmd(0x45);	//Vertical RAM address start position setting
    cDispWrDat(0x00);	//0x0000 = dec 0
    cDispWrDat(0x00);

    cDispWrCmd(0x46);	//Vertical RAM address end position setting
    cDispWrDat(0x01);	//0x013F = dec 319
    cDispWrDat(0x3F);

}

#define SPI_RX_BUFFER_SIZE  256
#ifndef SPI_RX_BUFFER_SIZE
	#error SPI_RX_BUFFER_SIZE is not defined		/* 2,4,8,16,32,64,128 or 256 bytes */
#endif
#define SPI_RX_BUFFER_MASK ( SPI_RX_BUFFER_SIZE - 1 )
#if ( SPI_RX_BUFFER_SIZE & SPI_RX_BUFFER_MASK )
	#error RX buffer size is not a power of 2
#endif

static byte SPI_RxBuf[SPI_RX_BUFFER_SIZE];
static volatile byte SPI_RxHead;
static volatile byte SPI_RxTail;
static volatile bool SPI_BufferOverflow;

__attribute__((__interrupt__)) static void spi_int_handler(void)
{
	if ( AVR32_SPI.SR.rdrf )
	{
		byte data;
		byte tmphead;

		data = AVR32_SPI.RDR.rd & 0xFF;		    /* Read the received data */

		/* Calculate buffer index */
		tmphead = ( SPI_RxHead + 1 ) & SPI_RX_BUFFER_MASK;
		SPI_RxHead = tmphead;      /* Store new index */

		if ( tmphead == SPI_RxTail )
		{
			/* ERROR! Receive buffer overflow */
			SPI_BufferOverflow = true;
		}
		else
		{
			SPI_RxBuf[tmphead] = data; /* Store received data in buffer */
		}
	}
	else
	{
		AVR32_SPI.CR.swrst = 1;
		AVR32_SPI.csr0 = AVR32_SPI_CSR0_BITS_8_BPT | AVR32_SPI_CSR0_NCPHA_MASK;
	    AVR32_SPI.ier = _BV(AVR32_SPI_IER_RDRF) | _BV(AVR32_SPI_IER_NSSR);
	    AVR32_SPI.CR.spien = 1;
	}
}

static byte spi_ReadByte( void )
{
    byte tmptail;

    while ( SPI_RxHead == SPI_RxTail )  /* Wait for incomming data */
	;
    tmptail = ( SPI_RxTail + 1 ) & SPI_RX_BUFFER_MASK;/* Calculate buffer index */

    SPI_RxTail = tmptail;                /* Store new index */

    return SPI_RxBuf[tmptail];           /* Return data */
}

static bool spi_ReadDataAvailable( void )
{
    return ( SPI_RxHead != SPI_RxTail ); /* Return 0 (FALSE) if the receive buffer is empty */
}

static void spi_init(void)
{
    SPI_RxHead = 0;
    SPI_RxTail = 0;
    SPI_BufferOverflow = false;

    // Set up the SPI pins
    gpio_enable_module_pin( AVR32_SPI_MISO_0_0_PIN, AVR32_SPI_MISO_0_0_FUNCTION );
    gpio_enable_module_pin( AVR32_SPI_MOSI_0_0_PIN, AVR32_SPI_MOSI_0_0_FUNCTION );
    gpio_enable_module_pin( AVR32_SPI_SCK_0_0_PIN, AVR32_SPI_SCK_0_0_FUNCTION );
    gpio_enable_module_pin( AVR32_SPI_NPCS_0_0_PIN, AVR32_SPI_NPCS_0_0_FUNCTION );

    Disable_global_interrupt();
    INTC_init_interrupts();
    INTC_register_interrupt(&spi_int_handler, AVR32_SPI_IRQ, AVR32_INTC_INT0);
    Enable_global_interrupt();

    spi_initSlave( &AVR32_SPI,  8, 0 );	// 8 bit slave
    AVR32_SPI.CSR0.cpol = 0;
    AVR32_SPI.CSR0.ncpha = 1;
    AVR32_SPI.MR.modfdis = 1;

    spi_enable( &AVR32_SPI );

    AVR32_SPI.IER.rdrf = 1;
    AVR32_SPI.IER.nssr = 1;
}


int main()
{
	ioinit();

    g_BackgroundColour = 0;
    g_ForegroundColour = 0xFFFF;
    g_TextX = 0;
    g_TextY = 0;
    g_PixelX = 0;
    g_PixelY = 0;
    g_bPixelCursor = false;
    g_pFont = fontData;

    int f, i;
    for ( f = 0; f < 2; f++ )
    	for ( i = 0; i < 128; i++ )
    		if ( i < FIRST_CHAR || i > FIRST_CHAR + CHAR_COUNT )
    			fontData[f].nFontOffsets[i] = 0;
    		else
    			fontData[f].nFontOffsets[i] = fontData[f].pFont + (i-FIRST_CHAR) * ((fontData[f].nCharWidth>>3)+1) * fontData[f].nCharHeight;

    gpio_local_set_gpio_pin(LCDReset);
    delay_ms(20);
    gpio_local_clr_gpio_pin(LCDReset);
    delay_ms(100);
    gpio_local_set_gpio_pin(LCDReset);
    delay_ms(100);

    LCD_Init();

    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_ClearScreen();
    LCD_SetFont( 0 );
    LCD_SetTextCursor(0,0);
    LCD_SetBacklight( 50 );

//	LCD_WriteString( 1, " " );
#if 0
	LCD_SetBackgroundColour( RGB(0xFF,0xFF,0xFF) );
	LCD_SetForegroundColour( RGB(0,0,0) );
	LCD_WriteString( 14, "Black on White" );

	LCD_SetBackgroundColour( RGB(0xFF,0,0) );
	LCD_SetForegroundColour( RGB(0,0,0) );
	LCD_WriteString( 14, "Black on Red  " );

	LCD_SetBackgroundColour( RGB(0,0xFF,0) );
	LCD_SetForegroundColour( RGB(0,0,0) );
	LCD_WriteString( 14, "Black on Green" );

	LCD_SetBackgroundColour( RGB(0,0,0xFF) );
	LCD_SetForegroundColour( RGB(0,0,0) );
	LCD_WriteString( 14, "Black on Blue " );

	LCD_SetBackgroundColour( RGB(0,0,0) );
	LCD_SetForegroundColour( RGB(0xFF,0,0) );
	LCD_WriteString( 14, "Red on Black  " );

	LCD_SetBackgroundColour( RGB(0,0,0) );
	LCD_SetForegroundColour( RGB(0,0xFF,0) );
	LCD_WriteString( 14, "Green on Black" );

	LCD_SetBackgroundColour( RGB(0,0,0) );
	LCD_SetForegroundColour( RGB(0,0,0xFF) );
	LCD_WriteString( 14, "Blue on Black " );

	LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
	LCD_SolidRect(0,50,50,50);
	LCD_SetForegroundColour( RGB(0xFF,0,0) );
	LCD_SolidRect(50,50,50,50);
	LCD_SetForegroundColour( RGB(0,0xFF,0) );
	LCD_SolidRect(100,50,50,50);
	LCD_SetForegroundColour( RGB(0,0,0xFF) );
	LCD_SolidRect(150,50,50,50);
	LCD_SetForegroundColour( RGB(0,0,0) );
	LCD_SolidRect(200,50,50,50);
#endif

//    delay_ms(500);
//    LCD_SetBackgroundColour( RGB(255,0,0) );
//    LCD_ClearScreen();
//    delay_ms(500);
//    LCD_SetBackgroundColour( RGB(0,255,0) );
//    LCD_ClearScreen();
//    delay_ms(500);
//    LCD_SetBackgroundColour( RGB(0,0,255) );
//    LCD_ClearScreen();
//    delay_ms(500);


////#ifdef TEST_COMMANDS
//
//    //delay_ms(1000);
//    LCD_WriteString( 14, "This is a test" );
//
//
//    //delay_ms(1000);
//    LCD_SetForegroundColour( RGB(0xFF,0,0) );
//    LCD_WriteString( 12, "Another test" );
//
//    //delay_ms(1000);
//    LCD_SetBackgroundColour( RGB(0xFF,0xFF,0xFF) );
//    LCD_SetForegroundColour( RGB(0,0,0) );
//    LCD_WriteString( 44, "the quick brown fox jumps over the lazy dog." );
//
//    //delay_ms(1000);
//    LCD_SetBackgroundColour( RGB(0,0,0) );
//    LCD_SetForegroundColour( RGB(0xFF,0,0xFF) );
//    LCD_WriteString( 44, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG." );
//
//    //delay_ms(1000);
//    LCD_SetBackgroundColour( RGB(0,0,0) );
//    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
//    LCD_SetTextCursor( 0, 4 );
//    LCD_SetFont( 1 );
//    LCD_WriteString( 44, "The quick brown fox jumps over the lazy dog." );
//    LCD_SetFont( 1 );
//    for ( i = 0; i < 1000; i++ )
//    {
//    	//delay_ms(1000);
//    	LCD_WriteString( 44, "The quick brown fox jumps over the lazy dog." );
//
//    }
//#define SPLASH
#ifdef SPLASH
	{
		int width = 5, height = 5;
		int x, x2;
		for ( x = 0, x2=0; x < 320 - width; x += width, x2++ )
		{
			int y, y2;
			for ( y = 0, y2 = 0; y < 240 - height; y += height, y2++ )
			{
				uint16_t c;
				if ( y < 60 )
					c = RGB( x*3/4, 0, 0);
				else if ( y < 120 )
					c = RGB( 0, x*3/4, 0);
				else if ( y < 180 )
					c = RGB( 0, 0, x*3/4);
				else
					c = RGB( x*3/4, x*3/4, x*3/4);
				g_ForegroundColour = c;
				LCD_SolidRect( x, y, width, height );
			}
		}
	}
    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );

    LCD_SetFont(0);
    LCD_SetTextCursor(0,0);
#endif

//#define SPEED_TEST
#ifdef SPEED_TEST
	{
		unsigned int nStart = Get_sys_count();
		int i;
		for ( i = 2; i < 240; i++ )
		{
			int width = i, height = i;
			int x, x2;
		    for ( x = 0, x2=0; x < 320 - width; x += width, x2++ )
		    {
		    	int y, y2;
				for ( y = 0, y2 = 0; y < 240 - height; y += height, y2++ )
				{
					uint16_t c;
				    if ( y < 60 )
				    	c = RGB( x*3/4, 0, 0);
				    else if ( y < 120 )
				    	c = RGB( 0, x*3/4, 0);
				    else if ( y < 180 )
				    	c = RGB( 0, 0, x*3/4);
				    else
				    	c = RGB( x*3/4, x*3/4, x*3/4);
					g_ForegroundColour = c;
					LCD_SolidRect( x, y, width, height );
				}
		    }
		}
		unsigned int nEnd1 = Get_sys_count();

	    LCD_SetBackgroundColour( RGB(0,0,0) );
	    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );

	    LCD_SetFont(1);
	    LCD_SetTextCursor(0,0);
		for ( i = 0; i < 250; i++ )
		{
			LCD_WriteString( 44, "The rain in spain falls mainly on the plain ");
		}

	    LCD_SetFont(0);
	    LCD_SetTextCursor(0,0);
		for ( i = 0; i < 250; i++ )
		{
			LCD_WriteString( 44, "The rain in spain falls mainly on the plain ");
		}

		unsigned int nEnd2 = Get_sys_count();

		double Time1 = (double)(nEnd1 - nStart) / (double)CLOCK;
		double Time2 = (double)(nEnd2 - nEnd1) / (double)CLOCK;

		char sBuf[100];
		LCD_ClearScreen();
		LCD_SetTextCursor(0,0);
		snprintf( sBuf, sizeof(sBuf), "LCD_SolidRect Test=%.3f", Time1);
		LCD_WriteString( strlen(sBuf), sBuf );
		LCD_SetTextCursor(0,1);
		snprintf( sBuf, sizeof(sBuf), "LCD_WriteText Test=%.3f", Time2);
		LCD_WriteString( strlen(sBuf), sBuf );

	}
#endif

    spi_init();

	enum
	{
		WAIT_FOR_COMMAND, WAIT_FOR_BYTES, WAIT_FOR_TEXT
	};

	byte state = WAIT_FOR_COMMAND;
	byte buffer[MAX_CMD_LEN];
	byte nWaitBytes=0;
	byte nBufPtr=0;
	unsigned int nLastTime = Get_sys_count();
	for (;;)
	{
		if (spi_ReadDataAvailable())
		{
			byte b = spi_ReadByte();
//#define DEBUG_COMMS
#ifdef DEBUG_COMMS
			static int n = 0;
			n++;
			if ( ( n % (765/3) ) == 0 )
			{
				byte x = n/(765/3);
				LCD_SetBackgroundColour( RGB((x&4)?255:0,(x&2)?255:0,(x&1)?255:0) );
			}
			byte b1 = b >> 4;
			b1 = b1 > 9 ? (b1+'A'-10) : (b1+'0');
			LCD_WriteChar( b1 );
			b1 = b & 0xF;
			b1 = b1 > 9 ? (b1+'A'-10) : (b1+'0');
			LCD_WriteChar( b1 );
			b1 = ' ';
			LCD_WriteChar( b1 );
#else
			switch (state)
			{
			case WAIT_FOR_COMMAND:
				if (b >= CMD_MIN && b <= CMD_MAX)
				{
					nBufPtr = 0;
					buffer[nBufPtr++] = b;
					state = WAIT_FOR_BYTES;
					switch (b)
					{
					case CMD_SET_BACKGROUND_COLOUR: // Set Background Colour 0x00, MSB, LSB
						nWaitBytes = sizeof(CmdSetBackgroundColour) - 1;
						break;
					case CMD_SET_FOREGROUND_COLOUR: // Set Foreground Colour 0x01, MSB, LSB
						nWaitBytes = sizeof(CmdSetForegroundColour) - 1;
						break;
					case CMD_CLEAR_SCREEN: // Clear Screen 0x02,
						LCD_ClearScreen();
						state = WAIT_FOR_COMMAND;
						break;
					case CMD_WRITE_TEXT: // Write Text - 0x03, data..., NULL,
						state = WAIT_FOR_TEXT;
						break;
					case CMD_SET_TEXT_POSITION: // Set position 0x04, x, y
						nWaitBytes = sizeof(CmdSetTextPosition) - 1;
						break;
					case CMD_SET_GRAPHICS_POSITION: // Set position 0x0?, xmsb, xlsb, y
						nWaitBytes = sizeof(CmdSetGraphicsPosition) - 1;
						break;
					case CMD_SET_FONT: // Set font? 0x05, 0/1
						nWaitBytes = sizeof(CmdSetFont) - 1;
						break;
					case CMD_SET_BACKLIGHT: // set backlight 0x06, 0-100
						nWaitBytes = sizeof(CmdSetBacklight) - 1;
						break;
					case CMD_SET_TEXT_ATTRIBUTE: // set attribute 0x07, [underline][Reverse?bg/fg?][italic][bold][strikethrough]
						nWaitBytes = sizeof(CmdSetAttribute) - 1;
						break;
					case CMD_DRAW_RECTANGLE:
						nWaitBytes = sizeof(CmdDrawRectangle) - 1;
						break;
					default:
						state = WAIT_FOR_COMMAND;
						break;
					}
				}
				break;

			case WAIT_FOR_BYTES:
				buffer[nBufPtr++] = b;
				nWaitBytes--;
				if (nWaitBytes == 0)
				{
					switch (buffer[0])
					{
					case CMD_SET_BACKGROUND_COLOUR: // Set Background Colour 0x00, LSB, MSB
						LCD_SetBackgroundColour(
								SwapBytes(((CmdSetBackgroundColour *) buffer)->color));
						break;
					case CMD_SET_FOREGROUND_COLOUR: // Set Foreground Colour 0x01, LSB, MSB
						LCD_SetForegroundColour(
								SwapBytes(((CmdSetForegroundColour *) buffer)->color));
						break;
					case CMD_SET_TEXT_POSITION: // Set position 0x04, x, y
						LCD_SetTextCursor( ((CmdSetTextPosition *) buffer)->x,
										   ((CmdSetTextPosition *) buffer)->y);
						break;
					case CMD_SET_GRAPHICS_POSITION: // Set position 0x0?, xlsb, xmsb, y
						LCD_SetPixelCursor(
								SwapBytes( ((CmdSetGraphicsPosition *) buffer)->x ),
								((CmdSetGraphicsPosition *) buffer)->y);
						break;
					case CMD_SET_FONT: // Set font? 0x05, 0/1
						LCD_SetFont(((CmdSetFont *) buffer)->font);
						break;
					case CMD_SET_BACKLIGHT: // set backlight 0x06, 0-100
						LCD_SetBacklight(
								((CmdSetBacklight *) buffer)->intensity);
						break;
					case CMD_SET_TEXT_ATTRIBUTE: // set attribute 0x07, [underline][Reverse?bg/fg?][italic][bold][strikethrough]
						break;
					case CMD_DRAW_RECTANGLE:
						if (((CmdDrawRectangle *) buffer)->fill)
							LCD_SolidRect( SwapBytes( ((CmdDrawRectangle *) buffer)->x ),
										   ((CmdDrawRectangle *) buffer)->y,
										   SwapBytes(((CmdDrawRectangle *) buffer)->width),
										   ((CmdDrawRectangle *) buffer)->height);
						else
							LCD_Rect( SwapBytes(((CmdDrawRectangle *) buffer)->x),
									  ((CmdDrawRectangle *) buffer)->y,
									  SwapBytes(((CmdDrawRectangle *) buffer)->width),
									  ((CmdDrawRectangle *) buffer)->height );

						break;
					}
					state = WAIT_FOR_COMMAND;
				}
				break;
			case WAIT_FOR_TEXT:
				if (b == 0)
					state = WAIT_FOR_COMMAND;
				else
				{
					if (spi_ReadDataAvailable())
					{
						// we can grab more bytes from the buffer and write them as
						// as string while they are arriving.  Writing a string is a bit faster.
						char buf[32];
						byte nBufLen = sizeof(buf);
						byte n = 0;
						buf[n++] = b;
						do
						{
							b = spi_ReadByte();
							if (b == 0)
							{
								state = WAIT_FOR_COMMAND;
								break;
							}
							buf[n++] = b;
						} while (spi_ReadDataAvailable() && n < nBufLen);

						LCD_WriteString(n, buf);
					}
					else
						LCD_WriteChar(b);
				}
				break;
			}
#endif
		}
		else
		{
			// check for a time out. If we haven't got anything for 100ms, go back to wait state
			unsigned int nTime = Get_sys_count();
			if ( nTime - nLastTime > CLOCK/10 )
				state = WAIT_FOR_COMMAND;
			nLastTime = nTime;
		}
	}
}



/*
    - SPI slave
	- interrupt driven, 128/256 byte buffer
	- do we output?  Buffer len?
    - Commands
*/

