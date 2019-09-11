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

#ifndef __AVR_SERIAL_H__
#define __AVR_SERIAL_H__
 
enum EParity
{
    eNone,
    eOdd,
    eEven
} ;

extern void InitRS2320( unsigned short nBaud, bool bTransmit, bool bReceive );
extern void RS2320SetParity( enum EParity eParity );
extern void RS2320SetStopBits( byte nBits );
extern void RS2320SetCharSize( byte nBits );

#ifdef RS2320_TX
	extern void RS2320SendChar( byte data );
	extern void RS2320Send( char *str );
	extern void RS2320Send_P( PGM_P s  );
	extern void RS2320SendInt( int v );
	extern void RS2320SendLong( long v );
	extern void RS2320SendHex( byte v );
	extern void RS2320SendCRLF( void ); 
#endif
#ifdef RS2320_RX
	extern byte RS2320ReadByte( void );
	extern bool RS2320ReadDataAvailable( void );
#endif


#endif
