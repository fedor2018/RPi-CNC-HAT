/*
 * dfu-programmer
 *
 * $Id: atmel.c 87 2009-05-31 08:01:54Z schmidtw $
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


#include <stdio.h>
#include <stdarg.h>
#include "inttypes.h"
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include "dfucommon.h"
#include "dfu-device.h"
#include "dfu.h"
#include "atmel.h"


/*
 * Atmel's firmware doesn't export a DFU descriptor in its config
 * descriptor, so we have to guess about parameters listed there.
 * We use 3KB for wTransferSize (MAX_TRANSFER_SIZE).
 */

#define ATMEL_MAX_TRANSFER_SIZE     0x0100
#define ATMEL_MAX_FLASH_BUFFER_SIZE (ATMEL_MAX_TRANSFER_SIZE +              \
                                        ATMEL_AVR32_CONTROL_BLOCK_SIZE +    \
                                        ATMEL_AVR32_CONTROL_BLOCK_SIZE +    \
                                        ATMEL_FOOTER_SIZE)

#define ATMEL_FOOTER_SIZE               16
#define ATMEL_CONTROL_BLOCK_SIZE        32
#define ATMEL_AVR32_CONTROL_BLOCK_SIZE  64

#define ATMEL_DEBUG_THRESHOLD   50
#define ATMEL_TRACE_THRESHOLD   55


static int32_t atmel_flash_block( dfu_device_t *device,
                                  int16_t *buffer,
                                  const uint32_t base_address,
                                  const size_t length,
                                  const bool eeprom );
static int32_t atmel_select_flash( dfu_device_t *device );
static int32_t atmel_select_user( dfu_device_t *device );
static int32_t atmel_select_fuses( dfu_device_t *device );
static int32_t atmel_select_page( dfu_device_t *device,
                                  const uint16_t mem_page );
static int32_t __atmel_read_page( dfu_device_t *device,
                                  const uint32_t start,
                                  const uint32_t end,
                                  uint8_t* buffer,
                                  const bool eeprom );

/* returns 0 - 255 on success, < 0 otherwise */
static int32_t atmel_read_command( dfu_device_t *device,
                                   const uint8_t data0,
                                   const uint8_t data1 )
{
    if( adc_AVR32 == device->type ) {
        //We need to talk to configuration memory.  It comes
        //in two varieties in this chip.  data0 is the command to
        //select it
        //Data1 is the byte of that group we want
        
    if( NULL != device ) {
        uint8_t command[4] = { 0x06, 0x03, 0x00, data0 };

        if( 4 != dfu_download(device, 4, command) ) {
            ERROR_MSG( "dfu_download failed.\n" );
            return -1;
        }

        int32_t result;
        uint8_t buffer[1];
        result = __atmel_read_page( device, data1, data1+1, buffer, false );
        if( 1 != result ) {
            return -5;
        }

        return (0xff & buffer[0]);

    } else {

       return -1;
    }
    } else {
    uint8_t command[3] = { 0x05, 0x00, 0x00 };
    uint8_t data[1]    = { 0x00 };
    dfu_status_t status;

    command[1] = data0;
    command[2] = data1;

    DEBUG_MSG( QString("%1( %2, 0x%3, 0x%4 )\n").arg(__FUNCTION__).arg((int)device).arg(data0,2,16,QChar('0')).arg(data1,2,16,QChar('0')) );

    if( 3 != dfu_download(device, 3, command) ) {
        ERROR_MSG( "dfu_download failed\n" );
        return -1;
    }

    if( 0 != dfu_get_status(device, &status) ) {
        ERROR_MSG( "dfu_get_status failed\n" );
        return -2;
    }

    if( DFU_STATUS_OK != status.bStatus ) {
        ERROR_MSG( QString("status(%1) was not OK.\n").arg(dfu_status_to_string(status.bStatus)) );
        return -3;
    }

    if( 1 != dfu_upload(device, 1, data) ) {
        ERROR_MSG( "dfu_upload failed\n" );
        return -4;
    }

    return (0xff & data[0]);
    }
}

int32_t atmel_read_fuses( dfu_device_t *device,
                           atmel_avr32_fuses_t *info )
{
    if( adc_AVR32 != device->type ) {
       ERROR_MSG( "target does not support fuse operation.\n" );
       return -1;
    } 
    
    if( 0 != atmel_select_fuses(device) ) {
        return -3;
    }
    int32_t result;
    uint8_t buffer[32];
    int i;
    result = __atmel_read_page( device, 0, 32, buffer, false );
    if( 32 != result ) {
        return -5;
    }
    info->lock = 0;
    for(i = 0; i < 16; i++) {
        info->lock = info->lock | (buffer[i] << i);
    }
    info->epfl = buffer[16];
    info->bootprot = (buffer[19] << 2) | (buffer[18] << 1) | (buffer[17] << 0);
    info->bodlevel = 0;
    for(i = 20; i < 26; i++) {
        info->bodlevel = info->bodlevel | (buffer[i] << (i-20));
    }
    info->bodhyst = buffer[26];
    info->boden = (buffer[28] << 1) | (buffer[27] << 0);
    info->isp_bod_en = buffer[29];
    info->isp_io_cond_en = buffer[30];
    info->isp_force = buffer[31];

    return 0;
 }
 
