// hidcomp/hidconfig, HID device interface for emc
// Copyright (C) 2008, Frank Tkalcevic, www.franksworkshop.com

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

#ifndef HAL_H1
#define HAL_H1

/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/

/********************************************************************
* Description:  hal.h
*               This file, 'hal.h', defines the API and data 
*               structures used by the HAL
*
* Author: John Kasunich
* License: LGPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/
/* testing */

/** This file, 'hal.h', defines the API and data structures used by
    the HAL.  This file is included in both realtime and non-realtime
    HAL components.  HAL uses the RTPAI real time interface, and the
    #define symbols RTAPI and ULAPI are used to distinguish between
    realtime and non-realtime code.  The API defined in this file
    is implemented in hal_lib.c and can be compiled for linking to
    either realtime or user space HAL components.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This library is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU Lesser General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

/***********************************************************************
*                   GENERAL NOTES AND DOCUMENTATION                    *
************************************************************************/

/** The HAL is a very modular approach to the low level parts of a
    motion control system.  The goal of the HAL is to allow a systems
    integrator to connect a group of software components together to
    meet whatever I/O requirements he (or she) needs.  This includes
    realtime and non-realtime I/O, as well as basic motor control up
    to and including a PID position loop.  What these functions have
    in common is that they all process signals.  In general, a signal
    is a data item that is updated at regular intervals.  For example,
    a PID loop gets position command and feedback signals, and produces
    a velocity command signal.

    HAL is based on the approach used to design electronic circuits.
    In electronics, off-the-shelf components like integrated circuits
    are placed on a circuit board and their pins are interconnected
    to build whatever overall function is needed.  The individual
    components may be as simple as an op-amp, or as complex as a
    digital signal processor.  Each component can be individually
    tested, to make sure it works as designed.  After the components
    are placed in a larger circuit, the signals connecting them can
    still be monitored for testing and troubleshooting.

    Like electronic components, HAL components have pins, and the pins
    can be interconnected by signals.  At some point, I hope to be able
    to use a GPL'ed schematic capture program with a special HAL library
    to draw a "schematic" diagram with interconnected HAL components.
    You would then generate a netlist from the schematic, and the netlist
    would configure the HAL.  For now, however, HAL configuration will be
    done by a utility that parses a text file or works interactively.

    In the HAL, a 'signal' contains the actual data value that passes
    from one pin to another.  When a signal is created, space is allocated
    for the data value.  A 'pin' on the other hand, is a pointer, not a
    data value.  When a pin is connected to a signal, the pin's pointer
    is set to point at the signal's data value.  This allows the component
    to access the signal with very little run-time overhead.  (If a pin
    is not linked to any signal, the pointer points to a dummy location,
    so the realtime code doesn't have to deal with null pointers or treat
    unlinked variables as a special case in any way.)

    There are three approaches to writing a HAL component.  Those
    that do not require hard realtime performance can be written
    as a single user mode process.  Components that need hard realtime
    performance but have simple configuration and init requirements
    can be done as a single kernel module, using either pre-defined
    init info, or insmod-time parameters.  Finally, complex components
    may use both a kernel module for the realtime part, and a user
    space process to handle ini file access, user interface (possibly
    including GUI features), and other details.

    HAL uses the RTAPI/ULAPI interface.  If RTAPI is #defined, (done
    by the makefile) hal_lib.c would generate a kernel module hal_lib.o
    that is insmoded and provides the functions for all kernel module
    based components.  The same source file compiled with the ULAPI
    #define would make a user space hal_lib.o that is staticlly linked
    to user space code to make user space executables.  The variable
    lists and link information are stored in a block of shared memory
    and protected with mutexes, so that kernel modules and any of
    several user mode programs can access the data.

*/

#ifdef __cplusplus 
extern "C" {
#endif


#if ( !defined RTAPI ) && ( !defined ULAPI )
#error HAL needs RTAPI/ULAPI, check makefile and flags
#endif

/** These status codes are returned by many HAL functions. */

#define HAL_SUCCESS       0	/* call successfull */
#define HAL_UNSUP        -1	/* function not supported */
#define HAL_BADVAR       -2	/* duplicate or not-found variable name */
#define HAL_INVAL        -3	/* invalid argument */
#define HAL_NOMEM        -4	/* not enough memory */
#define HAL_LIMIT        -5	/* resource limit reached */
#define HAL_PERM         -6	/* permission denied */
#define HAL_BUSY         -7	/* resource is busy or locked */
#define HAL_NOTFND       -8	/* object not found */
#define HAL_FAIL         -9	/* operation failed */

#define HAL_NAME_LEN     41	/* length for pin, signal, etc, names */

/** These locking codes define the state of HAL locking, are used by most functions */
/** The functions locked will return a HAL_PERM error message **/

#define HAL_LOCK_NONE     0     /* no locking done, any command is permitted */
#define HAL_LOCK_LOAD     1     /* loading rt components is not permitted */
#define HAL_LOCK_CONFIG   2     /* locking of link and addf related commands */
#define HAL_LOCK_PARAMS   4     /* locking of parameter set commands */
#define HAL_LOCK_RUN      8     /* locking of start/stop of HAL threads */

#define HAL_LOCK_ALL      255   /* locks every action */


#ifdef __cplusplus
}
#endif
#endif /* HAL_H */
