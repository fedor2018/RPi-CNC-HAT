/*
 * dfu-programmer
 *
 * $Id: atmel.h 85 2009-04-30 09:46:27Z schmidtw $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */
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


#ifndef __ATMEL_H__
#define __ATMEL_H__

#include <stdio.h>
#include "inttypes.h"
#include "dfu-device.h"

#define ATMEL_ERASE_BLOCK_0     0
#define ATMEL_ERASE_BLOCK_1     1
#define ATMEL_ERASE_BLOCK_2     2
#define ATMEL_ERASE_BLOCK_3     3
#define ATMEL_ERASE_ALL         4

#define ATMEL_SET_CONFIG_BSB    0
#define ATMEL_SET_CONFIG_SBV    1
#define ATMEL_SET_CONFIG_SSB    2
#define ATMEL_SET_CONFIG_EB     3
#define ATMEL_SET_CONFIG_HSB    4

/* All values are valid if in the range of 0-255, invalid otherwise */
typedef struct {
    int16_t bootloaderVersion;  // Bootloader Version
    int16_t bootID1;            // Device boot ID 1
    int16_t bootID2;            // Device boot ID 2
    int16_t bsb;                // Boot Status Byte
    int16_t sbv;                // Software Boot Vector
    int16_t ssb;                // Software Security Byte
    int16_t eb;                 // Extra Byte
    int16_t manufacturerCode;   // Manufacturer Code
    int16_t familyCode;         // Family Code
    int16_t productName;        // Product Name
    int16_t productRevision;    // Product Revision
    int16_t hsb;                // Hardware Security Byte
} atmel_device_info_t;

typedef struct {
    int32_t lock;               // Locked region
    int32_t epfl;               // External Privileged fetch lock
    int32_t bootprot;           // Bootloader protected area
    int32_t bodlevel;           // Brown-out detector trigger level
    int32_t bodhyst;            // BOD hysteresis enable
    int32_t boden;              // BOD enable state
    int32_t isp_bod_en;         // Tells the ISP to enable BOD
    int32_t isp_io_cond_en;     // ISP uses User page to launch bootloader
    int32_t isp_force;          // Start the ISP no matter what
} atmel_avr32_fuses_t;

int32_t atmel_read_config( dfu_device_t *device,
                           atmel_device_info_t *info );
int32_t atmel_read_fuses( dfu_device_t *device,
                          atmel_avr32_fuses_t * info );
int32_t atmel_erase_flash( dfu_device_t *device,
                           const uint8_t mode );
int32_t atmel_set_fuse( dfu_device_t *device,
                          const uint8_t property,
                          const uint32_t value );
int32_t atmel_set_config( dfu_device_t *device,
                          const uint8_t property,
                          const uint8_t value );
int32_t atmel_read_flash( dfu_device_t *device,
                          const uint32_t start,
                          const uint32_t end,
                          uint8_t* buffer,
                          const size_t buffer_len,
                          const bool eeprom,
                          const bool user,
			  void (*callback)( void *, int ),
			  void *user_data );
int32_t atmel_blank_check( dfu_device_t *device,
                           const uint32_t start,
                           const uint32_t end,
			   void (*callback)( void *, int ),
			   void *user_data );
int32_t atmel_reset( dfu_device_t *device );
int32_t atmel_flash( dfu_device_t *device,
                     int16_t *buffer,
                     const uint32_t start,
                     const uint32_t end,
                     const size_t flash_page_size,
                     const bool eeprom,
		     void (*callback)( void *, int ),
		     void *user_data );
int32_t atmel_user( dfu_device_t *device,
                    int16_t *buffer,
                    const uint32_t end );
int32_t atmel_start_app( dfu_device_t *device, uint16_t addr );

void atmel_print_device_info( FILE *stream, atmel_device_info_t *info );
#endif
