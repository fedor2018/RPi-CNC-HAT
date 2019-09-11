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



#ifndef _UVGA_H_
#define _UVGA_H_

bool uVGA_SetDisplayPage( byte nPage );
bool uVGA_SetReadPage( byte nPage );
bool uVGA_SetWritePage( byte nPage );
bool uVGA_SetBackgroundColour( byte nColour );
bool uVGA_ErasePage(void);
bool uVGA_BufferEmpty(void);
void uVGA_PopMessage( void );
byte uVGA_PeekBuffer( byte nOffset );
bool uVGA_PlaceTextCharacterF( char c, byte nCol, byte nRow, byte nColour );
bool uVGA_PlaceStringFormatted( byte nCol, byte nRow, byte nFont, byte nColour, char *s );
bool uVGA_BlockCopyAndPaste( uint16_t xs, uint16_t ys, uint16_t xd, uint16_t yd, uint16_t width, uint16_t height, byte source_page, byte destination_page );
bool uVGA_FontSize( byte nSize );
bool uVGA_OpaqueTransparentText( bool bOpaque );
bool uVGA_PutPixel( uint16_t xs, uint16_t ys, byte colour );
bool uVGA_DrawLine( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, byte colour );
bool uVGA_TextButton( bool bUp, uint16_t x, uint16_t y, byte ButtonColour, byte Font, byte TextColour, byte TextWidth, byte TextHeight,  char *s );

#define uVGA_ACK	0x06
#define uVGA_NAK	0x15

#define uVGA_COLOUR_WHITE		255
#define uVGA_COLOUR_BLACK		0

#endif
