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

#ifndef __AVR_SERIAL1_H__
#define __AVR_SERIAL1_H__
 
#include "serial.h"

extern void InitRS2321( unsigned short nBaud, bool bTransmit, bool bReceive );
extern void RS2321SetParity( enum EParity eParity );
extern void RS2321SetStopBits( byte nBits );
extern void RS2321SetCharSize( byte nBits );

#ifdef RS2321_TX
	extern void RS2321SendChar( byte data );
	extern void RS2321Send( char *str );
	extern void RS2321Send_P( PGM_P s  );
	extern void RS2321SendInt( int v );
	extern void RS2321SendLong( long v );
	extern void RS2321SendHex( byte v );
	extern void RS2321SendCRLF( void ); 
#endif
#ifdef RS2321_RX
	extern byte RS2321ReadByte( void );
	extern bool RS2321ReadDataAvailable( void );
#endif


#endif