/*
 *  This reads in all of the configuration and Manufacturer Information
 *  into the atmel_device_info data structure for easier use later.
 *
 *  device    - the usb_dev_handle to communicate with
 *  info      - the data structure to populate
 *
 *  returns 0 if successful, < 0 if not
 */
int32_t atmel_read_config( dfu_device_t *device,
                           atmel_device_info_t *info )
{
    typedef struct {
        uint8_t data0;
        uint8_t data1;
        uint8_t device_map;
        size_t  offset;
    } atmel_read_config_t;
#   define DM_8051  0x01
#   define DM_AVR   0x02
#   define DM_AVR32 0x04

    /* These commands are documented in Appendix A of the
     * "AT89C5131A USB Bootloader Datasheet" or
     * "AT90usb128x/AT90usb64x USB DFU Bootloader Datasheet"
     */
    static const atmel_read_config_t data[] = {
        { 0x00, 0x00, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, bootloaderVersion) },
        { 0x04, 0x00, (DM_AVR32),         offsetof(atmel_device_info_t, bootloaderVersion) },
        { 0x00, 0x01, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, bootID1)           },
        { 0x04, 0x01, (DM_AVR32),         offsetof(atmel_device_info_t, bootID1)           },
        { 0x00, 0x02, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, bootID2)           },
        { 0x04, 0x02, (DM_AVR32),         offsetof(atmel_device_info_t, bootID2)           },
        { 0x01, 0x30, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, manufacturerCode)  },
        { 0x05, 0x00, (DM_AVR32),         offsetof(atmel_device_info_t, manufacturerCode)  },
        { 0x01, 0x31, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, familyCode)        },
        { 0x05, 0x01, (DM_AVR32),         offsetof(atmel_device_info_t, familyCode)        },
        { 0x01, 0x60, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, productName)       },
        { 0x05, 0x02, (DM_AVR32),         offsetof(atmel_device_info_t, productName)       },
        { 0x01, 0x61, (DM_8051 | DM_AVR), offsetof(atmel_device_info_t, productRevision)   },
        { 0x05, 0x03, (DM_AVR32),         offsetof(atmel_device_info_t, productRevision)   },
        { 0x01, 0x00, DM_8051,            offsetof(atmel_device_info_t, bsb)               },
        { 0x01, 0x01, DM_8051,            offsetof(atmel_device_info_t, sbv)               },
        { 0x01, 0x05, DM_8051,            offsetof(atmel_device_info_t, ssb)               },
        { 0x01, 0x06, DM_8051,            offsetof(atmel_device_info_t, eb)                },
        { 0x02, 0x00, DM_8051,            offsetof(atmel_device_info_t, hsb)               }
    };

    int32_t result;
    int32_t retVal = 0;

    DEBUG_MSG( QString("%1( %2, %3 )\n").arg(__FUNCTION__).arg((int)device).arg((int)info) );

    for( unsigned int i = 0; i < sizeof(data)/sizeof(atmel_read_config_t); i++ ) {
        atmel_read_config_t *row = (atmel_read_config_t*) &data[i];

        if( ((DM_8051 & row->device_map) && (adc_8051 == device->type)) ||
            ((DM_AVR & row->device_map) && (adc_AVR == device->type)) ||
            ((DM_AVR32 & row->device_map) && (adc_AVR32 == device->type)) )
        {
            int16_t *ptr = (int16_t *)(row->offset + (uint8_t *) info);

            result = atmel_read_command( device, row->data0, row->data1 );
            if( result < 0 ) {
                retVal = result;
            }
            *ptr = (int16_t)result;
        }
    }

    return retVal;
}


/*
 *
 *  device    - the usb_dev_handle to communicate with
 *  mode      - the mode to use when erasing flash
 *              ATMEL_ERASE_BLOCK_0
 *              ATMEL_ERASE_BLOCK_1
 *              ATMEL_ERASE_BLOCK_2
 *              ATMEL_ERASE_BLOCK_3
 *              ATMEL_ERASE_ALL
 *
 *  returns status DFU_STATUS_OK if ok, anything else on error
 */
int32_t atmel_erase_flash( dfu_device_t *device,
                           const uint8_t mode )
{
    uint8_t command[3] = { 0x04, 0x00, 0x00 };
    dfu_status_t status;
    int32_t i;

    DEBUG_MSG( QString("%1( %2, %3 )\n").arg(__FUNCTION__).arg((int)device).arg(mode) );

    switch( mode ) {
        case ATMEL_ERASE_BLOCK_0:
            command[2] = 0x00;
            break;
        case ATMEL_ERASE_BLOCK_1:
            command[2] = 0x20;
            break;
        case ATMEL_ERASE_BLOCK_2:
            command[2] = 0x40;
            break;
        case ATMEL_ERASE_BLOCK_3:
            command[2] = 0x80;
            break;
        case ATMEL_ERASE_ALL:
            command[2] = 0xff;
            break;

        default:
            return -1;
    }

    if( 3 != dfu_download(device, 3, command) ) {
        ERROR_MSG( "dfu_download failed\n" );
        return -2;
    }

    /* It looks like it can take a while to erase the chip.
     * We will try for 10 seconds before giving up.
     */
    for( i = 0; i < 10; i++ ) {
        if( 0 == dfu_get_status(device, &status) ) {
            return status.bStatus;
        }
    }

    return -3;
}

