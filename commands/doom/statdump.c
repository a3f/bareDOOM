 /*

 Copyright(C) 2005-2014 Simon Howard

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 --

 Functions for presenting the information captured from the statistics
 buffer to a file.

 */

#include "d_player.h"
#include "d_mode.h"
#include "m_argv.h"

#include "statdump.h"

// Array of end-of-level statistics that have been captured.

#define MAX_CAPTURES 32
static wbstartstruct_t captured_stats[MAX_CAPTURES];
static int num_captured_stats = 0;

void StatCopy(wbstartstruct_t *stats)
{
    if (M_ParmExists("-statdump") && num_captured_stats < MAX_CAPTURES)
    {
        memcpy(&captured_stats[num_captured_stats], stats,
               sizeof(wbstartstruct_t));
        ++num_captured_stats;
    }
}

void StatDump(void)
{
}
