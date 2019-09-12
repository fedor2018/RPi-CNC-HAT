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
#ifdef RS2320_TX
 
	#ifndef UART0_TX_BUFFER_SIZE
		#error UART0_TX_BUFFER_SIZE is not defined		/* 2,4,8,16,32,64,128 or 256 bytes */ 
	#endif
	#define UART0_TX_BUFFER_MASK ( UART0_TX_BUFFER_SIZE - 1 )
	#if ( UART0_TX_BUFFER_SIZE & UART0_TX_BUFFER_MASK )
		#error TX buffer size is not a power of 2
	#endif

	static byte UART0_TxBuf[UART0_TX_BUFFER_SIZE];
	static volatile byte UART0_TxHead;
	static volatile byte UART0_TxTail; 

#endif

#ifdef RS2320_RX

	#ifndef UART0_RX_BUFFER_SIZE
		#error UART0_RX_BUFFER_SIZE is not defined		/* 2,4,8,16,32,64,128 or 256 bytes */
	#endif
	#define UART0_RX_BUFFER_MASK ( UART0_RX_BUFFER_SIZE - 1 )
	#if ( UART0_RX_BUFFER_SIZE & UART0_RX_BUFFER_MASK )
		#error RX buffer size is not a power of 2
	#endif

	static byte UART0_RxBuf[UART0_RX_BUFFER_SIZE]; 
	static volatile byte UART0_RxHead;
	static volatile byte UART0_RxTail;

#endif
//
////#ifdef USE_UART1
//	#define UDR		UDR0
//	#define UCSRA	UCSR0A
//	#define UCSRB	UCSR0B
//	#define UCSRC	UCSR0C
//	#define UBRRH	UBRR0H
//	#define UBRRL	UBRR0L
//	#define SIG_UART_DATA	SIG_UART0_DATA
//	#define SIG_UART_RECV	SIG_UART0_RECV
////#endif

#if MCU == at90s4433
    #if !defined(UBRRL)
        #define UBRRL   UBRR
    #endif
#endif

#ifdef RS2320_TX

	SIGNAL (SIG_USART0_DATA)	// UART Data ready interrupt
	{ 
		byte tmptail;

		/* Check if all data is transmitted */
		if ( UART0_TxHead != UART0_TxTail )
		{
			/* Calculate buffer index */
			tmptail = ( UART0_TxTail + 1 ) & UART0_TX_BUFFER_MASK;
			UART0_TxTail = tmptail;      /* Store new index */
		
			UDR0 = UART0_TxBuf[tmptail];  /* Start transmition */
		}
		else
		{
			UCSR0B &= ~(1<<UDRIE0);         /* Disable UDRE interrupt */
		}
	}

#endif

#ifdef RS2320_RX

	SIGNAL (SIG_USART0_RECV)	// UART Data ready interrupt
	{ 
		byte data;
		byte tmphead;

		data = UDR0;                 /* Read the received data */
		/* Calculate buffer index */
		tmphead = ( UART0_RxHead + 1 ) & UART0_RX_BUFFER_MASK;
		UART0_RxHead = tmphead;      /* Store new index */ 

		if ( tmphead == UART0_RxTail )
		{
			/* ERROR! Receive buffer overflow */
		}
		
		UART0_RxBuf[tmphead] = data; /* Store received data in buffer */
	}

#endif

void InitRS2320( unsigned short nBaud, bool bTransmit, bool bReceive )
{
#ifdef RS2320_TX
	UART0_TxTail = 0; 
	UART0_TxHead = 0;
#endif

#ifdef RS2320_RX
	UART0_RxTail = 0;
	UART0_RxHead = 0;
#endif

	UBRR0H = nBaud >> 8;
	UBRR0L = nBaud & 0xFF;

	UCSR0A = 0;
	UCSR0B = 0;

	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);	// 8,n,1

#ifdef RS2320_TX
	if ( bTransmit )
		UCSR0B |=  _BV(TXEN0);
#endif
#ifdef RS2320_RX
	if ( bReceive )
		UCSR0B |=  _BV(RXCIE0) | _BV(RXEN0);
#endif
}

void RS2320SetParity( enum EParity eParity )
{
	register byte oldUCSRC;
	cli();
	oldUCSRC = UCSR0C;
	oldUCSRC = UCSR0C;
	sei();

    switch ( eParity )
    {
        case eOdd:
            UCSR0C = oldUCSRC | _BV(UPM01) | _BV(UPM00);
            break;
        case eEven:
            UCSR0C = (oldUCSRC & ~ (_BV(UPM00))) | _BV(UPM01);
            break;
        case eNone:
        default:
            UCSR0C = (oldUCSRC & ~ (_BV(UPM01) | _BV(UPM00)));
            break;
    }
}