int32_t atmel_set_fuse( dfu_device_t *device,
                          const uint8_t property,
                          const uint32_t value )
{
    int32_t result;
    int16_t buffer[16];
    int32_t address;
    int8_t numbytes;
    int8_t i;

    if( adc_AVR32 != device->type ) {
       ERROR_MSG( "target does not support fuse operation.\n" );
       return -1;
    } 

    if( 0 != atmel_select_fuses(device) ) {
        return -3;
    }

    switch( property ) {
        case set_lock:
            for( i = 0; i < 16; i++ ) {
                buffer[i] = (int16_t)(value & (0x0001 << i));
            }
            numbytes = 16;
            address = 0;
            break;
        case set_epfl:
            buffer[0] = (int16_t)(value & 0x0001);
            numbytes = 1;
            address = 16;
            break;
        case set_bootprot:
            buffer[0] = (int16_t)(value & 0x0001);
            buffer[1] = (int16_t)(value & 0x0002);
            buffer[2] = (int16_t)(value & 0x0004);
            numbytes = 3;
            address = 17;
            break;
        case set_bodlevel:
#ifdef SUPPORT_SET_BOD_FUSES
            /* Enable at your own risk - this has not been tested &
             * may brick your device. */
            for(i = 20;i < 26; i++){
                buffer[i] = value & (0x0001 << (i-20));
            }
            numbytes = 6;
            address = 20;
            break;
#else
            ERROR_MSG( "Setting BODLEVEL can break your chip. Operation not performed\n"
                   "Rebuild with the SUPPORT_SET_BOD_FUSES #define enabled if you really want to do this.\n" );
            return -1;
#endif
        case set_bodhyst:
#ifdef SUPPORT_SET_BOD_FUSES
            /* Enable at your own risk - this has not been tested &
             * may brick your device. */
            buffer[0] = value & 0x0001;
            numbytes = 1;
            address = 26;
            break;
#else
            ERROR_MSG( "Setting BODHYST can break your chip. Operation not performed\n"
                   "Rebuild with the SUPPORT_SET_BOD_FUSES #define enabled if you really want to do this.\n" );
            return -1;
#endif
        case set_boden:
#ifdef SUPPORT_SET_BOD_FUSES
            /* Enable at your own risk - this has not been tested &
             * may brick your device. */
            buffer[0] = value & 0x0001;
            buffer[1] = value & 0x0002;
            numbytes = 2;
            address = 27;
            break;
#else
	    ERROR_MSG( "Setting BODEN can break your chip. Operation not performed\n" 
		   "Rebuild with the SUPPORT_SET_BOD_FUSES #define enabled if you really want to do this.\n" );
            return -1;
#endif
        case set_isp_bod_en:
#ifdef SUPPORT_SET_BOD_FUSES
            /* Enable at your own risk - this has not been tested &
             * may brick your device. */
            buffer[0] = value & 0x0001;
            numbytes = 1;
            address = 29;
            break;
#else
            ERROR_MSG( "Setting ISP_BOD_EN can break your chip. Operation not performed\n" 
		   "Rebuild with the SUPPORT_SET_BOD_FUSES #define enabled if you really want to do this.\n" );
            return -1;
#endif
        case set_isp_io_cond_en:
            buffer[0] = (int16_t)(value & 0x0001);
            numbytes = 1;
            address = 30;
            break;
        case set_isp_force:
            buffer[0] = (int16_t)(value & 0x0001);
            numbytes = 1;
            address = 31;
            break;
        default:
            ERROR_MSG( "Fuse bits unrecognized\n" );
            return -2;
            break;
        }
    
    result = atmel_flash_block( device, buffer, address, numbytes, false );
    if(result < 0) {
        return -6;
     }
    return 0;

}
 
int32_t atmel_set_config( dfu_device_t *device,
                          const uint8_t property,
                          const uint8_t value )
{
    uint8_t command[4] = { 0x04, 0x01, 0x00, 0x00 };
    dfu_status_t status;

    DEBUG_MSG( QString("%1( %2, %3, 0x%4 )\n").arg(__FUNCTION__).arg((int)device).arg(property).arg(value,2,16,QChar('0')) );

    switch( property ) {
        case ATMEL_SET_CONFIG_BSB:
            break;
        case ATMEL_SET_CONFIG_SBV:
            command[2] = 0x01;
            break;
        case ATMEL_SET_CONFIG_SSB:
            command[2] = 0x05;
            break;
        case ATMEL_SET_CONFIG_EB:
            command[2] = 0x06;
            break;
        case ATMEL_SET_CONFIG_HSB:
            command[1] = 0x02;
            break;
        default:
            return -1;
    }

    command[3] = value;

    if( 4 != dfu_download(device, 4, command) ) {
        ERROR_MSG( "dfu_download failed\n" );
        return -2;
    }

    if( 0 != dfu_get_status(device, &status) ) {
        ERROR_MSG( "dfu_get_status failed\n" );
        return -3;
    }

    if( DFU_STATUS_ERROR_WRITE == status.bStatus ) {
        ERROR_MSG( "Device is write protected.\n" );
    }

    return status.bStatus;
}

