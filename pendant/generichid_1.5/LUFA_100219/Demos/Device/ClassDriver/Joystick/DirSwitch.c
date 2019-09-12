// generichid, DIY HID device 
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
#include "Controls.h"
#include "DirSwitch.h"
#include "Serial.h"
#include "Joystick.h"

#include <string.h>


static bool bInitialised = false;			// first time through we need to initialise everything

void InitDirSwitch( struct SDirSwitchControl *pData )
{
    if ( !bInitialised )
    {
    }

    for ( byte i = 0; i < pData->Ports; i++ )
    {
        byte nPort = GET_PORT_ID(pData->Switches[i].Port);
        byte nPin = GET_PORT_PIN(pData->Switches[i].Port);

        // Set DDR - set port to input
        SetDDRBit( nPort, nPin, DDR_INPUT );
        if ( pData->Options & _BV(DSW_PULLUP) )
            SetPORTBit( nPort, nPin, 1 );	// use pullup
        else
            SetPORTBit( nPort, nPin, 0 );

        pData->Switches[i].Debounce.key_down = 0;
        pData->Switches[i].Debounce.start_time = milliseconds_counter;
    }

    bInitialised = true;
}


void ReadDirSwitch( struct SDirSwitchControl *pData, byte **ReportBuffer, byte *nBit )
{
    byte nOnMask = 0 ;
    byte nOnSwitch = 0;

    for ( byte i = 0; i < pData->Ports; i++ )
    {
        byte key_down = GetIOBit( pData->Switches[i].Port );

        if ( pData->Options & _BV(DSW_PULLUP) )	// Pullup inverts button presses.
            key_down = 1 - key_down;

        if ( pData->DebounceMs )
        {
            if ( key_down == pData->Switches[i].Debounce.key_down )
            {
                pData->Switches[i].Debounce.start_time = milliseconds_counter;
            }
            else
            {
                unsigned char diff = milliseconds_counter - pData->Switches[i].Debounce.start_time;
                if ( diff >= pData->DebounceMs )
                {
                    pData->Switches[i].Debounce.key_down = key_down;
                    pData->Switches[i].Debounce.start_time = milliseconds_counter;
                }
            }
            key_down = pData->Switches[i].Debounce.key_down;
        }

        if ( key_down )
        {
            nOnMask |= 1 << i;
            nOnSwitch = i+1;
        }
    }

    // decode what the buttons mean
    if ( pData->Ports == 2 )
    {
        // Only one switch in a 2 way switch can be on
        PackData16( ReportBuffer, nBit, nOnSwitch, 2 );
    }
    else if ( pData->Ports == 4 )
    {
        // We support diagonal movement on 4-way switches, for example, NE if N and E are pressed.
        switch ( nOnMask )
        {
            case _BV(DIRSW_PORTN):			nOnSwitch = 1; break;
            case _BV(DIRSW_PORTN) | _BV(DIRSW_PORTE):	nOnSwitch = 2; break;
            case _BV(DIRSW_PORTE):			nOnSwitch = 3; break;
            case _BV(DIRSW_PORTS) | _BV(DIRSW_PORTE):	nOnSwitch = 4; break;
            case _BV(DIRSW_PORTS):			nOnSwitch = 5; break;
            case _BV(DIRSW_PORTS) | _BV(DIRSW_PORTW):	nOnSwitch = 6; break;
            case _BV(DIRSW_PORTW):			nOnSwitch = 7; break;
            case _BV(DIRSW_PORTN) | _BV(DIRSW_PORTW):	nOnSwitch = 8; break;
        }

        PackData16( ReportBuffer, nBit, nOnSwitch, 4 );
    }
    else
    {
        // Only one switch in a 8 way switch can be on
        switch ( nOnSwitch-1 )
        {
            case DIRSW_PORTN:		nOnSwitch = 1; break;
            case DIRSW_PORTNE:		nOnSwitch = 2; break;
            case DIRSW_PORTE:		nOnSwitch = 3; break;
            case DIRSW_PORTSE:		nOnSwitch = 4; break;
            case DIRSW_PORTS:		nOnSwitch = 5; break;
            case DIRSW_PORTSW:		nOnSwitch = 6; break;
            case DIRSW_PORTW:		nOnSwitch = 7; break;
            case DIRSW_PORTNW:		nOnSwitch = 8; break;
        }

        PackData16( ReportBuffer, nBit, nOnSwitch, 4 );
    }
}

