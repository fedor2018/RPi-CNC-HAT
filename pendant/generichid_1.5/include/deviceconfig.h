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

#ifndef _DEVICECONFIG_H_
#define _DEVICECONFIG_H_

#if defined(_WIN32) || defined(__linux)
#pragma pack(push,1)
#include "datatypes.h"
#endif

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4200)
#endif


#define DEBOUNCE_MIN    0
#define DEBOUNCE_MAX    250

struct DebounceData
{
    uint8_t start_time;
    uint8_t key_down:1;
};
typedef struct DebounceData DebounceData;


enum ControlType
{
    None = 0,
    Potentiometer = 1,
    Switch =2,
    RotarySwitch = 3,
    KeyMatrix = 4,
    DigitalEncoder = 5,
    LED = 6,
    BicolourLED = 7,
    TricolourLED = 8, 
    LCD = 9,
    DirectionalSwitch = 10,
    Counter = 11,
    AnalogEncoder = 12,
    PWMOutput = 13,
    RGBLED = 14,
    LCD_SPI = 15
};

struct SControlHeader
{
    byte Type;
    byte Length;
    byte ReportIdMax;
    byte ReportId;
};


enum PortIDs
{
    PortA = 0,
    PortB,
    PortC,
    PortD,
    PortE,
    PortF,
};

struct SAnalogEncoderControl
{
    struct SControlHeader hdr;
    byte PortA;
    byte PortB;
    byte DebounceMs;
    byte Options;
	#define AE_PULLUPA     0
	#define AE_PULLUPB     1
    DebounceData DebounceA;
    DebounceData DebounceB;
};

struct SBicolourLEDControl
{
    struct SControlHeader hdr;
    byte PortA;
    byte PortB;
};

struct SCounterControl
{
    struct SControlHeader hdr;
    uint16_t Period;
    byte Bits;
};

struct SDigitalEncoderControl
{
    struct SControlHeader hdr;
    byte PortA;
    byte PortB;
    byte Bits;
    byte Options;
	#define DE_PULLUP      0
	#define DE_ABSOLUTE    1
    byte Index;	// Working data.
};

enum DirSwitchType
{
    DS_NS = 0,
    DS_WE,
    DS_4way,
    DS_8way
};

struct SDirSwitchControl
{
    struct SControlHeader hdr;
    byte Type;
    byte Ports;
    byte Options;
    byte DebounceMs;
	#define DSW_PULLUP      0
    struct _SDirSwitchData
    {
        byte Port;
        DebounceData Debounce;
    } Switches[0];
};

#define DIRSW_PORTN	0
#define DIRSW_PORTS 	1
#define DIRSW_PORTE 	2
#define DIRSW_PORTW 	3
#define DIRSW_PORTNE	4
#define DIRSW_PORTNW	5
#define DIRSW_PORTSE	6
#define DIRSW_PORTSW	7


struct SKeyMatrixControl
{
    struct SControlHeader hdr;
    byte Rows;
    byte Cols;
    byte DebounceMs;
    byte Data[0];	// row ports (Rows)
                    // column ports (Cols)
                    // debounce buffer (Cols * Rows)
};


struct SLCDControl
{
    struct SControlHeader hdr;
    byte nRows;
    byte nColumns;
    byte b8Bit;
    byte nFunctionSet;
    byte nPortRS;
    byte nPortRW;
    byte nPortE;
    byte nPortD0;
    byte nPortD1;
    byte nPortD2;
    byte nPortD3;
    byte nPortD4;
    byte nPortD5;
    byte nPortD6;
    byte nPortD7;
    byte RowAddr[4];		// Start of each row
};

struct SLCDSPIControl
{
    struct SControlHeader hdr;
    byte nRows;
    byte nColumns;
    byte nPortSS;
    byte nPortSCK;
    byte nPortMOSI;
    byte nPortMISO;
};

struct SLEDControl
{
    struct SControlHeader hdr;
    byte Port;
    byte Options;
	#define LED_SINK      0
};

struct SPotentiometerControl
{
    struct SControlHeader hdr;
    byte Port;
    byte Bits;
};

struct SPWMControl
{
    struct SControlHeader hdr;
    byte Port;
    byte Bits;
    uint16_t Resolution;
};