void RS2320SetStopBits( byte nBits )
{
	byte oldUCSRC;
	cli();
	oldUCSRC = UCSR0C;
	oldUCSRC = UCSR0C;
	sei();

    switch ( nBits )
    {
        case 2:
            UCSR0C = oldUCSRC | _BV(USBS0);
            break;
        case 1:
        default:
            UCSR0C = (oldUCSRC & ~ (_BV(USBS0)));
            break;
    }
}

void RS2320SetCharSize( byte nBits )
{
	byte oldUCSRC;
	cli();
	oldUCSRC = UCSR0C;
	oldUCSRC = UCSR0C;
	sei();

    switch ( nBits )
    {
        case 5:
            UCSR0C = (oldUCSRC & ~(_BV(UCSZ01) | _BV(UCSZ00)));
            UCSR0B = UCSR0B & ~(_BV(UCSZ02));
            break;
        case 6:
            UCSR0C = (oldUCSRC & ~(_BV(UCSZ01)))  | _BV(UCSZ00);
            UCSR0B = UCSR0B & ~(_BV(UCSZ02));
            break;
        case 7:
            UCSR0C = (oldUCSRC & ~(_BV(UCSZ00))) | _BV(UCSZ01);
            UCSR0B = UCSR0B & ~(_BV(UCSZ02));
            break;
        case 8:
        default:
            UCSR0C = oldUCSRC | _BV(UCSZ01) | _BV(UCSZ00);
            UCSR0B = UCSR0B & ~(_BV(UCSZ02));
            break;
        case 9:
            UCSR0C = oldUCSRC | _BV(UCSZ01) | _BV(UCSZ00);
            UCSR0B = UCSR0B | _BV(UCSZ02);
            break;
    }
}

#ifdef RS2320_TX
	void RS2320SendChar( byte data )
	{
		unsigned char tmphead;
		/* Calculate buffer index */
		tmphead = ( UART0_TxHead + 1 ) & UART0_TX_BUFFER_MASK; /* Wait for free space in buffer */
		while ( tmphead == UART0_TxTail );

		UART0_TxBuf[tmphead] = data;           /* Store data in buffer */
		UART0_TxHead = tmphead;                /* Store new index */

		UCSR0B |= (1<<UDRIE0);                    /* Enable UDRE interrupt */
	}

	void RS2320Send( char *str )
	{
		while ( *str != 0 )
		{
			RS2320SendChar( *str );
			str++;
		}
	}

	void RS2320Send_P( PGM_P s  )
	{
		char b;
		goto ReadByte;
		while ( b != 0 )
		{
			RS2320SendChar(b );
			s++;
	ReadByte:
			b = pgm_read_byte( s );
		}
	}


	void RS2320SendInt( int v )
	{
		char s[7];
		itoa( v, s, 10 );
		RS2320Send( s );
	}

	void RS2320SendLong( long v )
	{
		char s[11];
		ltoa( v, s, 10 );
		RS2320Send( s );
	}

	void RS2320SendHex( byte v )
	{
		byte b = v >> 4;
		if ( b < 10 )
			RS2320SendChar( '0' + b );
		else
			RS2320SendChar( 'A' + b - 10 );
		b = v & 0xF;
		if ( b < 10 )
			RS2320SendChar( '0' + b );
		else
			RS2320SendChar( 'A' + b - 10 );
	}

	void RS2320SendCRLF( void )
	{
		RS2320SendChar( '\r' );
		RS2320SendChar( '\n' );
	}
#endif 



#ifdef RS2320_RX

	byte RS2320ReadByte( void )
	{
		byte tmptail;
		
		while ( UART0_RxHead == UART0_RxTail )  /* Wait for incomming data */
			;
		tmptail = ( UART0_RxTail + 1 ) & UART0_RX_BUFFER_MASK;/* Calculate buffer index */
		
		UART0_RxTail = tmptail;                /* Store new index */
		
		return UART0_RxBuf[tmptail];           /* Return data */
	}

	bool RS2320ReadDataAvailable( void )
	{
		return ( UART0_RxHead != UART0_RxTail ); /* Return 0 (FALSE) if the receive buffer is empty */
	}

#endif
