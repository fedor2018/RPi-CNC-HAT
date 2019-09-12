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


#include "Common.h"
#include "Serial.h"
#include <stdlib.h>


/* UART Buffer Defines */ 
#ifdef RS2321_TX
 
	#ifndef UART1_TX_BUFFER_SIZE
		#error UART1_TX_BUFFER_SIZE is not defined		/* 2,4,8,16,32,64,128 or 256 bytes */ 
	#endif
	#define UART1_TX_BUFFER_MASK ( UART1_TX_BUFFER_SIZE - 1 )
	#if ( UART1_TX_BUFFER_SIZE & UART1_TX_BUFFER_MASK )
		#error TX buffer size is not a power of 2
	#endif

	static byte UART1_TxBuf[UART1_TX_BUFFER_SIZE];
	static volatile byte UART1_TxHead;
	static volatile byte UART1_TxTail; 

#endif

#ifdef RS2321_RX

	#ifndef UART1_RX_BUFFER_SIZE
		#error UART1_RX_BUFFER_SIZE is not defined		/* 2,4,8,16,32,64,128 or 256 bytes */
	#endif
	#define UART1_RX_BUFFER_MASK ( UART1_RX_BUFFER_SIZE - 1 )
	#if ( UART1_RX_BUFFER_SIZE & UART1_RX_BUFFER_MASK )
		#error RX buffer size is not a power of 2
	#endif

	static byte UART1_RxBuf[UART1_RX_BUFFER_SIZE]; 
	static volatile byte UART1_RxHead;
	static volatile byte UART1_RxTail;

#endif
//
////#ifdef USE_UART1
//	#define UDR		UDR1
//	#define UCSRA	UCSR1A
//	#define UCSRB	UCSR1B
//	#define UCSRC	UCSR1C
//	#define UBRRH	UBRR1H
//	#define UBRRL	UBRR1L
//	#define SIG_UART_DATA	SIG_UART1_DATA
//	#define SIG_UART_RECV	SIG_UART1_RECV
////#endif

#if MCU == at90s4433
    #if !defined(UBRRL)
        #define UBRRL   UBRR
    #endif
#endif

#ifdef RS2321_TX

	SIGNAL (SIG_USART1_DATA)	// UART Data ready interrupt
	{ 
		byte tmptail;

		/* Check if all data is transmitted */
		if ( UART1_TxHead != UART1_TxTail )
		{
			/* Calculate buffer index */
			tmptail = ( UART1_TxTail + 1 ) & UART1_TX_BUFFER_MASK;
			UART1_TxTail = tmptail;      /* Store new index */
		
			UDR1 = UART1_TxBuf[tmptail];  /* Start transmition */
		}
		else
		{
			UCSR1B &= ~(1<<UDRIE1);         /* Disable UDRE interrupt */
		}
	}

#endif

#ifdef RS2321_RX

	SIGNAL (SIG_USART1_RECV)	// UART Data ready interrupt
	{ 
		byte data;
		byte tmphead;

		data = UDR1;                 /* Read the received data */
		/* Calculate buffer index */
		tmphead = ( UART1_RxHead + 1 ) & UART1_RX_BUFFER_MASK;
		UART1_RxHead = tmphead;      /* Store new index */ 

		if ( tmphead == UART1_RxTail )
		{
			/* ERROR! Receive buffer overflow */
		}
		
		UART1_RxBuf[tmphead] = data; /* Store received data in buffer */
	}

#endif

void InitRS2321( unsigned short nBaud, bool bTransmit, bool bReceive )
{
#ifdef RS2321_TX
	UART1_TxTail = 0; 
	UART1_TxHead = 0;
#endif

#ifdef RS2321_RX
	UART1_RxTail = 0;
	UART1_RxHead = 0;
#endif

	UBRR1H = nBaud >> 8;
	UBRR1L = nBaud & 0xFF;

	UCSR1A = 0;
	UCSR1B = 0;

	UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);	// 8,n,1

#ifdef RS2321_TX
	if ( bTransmit )
		UCSR1B |=  _BV(TXEN1);
#endif
#ifdef RS2321_RX
	if ( bReceive )
		UCSR1B |=  _BV(RXCIE1) | _BV(RXEN1);
#endif
}