#define RGB_RED	    0x04
#define RGB_GREEN   0x02
#define RGB_BLUE    0x01

struct SRGBLEDControl
{
    struct SControlHeader hdr;
    byte PortR;
    byte PortG;
    byte PortB;
    byte Options;
	#define RGB_SINK      0
};



struct SRotarySwitchControl
{
    struct SControlHeader hdr;
    byte PinCount;
    byte ReportSize;
    byte Options;
    byte DebounceMs;
	#define RSW_PULLUP      0
	#define RSW_ENCODED     1
    byte LastValue;
    struct _SRotarySwitchPin
    {
        byte Port;
        byte Bit;
        DebounceData Debounce;
    } Pins[0];
};


struct SSwitchControl
{
    struct SControlHeader hdr;
    byte Port;
    byte DebounceMs;
    byte Options;
	#define SW_PULLUP      0
    DebounceData Debounce;
};

struct STricolourLEDControl
{
    struct SControlHeader hdr;
    byte PortA;
    byte PortB;
    byte Options;
	#define TRI_SINK      0
};





struct SDynamicHID
{
    uint16_t nBlockLength;
    uint16_t nStringCount;
    uint16_t nReportDescriptorOffset;
    uint16_t nReportDescriptorLength;
    uint16_t nDeviceDescriptorOffset;
    uint16_t nDeviceDescriptorLength;
    uint16_t nConfigDescriptorOffset;
    uint16_t nConfigDescriptorLength;
    uint16_t nHIDDescriptorOffset;
    uint16_t nHIDDescriptorLength;
    uint16_t nApplicationDataOffset;
    uint16_t nStringIndex0Offset[0];
};

#define MAX_READ_BUFFER		64
#define MAX_REPORTS		20
#define MAX_TIMERS		3

struct TimerConfig
{
    byte Mode;
	#define TC_MODE_PHASECORRECT	0
	#define TC_MODE_FASTPWM		1
    uint16_t Prescaler;
    uint16_t CounterTop;
};

struct SApplicationHeader
{
    byte nPowerPort;
    byte nOptions;
	#define DEVICE_OPTION_BUS_POWERED	0x01
	#define DEVICE_OPTION_USE_USBKEY_LED1	0x02
	#define DEVICE_OPTION_USE_USBKEY_LED2	0x04
	#define	DEVICE_OPTION_USE_USBKEY_LEDS	(DEVICE_OPTION_USE_USBKEY_LED1|DEVICE_OPTION_USE_USBKEY_LED2)
	#define DEVICE_OPTION_HID_DEBUG		0x08
	#define DEVICE_OPTION_5V		0x10
	#define DEVICE_OPTION_ENABLE_JTAG	0x20
    byte nDebugLevel;
    struct TimerConfig timers[MAX_TIMERS];
    byte OutputReportLength[MAX_REPORTS];
};

#define INPUT_REPORT_ID 1
#define OUTPUT_REPORT_ID 2
#define BOOTLOADER_REPORT_ID 3
#define FIRST_LCD_REPORT_ID 4
    #define LCD_ATTRIBUTE_REPORT_ID 0
    #define LCD_DISPLAY_REPORT_ID 0
    #define LCD_FONT_REPORT_ID 1
    #define LCD_CURSOR_POSITION_REPORT_ID 2
    #define LCD_FUNCTION_SET_REPORT_ID 3
    #define LCDSPI_ATTRIBUTE_REPORT_ID 0
    #define LCDSPI_DISPLAY_REPORT_ID 0
    #define LCDSPI_DISPLAY_CONTROL_REPORT_ID 1
    #define LCDSPI_DRAW_RECT_REPORT_ID 2

#define MAGIC_BOOTLOADER_CODE 0xDF0DF0DF
#define MAX_HID_DATA	4096				// 4k of eeprom
#define DYNAMIC_HIDDATA_ADDRESS	    (0x10000 - 0x1000 - 0x1000)	    // (this is actually 4k words)

#ifdef _WIN32
#pragma warning(pop)
#pragma warning(disable:4815)
#endif

#if defined(_WIN32) || defined(__linux)
#pragma pack(pop)
#endif

#endif
