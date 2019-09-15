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

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <avr/eeprom.h>
#include <avr/wdt.h> 

#include "Common.h"
#include "displaycommands.h"

//#include "font.h"
//#include "Font7x5.h"
#include "Font6x13.h"
//#include "font3.h"
#include "FontTypewriter.h"

//#define LED_PWM

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240

#define TEXT_WIDTH			(SCREEN_WIDTH/8-1)
#define TEXT_HEIGHT			(SCREEN_HEIGHT/8-1)

#define sign(x) ((x) < 0 ? -1 : 1 )


#define DATAIN_PORT		PORTB
#define DATAIN_PIN		PINB
#define DATAIN_DDR		DDRB

#define CTRLIN_PORT		PORTE
#define CTRLIN_PIN		PINE
#define CTRLIN_DDR		DDRE
#define CTRLIN_E3		PE7
#define CTRLIN_E2		PE6
#define CTRLIN_E1		PE5
#define CTRLIN_RW		PE4
#define CTRLIN_RS		PE2

#define LED_ENABLE		PE3

#define DATAOUT_PORT		PORTA
#define DATAOUT_PIN		PINA
#define DATAOUT_DDR		DDRA

#define CTRLOUT_PORT		PORTF
#define CTRLOUT_PIN		PINF
#define CTRLOUT_DDR		DDRF
#define CTRLOUT_E		PF0
#define CTRLOUT_RW		PF1
#define CTRLOUT_DC		PF2
#define CTRLOUT_RESET		PF3



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
//    { NEW_FONT_WIDTH, NEW_FONT_HEIGHT, SCREEN_WIDTH / NEW_FONT_WIDTH, SCREEN_HEIGHT / NEW_FONT_HEIGHT, new_Font },
//    { FONT7X5_WIDTH, FONT7X5_HEIGHT, SCREEN_WIDTH / FONT7X5_WIDTH, SCREEN_HEIGHT / FONT7X5_HEIGHT, Font7x5 },
/*45x17*/    { FONT6X13_WIDTH, FONT6X13_HEIGHT, SCREEN_WIDTH / FONT6X13_WIDTH, SCREEN_HEIGHT / FONT6X13_HEIGHT, Font6x13 },
//    { NEW3_FONT_WIDTH, NEW3_FONT_HEIGHT, SCREEN_WIDTH / NEW3_FONT_WIDTH, SCREEN_HEIGHT / NEW3_FONT_HEIGHT, new3_Font },
/*22x9 */    { FONTTYPEWRITER_WIDTH, FONTTYPEWRITER_HEIGHT, SCREEN_WIDTH / FONTTYPEWRITER_WIDTH, SCREEN_HEIGHT / FONTTYPEWRITER_HEIGHT, FontTypewriter },
};


static void ioinit (void)
{
    ///////////////////////
    // Set IO registers	
    ///////////////////////
    DDRB = 0;
    DDRE = INPUT(CTRLIN_E3) | INPUT(CTRLIN_E2) | INPUT(CTRLIN_E1) | INPUT(CTRLIN_RW) | INPUT(CTRLIN_RS) | OUTPUT(LED_ENABLE);
    PORTE = 0;
    DDRA = 0;
    DDRF = OUTPUT(CTRLOUT_E) | OUTPUT(CTRLOUT_RW) | OUTPUT(CTRLOUT_DC) | OUTPUT(CTRLOUT_RESET);
    CTRLOUT_PORT = 0;

    // we only ever output on the data bus
    DATAOUT_DDR = 0xFF;

    // PWM output on OCR3A PE3 (backlight)
#ifdef LED_PWM
    OCR3A = 0;
    ICR3 = 100;
    TCCR3A = _BV(COM3A1) | _BV(WGM31);	// mode 10, phase correct pwm, set OC3A on match
    TCCR3B = _BV(WGM33) | _BV(CS30);			// clk/8
    TCCR3C = 0;
#endif

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
static inline void cDispWrCmd(uint8_t cmd)
{
    DATAOUT_PORT = cmd;
    CTRLOUT_PORT &= ~( _BV(CTRLOUT_RW) | _BV(CTRLOUT_DC));
    CTRLOUT_PORT |= _BV(CTRLOUT_E);
    CTRLOUT_PORT &= ~_BV(CTRLOUT_E);
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
static inline void cDispWrDat(uint8_t dat)
{
    DATAOUT_PORT = dat;
    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);
    CTRLOUT_PORT |= _BV(CTRLOUT_E);
    CTRLOUT_PORT &= ~_BV(CTRLOUT_E);
}

static inline void SetWriteDataMode( void )
{
    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);
}