static int32_t __atmel_read_page( dfu_device_t *device,
                                  const uint32_t start,
                                  const uint32_t end,
                                  uint8_t* buffer,
                                  const bool eeprom )
{
    uint8_t command[6] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint32_t current_start;
    size_t size;
    uint32_t mini_page;
    int32_t result;

    DEBUG_MSG( QString("%1( %2, %3, %4, %5, %6 )\n").arg(__FUNCTION__).arg((int)device).arg(start).arg(end).arg((int)buffer).arg(eeprom ? "true" : "false") );

    if( true == eeprom ) {
        command[1] = 0x02;
    }

    current_start = start;
    size = end - current_start;
    for( mini_page = 0; 0 < size; mini_page++ ) {
        if( ATMEL_MAX_TRANSFER_SIZE < size ) {
            size = ATMEL_MAX_TRANSFER_SIZE;
        }
        command[2] = (uint8_t)(0xff & (current_start >> 8));
        command[3] = (uint8_t)(0xff & current_start);
        command[4] = (uint8_t)(0xff & ((current_start + size - 1)>> 8));
        command[5] = (uint8_t)(0xff & (current_start + size - 1));

        if( 6 != dfu_download(device, 6, command) ) {
            ERROR_MSG( "dfu_download failed\n" );
            return -1;
        }

        result = dfu_upload( device, size, buffer );
        if( result < 0) {
            dfu_status_t status;

            DEBUG_MSG( QString("result: %1\n").arg(result) );
            if( 0 == dfu_get_status(device, &status) ) {
                if( DFU_STATUS_ERROR_FILE == status.bStatus ) {
                    ERROR_MSG( "The device is read protected.\n" );
                } else {
                    ERROR_MSG( "Unknown error.  Try enabling debug.\n" );
                }
            } else {
                ERROR_MSG( "Device is unresponsive.\n" );
            }

            return result;
        }

        buffer += size;
        current_start += size;

        if( current_start < end ) {
            size = end - current_start;
        } else {
            size = 0;
        }
    }

    return (end - start);
}

/* Just to be safe, let's limit the transfer size */
int32_t atmel_read_flash( dfu_device_t *device,
                          const uint32_t start,
                          const uint32_t end,
                          uint8_t* buffer,
                          const size_t buffer_len,
                          const bool eeprom,
                          const bool user,
			  void (*callback)( void *, int ),
			  void *user_data )
{
    uint16_t page = 0;
    uint32_t current_start;
    size_t size;

    DEBUG_MSG( QString("%1( %2, 0x%3, 0x%4, %5, %6, %7 )\n").arg(__FUNCTION__).arg((int)device).arg(start,8,16,QChar('0')).arg(end,8,16,QChar('0')).arg((int)buffer).arg(buffer_len).arg(eeprom ? "true" : "false") );

    if ( callback )
	(*callback)(user_data, 0 );

    if( (NULL == buffer) || (start >= end) || (NULL == device) ) {
        ERROR_MSG( "invalid arguments.\n" );
        return -1;
    }

    if( (end - start) > buffer_len ) {
        ERROR_MSG( QString("buffer isn't large enough - bytes needed: %1 : %2.\n").arg(end - start).arg(buffer_len) );
        return -2;
    }

    /* For the AVR32 chips, select the flash space. */
    if( adc_AVR32 == device->type ) {
        if( user == true ) {
            if( 0 != atmel_select_user(device) ) {
                return -3;
            }
        } else {
            if( 0 != atmel_select_flash(device) ) {
                return -3;
            }
        }
    }

    current_start = start;
    if( end > 0x10000 ) {
        size = 0x10000 - start;
    } else {
        size = end;
    }
    for( page = 0; 0 < size; page++ ) {
        int32_t result;
        if( size > 0x10000 ) {
            size = 0x10000;
        }
        if( user == false ) {
            if( 0 != atmel_select_page(device, page) ) {
                return -4;
            }
        }
        
        result = __atmel_read_page( device, current_start, (current_start + size), buffer, eeprom );
        if( size != (size_t)result ) {
            return -5;
        }

        /* Move the buffer forward. */
        buffer += size;

        current_start += size;

        if( current_start < end ) {
            size = end - current_start;
        } else {
            size = 0;
        }
	if ( callback )
	    (*callback)(user_data, 100 * (end - current_start) / (end - start ) );
    }
    if ( callback )
	(*callback)(user_data, 100 );


    return (end - start);
}

