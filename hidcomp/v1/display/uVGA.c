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
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "Common.h"
#include "serial.h"

#include "uVGA.h"

#define MESSAGE_BUFFER_SIZE	256
#define MESSAGE_BUFFER_MASK ( MESSAGE_BUFFER_SIZE - 1 )
#if ( MESSAGE_BUFFER_SIZE & MESSAGE_BUFFER_MASK )
	#error buffer size is not a power of 2
#endif

static byte Buf[MESSAGE_BUFFER_SIZE]; 
static volatile byte Head;
static volatile byte Tail;

static bool Push( byte n )
{
	byte tmphead;

	/* Calculate buffer index */
	tmphead = ( Head + 1 ) & MESSAGE_BUFFER_MASK;

	if ( tmphead == Tail )			// Is there room?
		return false;

	Buf[tmphead] = n;           /* Store data in buffer */
	Head = tmphead;                /* Store new index */

	return true;
}

static byte BufferSpace( void )
{
	return Tail - Head - 1;
}

static bool SendMessage( uint8_t nLen, ... )
{
	va_list argptr;
	va_start(argptr, nLen);

	if ( BufferSpace() < nLen + 1 )
		return false;

	Push(nLen);
	for ( byte i = 0; i < nLen; i++ )
	{
		byte n = va_arg( argptr, int );
		Push( n );
	}
	va_end(argptr);

	return true;
}

void uVGA_PopMessage( void )
{
	byte nLen = 1;
	for ( byte i = 0; i < nLen; i++ )
	{
		Tail = (Tail + 1) & MESSAGE_BUFFER_MASK;
		byte n = Buf[ Tail ];
		if ( i == 0 )
			nLen += n;
	}
}

byte uVGA_PeekBuffer( byte nOffset )
{
	return Buf[ (Tail + 1 + nOffset) & MESSAGE_BUFFER_MASK ];
}


bool uVGA_BufferEmpty(void)
{
	return Head == Tail;
}

bool uVGA_SetDisplayPage( byte nPage )
{
	return SendMessage( 3, 'Y', 0, nPage );
}

bool uVGA_SetReadPage( byte nPage )
{
	return SendMessage( 3, 'Y', 1, nPage );
}

bool uVGA_SetWritePage( byte nPage )
{
	return SendMessage( 3, 'Y', 2, nPage );
}

bool uVGA_SetBackgroundColour( byte nColour )
{
	return SendMessage( 2, 'B', nColour );
}

bool uVGA_FontSize( byte nSize )
{
	return SendMessage( 2, 'F', nSize );
}

bool uVGA_ErasePage(void)
{
	return SendMessage( 1, 'E' );
}

bool uVGA_PlaceTextCharacterF( char c, byte nCol, byte nRow, byte nColour )
{
	return SendMessage( 5, 'T', c, nCol, nRow, nColour );
}


bool uVGA_PlaceStringFormatted( byte nCol, byte nRow, byte nFont, byte nColour, char *s )
{
	byte nStrLen = strlen(s);
	byte nLen = 6 + nStrLen;

	if ( BufferSpace() < nLen + 1)
		return false;

	Push(nLen);
	Push('s');
	Push(nCol);
	Push(nRow);
	Push(nFont);
	Push(nColour);
	for ( byte i = 0; i < nStrLen; i++ )
		Push( s[i] );
	Push(0);

	return true;
}

bool uVGA_OpaqueTransparentText( bool bOpaque )
{
	return SendMessage( 2, 'O', bOpaque?1:0 );
}

bool uVGA_BlockCopyAndPaste( uint16_t xs, uint16_t ys, uint16_t xd, uint16_t yd, uint16_t width, uint16_t height, byte source_page, byte destination_page )
{
	return SendMessage( 15, 
						'c', 
						xs>>8, xs & 0xFF, 
						ys>>8, ys & 0xFF, 
						xd>>8, xd & 0xFF, 
						yd>>8, yd & 0xFF, 
						width>>8, width & 0xFF, 
						height>>8, height & 0xFF, 
						source_page,
						destination_page );
}

bool uVGA_PutPixel( uint16_t xs, uint16_t ys, byte colour )
{
	return SendMessage( 6, 'P', 
						xs>>8, xs & 0xFF, 
						ys>>8, ys & 0xFF, 
						colour );
}


bool uVGA_DrawLine( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, byte colour )
{
	return SendMessage( 10, 'L', 
						x1>>8, x1 & 0xFF, 
						y1>>8, y1 & 0xFF, 
						x2>>8, x2 & 0xFF, 
						y2>>8, y2 & 0xFF, 
						colour );
}


bool uVGA_TextButton( bool bUp, uint16_t x, uint16_t y, byte ButtonColour, byte Font, byte TextColour, byte TextWidth, byte TextHeight,  char *s )
{
	byte nStrLen = strlen(s);
	byte nLen = 11 + nStrLen;

	if ( BufferSpace() < nLen + 1)
		return false;

	Push(nLen);
	Push('b');
	Push(bUp?1:0);
	Push(x>>8);
	Push(x&0xFF);
	Push(y>>8);
	Push(y&0xFF);
	Push(ButtonColour);
	Push(Font);
	Push(TextColour);
	Push(TextWidth);
	Push(TextHeight);
	for ( byte i = 0; i < nStrLen; i++ )
		Push( s[i] );
	Push(0);

	return true;
}