void RS2321SetParity( enum EParity eParity )
{
	register byte oldUCSRC;
	cli();
	oldUCSRC = UCSR1C;
	oldUCSRC = UCSR1C;
	sei();

    switch ( eParity )
    {
        case eOdd:
            UCSR1C = oldUCSRC | _BV(UPM11) | _BV(UPM10);
            break;
        case eEven:
            UCSR1C = (oldUCSRC & ~ (_BV(UPM10))) | _BV(UPM11);
            break;
        case eNone:
        default:
            UCSR1C = (oldUCSRC & ~ (_BV(UPM11) | _BV(UPM10)));
            break;
    }
}

void RS2321SetStopBits( byte nBits )
{
	byte oldUCSRC;
	cli();
	oldUCSRC = UCSR1C;
	oldUCSRC = UCSR1C;
	sei();

    switch ( nBits )
    {
        case 2:
            UCSR1C = oldUCSRC | _BV(USBS1);
            break;
        case 1:
        default:
            UCSR1C = (oldUCSRC & ~ (_BV(USBS1)));
            break;
    }
}

void RS2321SetCharSize( byte nBits )
{
	byte oldUCSRC;
	cli();
	oldUCSRC = UCSR1C;
	oldUCSRC = UCSR1C;
	sei();

    switch ( nBits )
    {
        case 5:
            UCSR1C = (oldUCSRC & ~(_BV(UCSZ11) | _BV(UCSZ10)));
            UCSR1B = UCSR1B & ~(_BV(UCSZ12));
            break;
        case 6:
            UCSR1C = (oldUCSRC & ~(_BV(UCSZ11)))  | _BV(UCSZ10);
            UCSR1B = UCSR1B & ~(_BV(UCSZ12));
            break;
        case 7:
            UCSR1C = (oldUCSRC & ~(_BV(UCSZ10))) | _BV(UCSZ11);
            UCSR1B = UCSR1B & ~(_BV(UCSZ12));
            break;
        case 8:
        default:
            UCSR1C = oldUCSRC | _BV(UCSZ11) | _BV(UCSZ10);
            UCSR1B = UCSR1B & ~(_BV(UCSZ12));
            break;
        case 9:
            UCSR1C = oldUCSRC | _BV(UCSZ11) | _BV(UCSZ10);
            UCSR1B = UCSR1B | _BV(UCSZ12);
            break;
    }
}

#ifdef RS2321_TX
	void RS2321SendChar( byte data )
	{
		unsigned char tmphead;
		/* Calculate buffer index */
		tmphead = ( UART1_TxHead + 1 ) & UART1_TX_BUFFER_MASK; /* Wait for free space in buffer */
		while ( tmphead == UART1_TxTail );

		UART1_TxBuf[tmphead] = data;           /* Store data in buffer */
		UART1_TxHead = tmphead;                /* Store new index */

		UCSR1B |= (1<<UDRIE1);                    /* Enable UDRE interrupt */
	}

	void RS2321Send( char *str )
	{
		while ( *str != 0 )
		{
			RS2321SendChar( *str );
			str++;
		}
	}

	void RS2321Send_P( PGM_P s  )
	{
		char b;
		goto ReadByte;
		while ( b != 0 )
		{
			RS2321SendChar(b );
			s++;
	ReadByte:
			b = pgm_read_byte( s );
		}
	}


	void RS2321SendInt( int v )
	{
		char s[7];
		itoa( v, s, 10 );
		RS2321Send( s );
	}

	void RS2321SendLong( long v )
	{
		char s[11];
		ltoa( v, s, 10 );
		RS2321Send( s );
	}

	void RS2321SendHex( byte v )
	{
		byte b = v >> 4;
		if ( b < 10 )
			RS2321SendChar( '0' + b );
		else
			RS2321SendChar( 'A' + b - 10 );
		b = v & 0xF;
		if ( b < 10 )
			RS2321SendChar( '0' + b );
		else
			RS2321SendChar( 'A' + b - 10 );
	}

	void RS2321SendCRLF( void )
	{
		RS2321SendChar( '\r' );
		RS2321SendChar( '\n' );
	}
#endif 



#ifdef RS2321_RX

	byte RS2321ReadByte( void )
	{
		byte tmptail;
		
		while ( UART1_RxHead == UART1_RxTail )  /* Wait for incomming data */
			;
		tmptail = ( UART1_RxTail + 1 ) & UART1_RX_BUFFER_MASK;/* Calculate buffer index */
		
		UART1_RxTail = tmptail;                /* Store new index */
		
		return UART1_RxBuf[tmptail];           /* Return data */
	}

	bool RS2321ReadDataAvailable( void )
	{
		return ( UART1_RxHead != UART1_RxTail ); /* Return 0 (FALSE) if the receive buffer is empty */
	}

#endif