static int32_t __atmel_blank_check_internal( dfu_device_t *device,
                                             const uint32_t start,
                                             const uint32_t end )
{
    uint8_t command[6] = { 0x03, 0x01, 0x00, 0x00, 0x00, 0x00 };

    DEBUG_MSG( QString("%1( %2, 0x%3, 0x%3 )\n").arg(__FUNCTION__).arg((int)device).arg(start,8,16,QChar('0')).arg(end,8,16,QChar('0')) );

    command[2] = (uint8_t)(0xff & (start >> 8));
    command[3] = (uint8_t)(0xff & start);
    command[4] = (uint8_t)(0xff & (end >> 8));
    command[5] = (uint8_t)(0xff & end);

    if( 6 != dfu_download(device, 6, command) ) {
        ERROR_MSG( "dfu_download failed.\n" );
        return -2;
    }

    return 0;
}

int32_t atmel_blank_check( dfu_device_t *device,
                           const uint32_t start,
                           const uint32_t end,
			   void (*callback)( void *, int ),
			   void *user_data )
{
    int32_t rv;
    uint16_t page;
    uint32_t current_start;
    size_t size;

    DEBUG_MSG( QString("%1( %2, 0x%3, 0x%4 )\n").arg(__FUNCTION__).arg((int)device).arg(start,8,16,QChar('0')).arg(end,8,16,QChar('0')) );

    if ( callback )
	(*callback)(user_data, 0 );

    if( (start >= end) || (NULL == device) ) {
        ERROR_MSG( "invalid arguments.\n" );
        return -1;
    }

    rv = -3;

    /* Handle small memory (< 64k) devices without a page selection. */
    if( end < UINT16_MAX ) {
        rv = __atmel_blank_check_internal( device, start, end );
        goto done;
    }

    /* Select FLASH memory */
    if( adc_AVR32 == device->type ) {
        if( 0 != atmel_select_flash(device) ) {
            return -2;
        }
    }

    current_start = start;
    size = end - current_start;
    for( page = 0; 0 < size; page++ ) {
        if( UINT16_MAX < size ) {
            size = UINT16_MAX;
        }

        /* Select the page of memory */
        if( 0 != atmel_select_page(device, page) ) {
            return -2;
        }

        rv = __atmel_blank_check_internal( device, 0, size );
        if( 0 != rv ) {
            /* We ran into a problem. */
            return rv;
        }

        /* Add 1 because we checked an inclusive number of bytes. */
        current_start += size + 1;

        /* because we are subtracting the number of bytes compared, the
         * next starting position is 1 after the end, which causes all
         * sorts of problems if size_t is unsigned. */
        if( current_start < end ) {
            size = end - current_start;
        } else {
            size = 0;
        }

	if ( callback )
	    (*callback)(user_data, 100 * (end - (current_start - 1)) / (end - start ) );

    }
    if ( callback )
	(*callback)(user_data, 100 );

done:
    if( 0 == rv ) {
        int32_t i;

        /* It looks like it can take a while to erase the chip.
         * We will try for 10 seconds before giving up.
         */
        for( i = 0; i < 20; i++ ) {
            dfu_status_t status;
            if( 0 == dfu_get_status(device, &status) ) {
                return status.bStatus;
            }
        }
    }

    ERROR_MSG( "erase chip failed.\n" );
    return -3;
}



/* Not really sure how to test this one. */
int32_t atmel_reset( dfu_device_t *device )
{
    uint8_t command[3] = { 0x04, 0x03, 0x00 };

    DEBUG_MSG( QString("%1( %2 )\n").arg(__FUNCTION__).arg((int)device) );

    if( 3 != dfu_download(device, 3, command) ) {
        ERROR_MSG( "dfu_download failed.\n" );
        return -1;
    }

    if( 0 != dfu_download(device, 0, NULL) ) {
        return -2;
    }

    return 0;
}


int32_t atmel_start_app( dfu_device_t *device, uint16_t addr )
{
    uint8_t command[5] = { 0x04, 0x03, 0x01, (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF) };

    DEBUG_MSG( QString("%1( %2 )\n").arg(__FUNCTION__).arg((int)device) );

    if( 5 != dfu_download(device, 5, command) ) {
        ERROR_MSG( "dfu_download failed.\n" );
        return -1;
    }

    if( 0 != dfu_download(device, 0, NULL) ) {
        ERROR_MSG( "dfu_download failed.\n" );
        return -2;
    }

    return 0;
}


static int32_t atmel_select_flash( dfu_device_t *device )
{
    DEBUG_MSG( QString("%1( %2 )\n").arg(__FUNCTION__).arg((int)device) );

    if( (NULL != device) && (adc_AVR32 == device->type) ) {
        uint8_t command[4] = { 0x06, 0x03, 0x00, 0x00 };

        if( 4 != dfu_download(device, 4, command) ) {
            ERROR_MSG( "dfu_download failed.\n" );
            return -1;
        }
        DEBUG_MSG( "flash selected\n" );
    }

    return 0;
}

static int32_t atmel_select_fuses( dfu_device_t *device )
{
    DEBUG_MSG( QString("%1( %2 )\n").arg(__FUNCTION__).arg((int)device) );

    if( (NULL != device) && (adc_AVR32 == device->type) ) {
        uint8_t command[4] = { 0x06, 0x03, 0x00, 0x03 };

        if( 4 != dfu_download(device, 4, command) ) {
            ERROR_MSG( "dfu_download failed.\n" );
            return -1;
        }
        DEBUG_MSG( "fuses selected\n" );
    }

    return 0;
}