static inline void WriteData( uint8_t data )
{
    DATAOUT_PORT = data; 
    CTRLOUT_PORT |= _BV(CTRLOUT_E);
    CTRLOUT_PORT &= ~_BV(CTRLOUT_E);
}

// We work with a horizontal display and use xy values which make sense to the orientation.
// Therefore we map our xy to the device xy
static inline void LCD_SetXY( int x, byte y ) // 0..239, 0..319
{
    cDispWrCmd(0x4F);	// set device Y
    cDispWrDat(x >> 8);
    cDispWrDat(x & 0xFF);

    cDispWrCmd(0x4E);	// set device X
    cDispWrDat(0);
    cDispWrDat(y);
}

static inline void LCD_SetWindow( uint16_t x1, byte y1, uint16_t x2, byte y2 )
{
    cDispWrCmd(0x44);	//Horizontal RAM address position start/end setup
    cDispWrDat(y2);
    cDispWrDat(y1);
					    
    cDispWrCmd(0x45);	//Vertical RAM address start position setting
    cDispWrDat(x1>>8);
    cDispWrDat(x1&0xFF);

    cDispWrCmd(0x46);	//Vertical RAM address end position setting
    cDispWrDat(x2>>8);
    cDispWrDat(x2&0xFF);
}

static void LCD_SolidRect( int x, byte y, int nWidth, byte nHeight )
{
    uint16_t c = g_ForegroundColour;

    byte b1 = *(((uint8_t *)&c)+1);
    byte b2 = *(((uint8_t *)&c)+0);

    LCD_SetWindow( x, y, x+nWidth-1, y+nHeight-1 );
    LCD_SetXY( x, y );
    cDispWrCmd(0x22);
    uint32_t n = (uint32_t)nHeight * (uint32_t)nWidth;
    uint16_t nCount = (n>>1)+1;

    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);

    byte bHigh = CTRLOUT_PORT | _BV(CTRLOUT_E);
    byte bLow = CTRLOUT_PORT & ~_BV(CTRLOUT_E);

    while ( nCount-- )
    {
	// 4 byte, 2 pixels at a time
	DATAOUT_PORT = b1;
	CTRLOUT_PORT = bHigh;
	CTRLOUT_PORT = bLow;
	DATAOUT_PORT = b2;
	CTRLOUT_PORT = bHigh;
	CTRLOUT_PORT = bLow;

	DATAOUT_PORT = b1;
	CTRLOUT_PORT = bHigh;
	CTRLOUT_PORT = bLow;
	DATAOUT_PORT = b2;
	CTRLOUT_PORT = bHigh;
	CTRLOUT_PORT = bLow;
    }
}

static void LCD_Rect( int x, byte y, int nWidth, byte nHeight )
{
    LCD_SolidRect( x, y, nWidth, 1 );
    LCD_SolidRect( x, y, 1, nHeight );
    LCD_SolidRect( x+nWidth-1, y, 1, nHeight );
    LCD_SolidRect( x, y+nHeight-1, nWidth, 1 );
}

//void DrawColours(void)
//{
//
//    for ( int x = 0, x2=0; x < 320 - NEW_FONT_WIDTH; x += NEW_FONT_WIDTH, x2++ )
//    {
//	for ( int y = 0, y2 = 0; y < 240 - NEW_FONT_HEIGHT; y += NEW_FONT_HEIGHT, y2++ )
//	{
//	    uint16_t c;
//	 //   if ( y < 60 )
//		//c = RGB( x*3/4, 0, 0);
//	 //   else if ( y < 120 )
//		//c = RGB( 0, x*3/4, 0);
//	 //   else if ( y < 180 )
//		//c = RGB( 0, 0, x*3/4);
//	 //   else 
//		//c = RGB( x*3/4, x*3/4, x*3/4);
//	    c = ((x2+y2) & 1) ? 0 : 0xFFFF;
//
//	    LCD_SolidRect( x, y, NEW_FONT_WIDTH, NEW_FONT_HEIGHT, c );
//	}
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


