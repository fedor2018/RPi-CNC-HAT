#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#ifndef _WIN32
#include <sys/types.h>
#endif

typedef unsigned short ushort;
typedef unsigned short uint16_t;
typedef unsigned char byte;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef short int16_t;
#ifdef _WIN32
typedef char int8_t;
#endif

#endif