static int32_t atmel_select_user( dfu_device_t *device )
{
    DEBUG_MSG( QString("%1( %2 )\n").arg(__FUNCTION__).arg((int)device) );

    if( (NULL != device) && (adc_AVR32 == device->type) ) {
        uint8_t command[4] = { 0x06, 0x03, 0x00, 0x06 };

        if( 4 != dfu_download(device, 4, command) ) {
            ERROR_MSG( "dfu_download failed.\n" );
            return -1;
        }
        DEBUG_MSG( "flash selected\n" );
    }

    return 0;
}

static int32_t atmel_select_page( dfu_device_t *device,
                                  const uint16_t mem_page )
{
    DEBUG_MSG( QString("%1( %2, %3 )\n").arg(__FUNCTION__).arg((int)device).arg(mem_page) );

    if( NULL != device ) {
        if( adc_AVR32 == device->type ) {
            uint8_t command[5] = { 0x06, 0x03, 0x01, 0x00, 0x00 };
            command[3] = (uint8_t)(0xff & (mem_page >> 8));
            command[4] = (uint8_t)(0xff & mem_page);

            if( 5 != dfu_download(device, 5, command) ) {
                ERROR_MSG( "dfu_download failed.\n" );
                return -1;
            }
        } else if( adc_AVR == device->type ) {
            uint8_t command[4] = { 0x06, 0x03, 0x00, 0x00 };

            command[3] = (char) mem_page;

            if( 4 != dfu_download(device, 4, command) ) {
                ERROR_MSG( "dfu_download failed.\n" );
                return -1;
            }
        }
    }

    return 0;
}


static void atmel_flash_prepair_buffer( int16_t *buffer, const size_t size,
                                        const size_t page_size )
{
    int16_t *page;

    DEBUG_MSG( QString("%1( %2, %3, %4 )\n").arg(__FUNCTION__).arg((int)buffer).arg(size).arg(page_size) );

    for( page = buffer;
         &page[page_size] < &buffer[size];
         page = &page[page_size] )
    {
        uint32_t i;

        for( i = 0; i < page_size; i++ ) {
            if( (0 <= page[i]) && (page[i] <= UINT8_MAX) ) {
                /* We found a valid value. */
                break;
            }
        }

        if( page_size != i ) {
            /* There was valid data in the block & we need to make
             * sure there is no unassigned data.  */
            for( i = 0; i < page_size; i++ ) {
                if( (page[i] < 0) || (UINT8_MAX < page[i]) ) {
                    /* Invalid memory value. */
                    page[i] = 0;
                }
            }
        }
    }
}

int32_t atmel_user( dfu_device_t *device,
                    int16_t *buffer,
                    const uint32_t end )
{
    int32_t result = 0;
    DEBUG_MSG( QString("%1( %2, %3, %4)\n").arg(__FUNCTION__).arg((int)device).arg((int)buffer).arg(end) );

    if( (NULL == buffer) || (end <= 0) ) {
        ERROR_MSG( "invalid arguments.\n" );
        return -1;
    }
    
    /* Select USER page */
    uint8_t command[4] = { 0x06, 0x03, 0x00, 0x06 };
    if( 4 != dfu_download(device, 4, command) ) {
        ERROR_MSG( "dfu_download failed.\n" );
        return -2;
    }
    
    //The user block is one flash page, so we'll just do it all in a block.
    result = atmel_flash_block( device, buffer, 0, end, 0 );
    
    if( result < 0 ) {
        ERROR_MSG( QString("error flashing the block: %1\n").arg(result) );
        return -4;
    }
    
    return 0;
    
}