// clear the screen.  We paint every pixel, so we don't 
// have to worry about setting x,y to 0.  We just wrap.
static void LCD_ClearScreen( void )
{
    LCD_SetWindow( 0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1 );
    cDispWrCmd(0x22);	// data mode

    byte b2 = *((byte *)(&g_BackgroundColour));
    byte b1 = *((byte *)(&g_BackgroundColour) + 1);

    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);

    byte bHigh = CTRLOUT_PORT | _BV(CTRLOUT_E);
    byte bLow = CTRLOUT_PORT & ~_BV(CTRLOUT_E);
    uint16_t k=(240U/2U)*320U;
    byte k1 = k >> 8;
    byte k2 = 0xFF;

    if ( b1 == b2 )
    {
	DATAOUT_PORT = b1; 
	while ( k1-- )	// the nested loops is to optimise the code - 1 byte variable checks, not 2
	    do
	    {
		// 4 byte, 2 pixels at a time - so we can fit 320x240 into an int16t
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
	    }
	    while ( k2-- );
    }
    else
    {
	while ( k1-- )	// the nested loops is to optimise the code - 1 byte variable checks, not 2
	    do
	    {
		// 4 byte, 2 pixels at a time - so we can fit 320x240 into an int16t
		DATAOUT_PORT = b1; 
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		DATAOUT_PORT = b2; 
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		DATAOUT_PORT = b1; 
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
		DATAOUT_PORT = b2; 
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
	    }
	    while ( k2-- );
    }	
}


static void DrawText( byte nLen, const char *s )
{
    uint16_t y = g_TextY;
    y *= g_pFont->nCharHeight;
    uint16_t x = g_TextX;
    x *= g_pFont->nCharWidth;
    uint16_t width = nLen;
    width *= g_pFont->nCharWidth;

    LCD_SetWindow( x, y, x+width-1, y+g_pFont->nCharHeight-1 );
    LCD_SetXY( x, y );
    cDispWrCmd(0x22);

    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);

    byte bHigh = CTRLOUT_PORT | _BV(CTRLOUT_E);
    byte bLow = CTRLOUT_PORT & ~_BV(CTRLOUT_E);

    byte bg2 = *((byte *)(&g_BackgroundColour));
    byte bg1 = *((byte *)(&g_BackgroundColour) + 1);

    byte fg2 = *((byte *)(&g_ForegroundColour));
    byte fg1 = *((byte *)(&g_ForegroundColour) + 1);

    byte nColMask = 7;
    for ( byte row = 0; row < g_pFont->nCharHeight; row++ )
    {
	byte nFontOffset = (byte)row * (byte)((g_pFont->nCharWidth+7)/8);
	const char *ptr = s;
	byte nCount = nLen;
	while ( nCount-- )
	{
	    char c = *ptr++;

	    uint8_t * PROGMEM pFont = g_pFont->nFontOffsets[(byte)c] +  + nFontOffset;

	    byte data, bit;
	    for ( byte col = 0; col < g_pFont->nCharWidth; col++ )
	    {
		if ( (col & nColMask) == 0 )
		{
		    data = pgm_read_byte( pFont++ );
		    bit = 0x80;
		}
		if ( data & bit )
		{
		    DATAOUT_PORT = fg1;
		    CTRLOUT_PORT = bHigh;
		    CTRLOUT_PORT = bLow;

		    DATAOUT_PORT = fg2;
		    CTRLOUT_PORT = bHigh;
		    CTRLOUT_PORT = bLow;
		}
		else
		{
		    DATAOUT_PORT = bg1;
		    CTRLOUT_PORT = bHigh;
		    CTRLOUT_PORT = bLow;

		    DATAOUT_PORT = bg2;
		    CTRLOUT_PORT = bHigh;
		    CTRLOUT_PORT = bLow;
		}
		bit >>= 1;
	    }
	}
    }
}


