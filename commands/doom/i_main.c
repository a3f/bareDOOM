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
//	Main program, simply calls D_DoomMain high level loop.
//
//

#ifdef __BAREBOX__
#include <command.h>
#include <envfs.h>
#include <init.h>
#include <bthread.h>
#endif

#include "doom.h"
#include "z_zone.h"
#include "doomgeneric.h"
#include "m_argv.h"

static struct bthread *doom;

static int doom_main(int argc, char **argv)
{
	char **arg;

	if (argc == 2 && !strcmp(argv[1], "-c")) {
		if (doom)
			bthread_cancel(doom);
		doom = NULL;
		return 0;
	}

	// save arguments
	myargc = argc;
	myargv = memdup(argv, (argc + 1)  * sizeof(myargv));

	for (arg = myargv; *arg; arg++)
		*arg = strdup(*arg);

	doom = bthread_run(DG_RunDoom, NULL, "doom");

	return doom ? 0 : -ENOMEM;
}

#ifdef __BAREBOX__

BAREBOX_CMD_HELP_START(doom)
	BAREBOX_CMD_HELP_TEXT("This command runs the original DOOM ported to barebox")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(doom)
	.cmd		= doom_main,
	BAREBOX_CMD_DESC("slaughter demons")
	BAREBOX_CMD_GROUP(CMD_GRP_MISC)
	BAREBOX_CMD_HELP(cmd_doom_help)
BAREBOX_CMD_END

#else

int main(int argc, char **argv)
{
    return doom_main(argc, argv);
}

#endif
