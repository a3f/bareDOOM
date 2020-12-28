//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Endianess handling, swapping 16bit and 32bit.
//


#ifndef __I_SWAP__
#define __I_SWAP__

#if defined(__BAREBOX__)

#include <byteorder.h>

#define SHORT(x)  ((signed short) le16_to_cpu(x))
#define LONG(x)   ((signed int) le32_to_cpu(x))

#ifdef __LITTLE_ENDIAN
#define SYS_LITTLE_ENDIAN
#elif __BIG_ENDIAN
#define SYS_BIG_ENDIAN
#endif

#else

#define SHORT(x)  ((signed short) (x))
#define LONG(x)   ((signed int) (x))

#define SYS_LITTLE_ENDIAN

#endif
#endif