static void DrawChar( uint16_t x, uint8_t y, char c )
{
    LCD_SetWindow( x, y, x+g_pFont->nCharWidth-1, y+g_pFont->nCharHeight-1 );
    LCD_SetXY( x, y );
    cDispWrCmd(0x22);

    CTRLOUT_PORT &= ~_BV(CTRLOUT_RW);
    CTRLOUT_PORT |= _BV(CTRLOUT_DC);

    byte bHigh = CTRLOUT_PORT | _BV(CTRLOUT_E);
    byte bLow = CTRLOUT_PORT & ~_BV(CTRLOUT_E);

    byte bg2 = *((byte *)(&g_BackgroundColour));
    byte bg1 = *((byte *)(&g_BackgroundColour) + 1);

    byte fg2 = *((byte *)(&g_ForegroundColour));
    byte fg1 = *((byte *)(&g_ForegroundColour) + 1);

    byte nColMask = 7;
    //uint16_t nOffset = (byte)((byte)c - (byte)FIRST_CHAR);
    //nOffset *= (byte)(g_pFont->nCharWidth>>3)+(byte)1;
    //nOffset *= g_pFont->nCharHeight;
    //uint8_t * PROGMEM pFont = g_pFont->pFont + nOffset;
    uint8_t * PROGMEM pFont = g_pFont->nFontOffsets[(byte)c];
    for ( byte row = 0; row < g_pFont->nCharHeight; row++ )
    {
	byte data, bit;
	for ( byte col = 0; col < g_pFont->nCharWidth; col++ )
	{
	    if ( !(col & nColMask) )
	    {
		data = pgm_read_byte( pFont++ );
		bit = 0x80;
	    }
	    if ( data & bit )
	    {
		DATAOUT_PORT = fg1;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;

		DATAOUT_PORT = fg2;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
	    }
	    else
	    {
		DATAOUT_PORT = bg1;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;

		DATAOUT_PORT = bg2;
		CTRLOUT_PORT = bHigh;
		CTRLOUT_PORT = bLow;
	    }
	    bit >>= 1;
	}
    }
}

static inline void LCD_SetBackgroundColour( uint16_t colour )
{
    g_BackgroundColour = colour;
}

static inline void LCD_SetForegroundColour( uint16_t colour )
{
    g_ForegroundColour = colour;
}

static inline void LCD_SetTextCursor( byte x, byte y )
{
    g_TextX = x;
    g_TextY = y;
    g_bPixelCursor = false;
}


static inline void LCD_SetPixelCursor( uint16_t x, byte y )
{
    g_PixelX = x;
    g_PixelY = y;
    g_bPixelCursor = true;
}
static inline void LCD_SetFont( byte fontId )
{
    g_pFont = fontData + (fontId&1);	// currently only font 0 and 1
}

static inline void LCD_SetBacklight( byte intensity )
{
#ifdef LED_PWM
    if ( intensity >= 100 )
	intensity = 100;
    OCR3A = intensity;
 //   if ( intensity >= 50 )
	//OCR3A = 0;
 //   else
	//OCR3A = 50 - intensity;
#else
    if ( intensity != 0 )
	PORTE |= _BV(LED_ENABLE);
    else
	PORTE &= ~_BV(LED_ENABLE);
#endif
}

//static void LCD_WriteText( byte nLen, const char *s )
//{
//    while ( nLen )
//    {
//	// text will wrap
//	byte nSpaceOnLine = g_pFont->nCharsPerLine - g_TextX;
//	if ( nLen < nSpaceOnLine )
//	    nSpaceOnLine = nLen;
//
//	DrawText( nSpaceOnLine, s );
//
//	nLen -= nSpaceOnLine;
//	s += nSpaceOnLine;
//
//	g_TextX += nSpaceOnLine;
//	if ( g_TextX >= g_pFont->nCharsPerLine )
//	{
//	    g_TextX = 0;
//	    g_TextY++;
//	    if ( g_TextY >= g_pFont->nLines )
//		g_TextY = 0; // for now, just wrap.  Maybe scroll later. 
//	}
//    }
//}


//// to do - we only ever write one char at a time.
//// support pixel positioning;
//static void LCD_WriteText2( byte nLen, const char *s )
//{
//    while ( nLen )
//    {
//	DrawChar( *s );
//	s++;
//	nLen--;
//
//	g_TextX++;
//	if ( g_TextX >= g_pFont->nCharsPerLine )
//	{
//	    g_TextX = 0;
//	    g_TextY++;
//	    if ( g_TextY >= g_pFont->nLines )
//		g_TextY = 0; // for now, just wrap.  Maybe scroll later. 
//	}
//    }
//}

 static void LCD_WriteChar( char c )
{
    if ( g_bPixelCursor )
    {
	DrawChar( g_PixelX, g_PixelY, c );
	g_PixelX += g_pFont->nCharWidth;

	if ( g_PixelX + g_pFont->nCharWidth >= SCREEN_WIDTH )
	{
	    g_PixelX = 0;
	    g_PixelY += g_pFont->nCharHeight;
	    if ( g_PixelY + g_pFont->nCharHeight > SCREEN_HEIGHT )
		g_PixelY = 0;
	}
    }
    else
    {
	uint8_t y = g_TextY;
	y *= g_pFont->nCharHeight;
	uint16_t x = g_TextX;
	x *= g_pFont->nCharWidth;

	DrawChar( x, y, c );

	g_TextX++;
	if ( g_TextX >= g_pFont->nCharsPerLine )
	{
	    g_TextX = 0;
	    g_TextY++;
	    if ( g_TextY >= g_pFont->nLines )
		g_TextY = 0; // for now, just wrap.  Maybe scroll later. 
	}
    }
}

 // Writing a string is a bit faster, especially in text cursor mode (no need to multiply the coordinates)