int32_t atmel_flash( dfu_device_t *device,
                     int16_t *buffer,
                     const uint32_t start,
                     const uint32_t end,
                     const size_t page_size,
                     const bool eeprom,
		     void (*callback)( void *, int ),
		     void *user_data )
{
    uint32_t first = 0;
    int32_t sent = 0;
    uint8_t mem_page = 0;
    int32_t result = 0;
    size_t size = end - start;

    DEBUG_MSG( QString("%1( %2, %3, %4, %5, %6, %7 )\n").arg(__FUNCTION__).arg((int)device).arg((int)buffer).arg(start).arg(end).arg(page_size).arg(eeprom ? "true" : "false") );

    if ( callback )
	(*callback)(user_data, 0 );

    if( (NULL == buffer) || ((end - start) <= 0) ) {
        ERROR_MSG( "invalid arguments.\n" );
        return -1;
    }

    if( adc_AVR32 == device->type ) {
        /* Select FLASH memory */
        uint8_t command[4] = { 0x06, 0x03, 0x00, 0x00 };
        if( 4 != dfu_download(device, 4, command) ) {
            ERROR_MSG( "dfu_download failed.\n" );
            return -2;
        }

        /* Select Page 0 */
        result = atmel_select_page( device, mem_page );
        if( result < 0 ) {
            ERROR_MSG( QString("error selecting the page: %1\n").arg(result) );
            return -3;
        }

    } else {
        atmel_flash_prepair_buffer( &buffer[start], size, page_size );
    }

    first = start;

    for (;;)
    {
        uint32_t last = 0;
        int32_t length;

        /* Find the next valid character to start sending from */
        for( ; first < end; first++ ) {
            if( (0 <= buffer[first]) && (buffer[first] <= UINT8_MAX) ) {
                /* We found a valid value. */
                break;
            }
        }

        /* We didn't find anything to flash. */
        if( first == end ) {
            break;
        }

        /* Find the last character in this valid block to send. */
        for( last = first; last < end; last++ ) {
            if( (buffer[last] < 0) || (UINT8_MAX < buffer[last]) ) {
                break;
            }
        }

recheck_page:
        /* Make sure any writes align with the memory page boudary. */
        if( (0x10000 * (uint32_t)(1 + mem_page)) <= last ) {
            if( first < (0x10000 * (uint32_t)(1 + mem_page)) ) {
                last = 0x10000 * (1 + mem_page);
            } else {
                int32_t result;

                mem_page++;
                result = atmel_select_page( device, mem_page );
                if( result < 0 ) {
                    ERROR_MSG( QString("error selecting the page: %1\n").arg(result) );
                    return -3;
                }
                goto recheck_page;
            }
        }

        length = last - first;

        DEBUG_MSG( QString("valid block length: %1, (%2 - %3)\n").arg(length).arg(first).arg(last) );

	uint32_t start = first;
        while( 0 < length ) {
            int32_t result;

            if( ATMEL_MAX_TRANSFER_SIZE < length ) {
                length = ATMEL_MAX_TRANSFER_SIZE;
            }

            result = atmel_flash_block( device, &(buffer[first]),
                                        (UINT16_MAX & first), length, eeprom );

            if( result < 0 ) {
                ERROR_MSG( QString("error flashing the block: %1\n").arg(result) );
                return -4;
            }

            first += result;
            sent += result;

            DEBUG_MSG( QString("Next first: %1\n").arg(first) );
            length = last - first;
            DEBUG_MSG( QString("valid block length: %1\n").arg(length) );

	    if ( callback )
		(*callback)(user_data, 100 * (first - start) / (last - start) );
        }
        DEBUG_MSG( QString("sent: %1, first: %2 last: %3\n").arg(sent).arg(first).arg(last) );
    }

    if( mem_page > 0 ) {
        int32_t result = atmel_select_page( device, 0 );
        if( result < 0) {
            ERROR_MSG( QString("error selecting the page: %1\n").arg(result) );
            return -5;
        }
    }

    return sent;
}


static void atmel_flash_populate_footer( uint8_t *message, uint8_t *footer,
                                         const uint16_t vendorId,
                                         const uint16_t productId,
                                         const uint16_t bcdFirmware )
{
    int32_t crc;

    DEBUG_MSG( QString("%1( %2, %3, %4, %5, %6 )\n").arg(__FUNCTION__).arg((int)message).arg((int)footer).arg(vendorId).arg(productId).arg(bcdFirmware) );

    if( (NULL == message) || (NULL == footer) ) {
        return;
    }

    /* TODO: Calculate the message CRC */
    crc = 0;

    /* CRC 4 bytes */
    footer[0] = (uint8_t)(0xff & (crc >> 24));
    footer[1] = (uint8_t)(0xff & (crc >> 16));
    footer[2] = (uint8_t)(0xff & (crc >> 8));
    footer[3] = (uint8_t)(0xff & crc);

    /* Length of DFU suffix - always 16. */
    footer[4] = 16;

    /* ucdfuSignature - fixed 'DFU'. */
    footer[5] = 'D';
    footer[6] = 'F';
    footer[7] = 'U';

    /* BCD DFU specification number (1.1)*/
    footer[8] = 0x01;
    footer[9] = 0x10;

    /* Vendor ID or 0xFFFF */
    footer[10] = (uint8_t)(0xff & (vendorId >> 8));
    footer[11] = (uint8_t)(0xff & vendorId);

    /* Product ID or 0xFFFF */
    footer[12] = (uint8_t)(0xff & (productId >> 8));
    footer[13] = (uint8_t)(0xff & productId);

    /* BCD Firmware release number or 0xFFFF */
    footer[14] = (uint8_t)(0xff & (bcdFirmware >> 8));
    footer[15] = (uint8_t)(0xff & bcdFirmware);
}

static void atmel_flash_populate_header( uint8_t *header,
                                         const uint32_t start_address,
                                         const size_t length, const bool eeprom )
{
    uint16_t end;

    DEBUG_MSG( QString("%1( %2, %3, %4, %5 )\n").arg(__FUNCTION__).arg((int)header).arg(start_address).arg(length).arg(eeprom ? "true" : "false") );

    if( NULL == header ) {
        return;
    }

    /* If we send 1 byte @ 0x0000, the end address will also be 0x0000 */
    end = (uint16_t)(start_address + (length - 1));

    /* Command Identifier */
    header[0] = 0x01;   /* ld_prog_start */

    /* data[0] */
    header[1] = ((true == eeprom) ? 0x01 : 0x00);

    /* start_address */
    header[2] = (uint8_t)(0xff & (start_address >> 8));
    header[3] = (uint8_t)(0xff & start_address);

    /* end_address */
    header[4] = (uint8_t)(0xff & (end >> 8));
    header[5] = (uint8_t)(0xff & end);
}

static int32_t atmel_flash_block( dfu_device_t *device,
                                  int16_t *buffer,
                                  const uint32_t base_address,
                                  const size_t length,
                                  const bool eeprom )
                              
{
    uint8_t message[ATMEL_MAX_FLASH_BUFFER_SIZE];
    uint8_t *header;
    uint8_t *data;
    uint8_t *footer;
    size_t message_length;
    int32_t result;
    dfu_status_t status;
    size_t control_block_size;  /* USB control block size */
    size_t alignment;

    DEBUG_MSG( QString("%1( %2, %3, %4, %5, %6 )\n").arg(__FUNCTION__).arg((int)device).arg((int)buffer).arg(base_address).arg(length).arg(eeprom ? "true" : "false") );

    if( (NULL == buffer) || (ATMEL_MAX_TRANSFER_SIZE < length) ) {
        ERROR_MSG( "invalid arguments.\n" );
        return -1;
    }

    /* 0 out the message. */
    memset( message, 0, ATMEL_MAX_FLASH_BUFFER_SIZE );

    if( adc_AVR32 == device->type ) {
        control_block_size = ATMEL_AVR32_CONTROL_BLOCK_SIZE;
        alignment = base_address % ATMEL_AVR32_CONTROL_BLOCK_SIZE;
    } else {
        control_block_size = ATMEL_CONTROL_BLOCK_SIZE;
        alignment = 0;
    }

    header = &message[0];
    data   = &message[control_block_size + alignment];
    footer = &data[length];

    atmel_flash_populate_header( header, base_address, length, eeprom );

    DEBUG_MSG( QString("%1 bytes to MCU %2\n").arg(length).arg(base_address,6,16,QChar('0')) );

    /* Copy the data */
    for( unsigned i = 0; i < length; i++ ) {
        data[i] = (uint8_t) buffer[i];
    }

    atmel_flash_populate_footer( message, footer, 0xffff, 0xffff, 0xffff );

    message_length = ((size_t) (footer - header)) + ATMEL_FOOTER_SIZE;
    DEBUG_MSG( QString("message length: %1\n").arg(message_length) );

    result = dfu_download( device, message_length, message );

    if( message_length != (size_t)result ) {
        if( -EPIPE == result ) {
            /* The control pipe stalled - this is an error
             * caused by the device saying "you can't do that"
             * which means the device is write protected.
             */
            ERROR_MSG( "Device is write protected.\n" );

            dfu_clear_status( device );
        } else {
            ERROR_MSG( QString("dfu_download failed. %1\n").arg(result) );
        }
        return -2;
    }

    /* check status */
    if( 0 != dfu_get_status(device, &status) ) {
        ERROR_MSG( "dfu_get_status failed.\n" );
        return -3;
    }

    if( DFU_STATUS_OK != status.bStatus ) {
        ERROR_MSG( QString("status(%1) was not OK.\n").arg(dfu_status_to_string(status.bStatus)) );
        return -4;
    }

    return (int32_t) length;
}


void atmel_print_device_info( FILE *stream, atmel_device_info_t *info )
{
    fprintf( stream, "%18s: 0x%04x - %d\n", "Bootloader Version", info->bootloaderVersion, info->bootloaderVersion );
    fprintf( stream, "%18s: 0x%04x - %d\n", "Device boot ID 1", info->bootID1, info->bootID1 );
    fprintf( stream, "%18s: 0x%04x - %d\n", "Device boot ID 2", info->bootID2, info->bootID2 );

    //if( /* device is 8051 based */ 0 ) {
    //    fprintf( stream, "%18s: 0x%04x - %d\n", "Device BSB", info->bsb, info->bsb );
    //    fprintf( stream, "%18s: 0x%04x - %d\n", "Device SBV", info->sbv, info->sbv );
    //    fprintf( stream, "%18s: 0x%04x - %d\n", "Device SSB", info->ssb, info->ssb );
    //    fprintf( stream, "%18s: 0x%04x - %d\n", "Device EB", info->eb, info->eb );
    //}

    fprintf( stream, "%18s: 0x%04x - %d\n", "Manufacturer Code", info->manufacturerCode, info->manufacturerCode );
    fprintf( stream, "%18s: 0x%04x - %d\n", "Family Code", info->familyCode, info->familyCode );
    fprintf( stream, "%18s: 0x%04x - %d\n", "Product Name", info->productName, info->productName );
    fprintf( stream, "%18s: 0x%04x - %d\n", "Product Revision", info->productRevision, info->productRevision );
    fprintf( stream, "%18s: 0x%04x - %d\n", "HWB", info->hsb, info->hsb );
}