static void LCD_WriteString( byte nLen, const char *s )
{
    if ( g_bPixelCursor )
    {
	while ( nLen )
	{
	    DrawChar( g_PixelX, g_PixelY, *s );
	    s++;
	    nLen--;


	    g_PixelX += g_pFont->nCharWidth;

	    if ( g_PixelX + g_pFont->nCharWidth >= SCREEN_WIDTH )
	    {
		g_PixelX = 0;
		g_PixelY += g_pFont->nCharHeight;
		if ( g_PixelY + g_pFont->nCharHeight > SCREEN_HEIGHT )
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


	while ( nLen )
	{
	    DrawChar( x, y, *s );
	    s++;
	    nLen--;

	    g_TextX++;
	    if ( g_TextX >= g_pFont->nCharsPerLine )
	    {
		g_TextX = 0;
		x = 0;

		g_TextY++;
		if ( g_TextY >= g_pFont->nLines )
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
    _delay_ms(15);

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

    _delay_ms(20);

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

    _delay_ms(30);		//delay 30ms

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

SIGNAL (SIG_SPI)
{
    byte data;
    byte tmphead;

    data = SPDR;		    /* Read the received data */
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

    DDRB |= _BV(PB3);	// MISO output
    SPCR = _BV(SPIE) | _BV(SPE); 
}

int main (void)
{
//    byte reason = MCUSR;
    MCUSR = 0;
    wdt_disable();

    ioinit();

    g_BackgroundColour = 0;
    g_ForegroundColour = 0xFFFF;
    g_TextX = 0;
    g_TextY = 0;
    g_pFont = fontData;

    for ( int f = 0; f < 2; f++ )
	for ( int i = 0; i < 128; i++ )
	    if ( i < FIRST_CHAR || i > FIRST_CHAR + CHAR_COUNT )
		fontData[f].nFontOffsets[i] = 0;
	    else
		fontData[f].nFontOffsets[i] = fontData[f].pFont + (i-FIRST_CHAR) * ((fontData[f].nCharWidth>>3)+1) * fontData[f].nCharHeight;

    CTRLOUT_PORT |= _BV(CTRLOUT_RESET);
    _delay_ms(20);
    CTRLOUT_PORT &= ~_BV(CTRLOUT_RESET);
    _delay_ms(100);
    CTRLOUT_PORT |= _BV(CTRLOUT_RESET);
    _delay_ms(100);

    LCD_Init();

    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_ClearScreen();
    LCD_SetTextCursor( 0, 0 );
    LCD_SetFont( 0 );
    LCD_SetBacklight( 50 );

#ifdef TEST_COMMANDS
    LCD_WriteText2( 14, "This is a test" );


    LCD_SetForegroundColour( RGB(0xFF,0,0) );
    LCD_WriteText2( 12, "Another test" );

    LCD_SetBackgroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_SetForegroundColour( RGB(0,0,0) );
    LCD_WriteText2( 44, "the quick brown fox jumps over the lazy dog." );

    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0,0xFF) );
    LCD_WriteText2( 44, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG." );

    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_SetTextCursor( 0, 4 );
    LCD_SetFont( 1 );
    LCD_WriteText2( 44, "The quick brown fox jumps over the lazy dog." );



    LCD_SetFont( 0 );
    //for ( int i = 0; i < 1000; i++ )
	LCD_WriteText( 44, "The quick brown fox jumps over the lazy dog." );

	//- Set Background Colour 0x00, MSB, LSB
	//- Set Foreground Colour 0x01, MSB, LSB
	//- Clear Screen 0x02,
	//- Write Text 
	//    - 0x03, len(?), data..., or
	//    - 0x03, data, 0
	//- Set position 0x04, x, y
	//- set font? 0x05, 0/1
	//- set backlight 0x06, 0-255

 //   cDispWrCmd(0x22);	//RAM write command

 //   cDispClr(0x3f3F3F);	//initialize the LCD in white color for 24-bit color mode

 //   DrawColours();
 //   ShowText( 1, 3, "The Quick Brown Fox! Mmm!" );
 //   for ( int i = 0; i < 320/NEW_FONT_WIDTH; i++ )
 //   {
	//char buf[2];
	//buf[0] = '0' + ((i+1)%10);
	//buf[1] = 0;
	//ShowText( i, 0, buf );
 //   }
 //   for ( int i = 1; i < 240/NEW_FONT_HEIGHT; i++ )
 //   {
	//char buf[10];
	//itoa(i+1,buf,10);
	//ShowText( 0, i, buf );
 //   }

 //   ShowText( 5, 10, "X: " );
 //   int n1=0, n2=0;
 //   for (;;)
 //   {
	//char buf1[20];
	//char buf2[20];
	//itoa( n1, buf1, 10 );
	//itoa( n2, buf2, 10 );

	//while ( strlen(buf2) < 3 )
	//{
	//    char buf3[20];
	//    strcpy( buf3, "0" );
	//    strcat( buf3, buf2 );
	//    strcpy( buf2, buf3 );
	//}
	//strcat( buf1, "." );
	//strcat( buf1, buf2 );
	//strcat( buf1, "    " );

	//ShowText( 8, 10, buf1 );

	//n2+=3;
	//if ( n2 >= 1000 )
	//{
	//    n2 -= 1000;
	//    n1++;
	//}
	////_delay_ms(100);
 //   }

 //   for ( int i = 0; i < 100; i++ )
 //   {
	//LCD_SetBacklight( i );
	//_delay_ms(100);

 //   }

    LCD_ClearScreen();
    LCD_SetBackgroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_SetForegroundColour( RGB(0,0,0) );
    LCD_SetFont( 0 );
    LCD_SetTextCursor(0, 16 );
    LCD_WriteText2( 10, "    Up    " );

    LCD_SetTextCursor(11, 16 );
    LCD_WriteText2( 10, "   Down   " );

    LCD_SetTextCursor(22, 16 );
    LCD_WriteText2( 10, "   Left   " );


    LCD_SetTextCursor(33, 16 );
    LCD_WriteText2( 10, "   Right  " );

    LCD_SetBackgroundColour( RGB(0,0,0) );
    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_SetFont(1);
    LCD_SetTextCursor(0,2);
    LCD_WriteText2(11, "X:    9.372" );

    LCD_SetForegroundColour( RGB(0x80,0x80,0x80) );
    LCD_SetTextCursor(0,3);
    LCD_WriteText2(11, "Z:  123.123" );

    LCD_SetForegroundColour( RGB(0xFF,0xFF,0xFF) );
    LCD_SetTextCursor(0,4);
    LCD_WriteText2(4, "SPI:" );
#endif


    spi_init();

    enum 
    {
	WAIT_FOR_COMMAND,
	WAIT_FOR_BYTES,
	WAIT_FOR_TEXT
    };

    byte state = WAIT_FOR_COMMAND;
    byte buffer[MAX_CMD_LEN];
    byte nWaitBytes;
    byte nBufPtr;
    sei();
    for (;;)
    {
	if ( spi_ReadDataAvailable() )
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
	    switch ( state )
	    {
		case WAIT_FOR_COMMAND:
		    if ( b >= CMD_MIN && b <= CMD_MAX )
		    {
			nBufPtr = 0;
			buffer[nBufPtr++] = b;
			state = WAIT_FOR_BYTES;
			switch ( b )
			{
			    case CMD_SET_BACKGROUND_COLOUR:	    // Set Background Colour 0x00, MSB, LSB
				nWaitBytes = sizeof(CmdSetBackgroundColour)-1;
				break;
			    case CMD_SET_FOREGROUND_COLOUR:	    // Set Foreground Colour 0x01, MSB, LSB
				nWaitBytes = sizeof(CmdSetForegroundColour)-1;
				break;
			    case CMD_CLEAR_SCREEN:		    // Clear Screen 0x02,
				LCD_ClearScreen();
				state = WAIT_FOR_COMMAND;
				break;
			    case CMD_WRITE_TEXT:		    // Write Text - 0x03, data..., NULL, 
				state = WAIT_FOR_TEXT;
				break;
			    case CMD_SET_TEXT_POSITION:		    // Set position 0x04, x, y
				nWaitBytes = sizeof(CmdSetTextPosition)-1;
				break;
			    case CMD_SET_GRAPHICS_POSITION:	    // Set position 0x0?, xmsb, xlsb, y
				nWaitBytes = sizeof(CmdSetGraphicsPosition)-1;
				break;
			    case CMD_SET_FONT:			    // Set font? 0x05, 0/1
				nWaitBytes = sizeof(CmdSetFont)-1;
				break;
			    case CMD_SET_BACKLIGHT:		    // set backlight 0x06, 0-100
				nWaitBytes = sizeof(CmdSetBacklight)-1;
				break;
			    case CMD_SET_TEXT_ATTRIBUTE:	    // set attribute 0x07, [underline][Reverse?bg/fg?][italic][bold][strikethrough]
				nWaitBytes = sizeof(CmdSetAttribute)-1;
				break;
			    case CMD_DRAW_RECTANGLE:
				nWaitBytes = sizeof(CmdDrawRectangle)-1;
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
		    if ( nWaitBytes == 0 )
		    {
			switch ( buffer[0] )
			{
			    case CMD_SET_BACKGROUND_COLOUR:	    // Set Background Colour 0x00, LSB, MSB
				LCD_SetBackgroundColour( ((CmdSetBackgroundColour *)buffer)->color );
				break;
			    case CMD_SET_FOREGROUND_COLOUR:	    // Set Foreground Colour 0x01, LSB, MSB
				LCD_SetForegroundColour( ((CmdSetForegroundColour *)buffer)->color );
				break;
			    case CMD_SET_TEXT_POSITION:		    // Set position 0x04, x, y
				LCD_SetTextCursor( ((CmdSetTextPosition *)buffer)->x, ((CmdSetTextPosition *)buffer)->y );
				break;
			    case CMD_SET_GRAPHICS_POSITION:	    // Set position 0x0?, xlsb, xmsb, y
				LCD_SetPixelCursor( ((CmdSetGraphicsPosition *)buffer)->x, ((CmdSetGraphicsPosition *)buffer)->y );
				break;
			    case CMD_SET_FONT:			    // Set font? 0x05, 0/1
				LCD_SetFont( ((CmdSetFont *)buffer)->font );
				break;
			    case CMD_SET_BACKLIGHT:		    // set backlight 0x06, 0-100
				LCD_SetBacklight( ((CmdSetBacklight *)buffer)->intensity );
				break;
			    case CMD_SET_TEXT_ATTRIBUTE:	    // set attribute 0x07, [underline][Reverse?bg/fg?][italic][bold][strikethrough]
				break;
			    case CMD_DRAW_RECTANGLE:
				if ( ((CmdDrawRectangle *)buffer)->fill )
				    LCD_SolidRect( ((CmdDrawRectangle *)buffer)->x, ((CmdDrawRectangle *)buffer)->y, 
						   ((CmdDrawRectangle *)buffer)->width, ((CmdDrawRectangle *)buffer)->height );
				else
				    LCD_Rect( ((CmdDrawRectangle *)buffer)->x, ((CmdDrawRectangle *)buffer)->y, 
					      ((CmdDrawRectangle *)buffer)->width, ((CmdDrawRectangle *)buffer)->height );

				break;
			}
			state = WAIT_FOR_COMMAND;
		    }
		    break;
		case WAIT_FOR_TEXT:
		    if ( b == 0 )
			state = WAIT_FOR_COMMAND;
		    else
		    {
			if ( spi_ReadDataAvailable() )
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
				if ( b == 0 )
				{
				    state = WAIT_FOR_COMMAND;
				    break;
				}
				buf[n++] = b;
			    } 
			    while ( spi_ReadDataAvailable() && n < nBufLen );

			    LCD_WriteString( n, buf );
			}
			else
			    LCD_WriteChar( b );
		    }
		    break;
	    }
#endif
	}
    }
}



/*
    - SPI slave
	- interrupt driven, 128/256 byte buffer
	- do we output?  Buffer len?
    - Commands
*/

