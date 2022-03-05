// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
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
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "i_input.h"
#include "z_zone.h"

#include "tables.h"
#include "doomkeys.h"

#include "doomgeneric.h"

#include <stdlib.h>

struct FB_ScreenInfo s_Fb;

int fb_scaling = 1;

static void *cmap;

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Gamma correction level to use

int usegamma = 0;

void I_InitGraphics (void)
{
    int i;

    printf("I_InitGraphics: framebuffer: x_res: %d, y_res: %d, bpp: %d\n",
            s_Fb.xres, s_Fb.yres, s_Fb.bytes_per_pixel * 8);

    printf("I_InitGraphics: framebuffer: RGBA: %d%d%d%d, red_off: %d, green_off: %d, blue_off: %d, transp_off: %d\n",
            s_Fb.red.length, s_Fb.green.length, s_Fb.blue.length, s_Fb.transp.length, s_Fb.red.offset, s_Fb.green.offset, s_Fb.blue.offset, s_Fb.transp.offset);

    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0) {
        i = atoi(myargv[i + 1]);
        fb_scaling = i;
        printf("I_InitGraphics: Scaling factor: %d\n", fb_scaling);
    } else {
        fb_scaling = max(s_Fb.xres, s_Fb.yres) / SCREENWIDTH;
        if (min(s_Fb.xres, s_Fb.yres) / SCREENHEIGHT < fb_scaling)
            fb_scaling = min(s_Fb.xres, s_Fb.yres);
        printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);
    }

    if (s_Fb.xres < s_Fb.yres)
	printf("I_InitGraphics: Rotating by 90 degrees\n");

    cmap = Z_Malloc(256 * s_Fb.bytes_per_pixel, PU_STATIC, NULL);

    /* Allocate screen to draw to */
	I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);  // For DOOM to draw on

	screenvisible = true;

    I_InitInput();
}

void I_ShutdownGraphics (void)
{
	Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{

}

void I_StartTic (void)
{
	I_GetEvent();
}

void I_UpdateNoBlit (void)
{
}

//
// I_FinishUpdate
//

uint32_t* DG_ScreenBuffer;

#define PIXEL(x, y) ((x) + (y) * SCREENWIDTH)

void I_FinishUpdate (void)
{
    int x, y, xmax, ymax;
    unsigned char *pixel_out, *pixel_in;
    int xpad, ypad;
    bool rotate = false;

    /* DRAW SCREEN */
    pixel_out = (unsigned char *) DG_ScreenBuffer;
    pixel_in = (unsigned char *) I_VideoBuffer;

    if (s_Fb.xres < s_Fb.yres) {
	    rotate = true;

	    xmax = SCREENHEIGHT;
	    ymax = SCREENWIDTH;
    } else {
	    xmax = SCREENWIDTH;
	    ymax = SCREENHEIGHT;
    }

    xpad = (s_Fb.xres - xmax * fb_scaling) * s_Fb.bytes_per_pixel / 2;
    ypad = (s_Fb.yres - ymax * fb_scaling) * s_Fb.bytes_per_pixel / 2;
    pixel_out += s_Fb.xres * ypad;

    for (y = 0; y < ymax; y++) {
	int dupe_line = fb_scaling - 1;
	do {
		pixel_out += xpad;
		for (x = 0; x < xmax; x++) {
			int i;

			if (rotate)
				pixel_in = &I_VideoBuffer[PIXEL(ymax - y - 1, x)];
			else
				pixel_in = &I_VideoBuffer[PIXEL(x, y)];

			for (i = 0; i < fb_scaling; i++) {
				memcpy(pixel_out, cmap + *pixel_in * s_Fb.bytes_per_pixel,
				       s_Fb.bytes_per_pixel);
				pixel_out += s_Fb.bytes_per_pixel;
			}
		}
		pixel_out += xpad;
	} while (dupe_line-- > 0);
    }

    DG_DrawFrame();
}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

void I_SetPalette (byte* palette)
{
    byte *adr = cmap;
    int i;

    /* performance boost:
     * map to the right pixel format over here! */

    for (i = 0; i < 256; ++i) {
	u8 r, g, b;
	u32 pixel;

        r = gammatable[usegamma][*palette++];
        g = gammatable[usegamma][*palette++];
        b = gammatable[usegamma][*palette++];

	pixel  = (r >> (8 - s_Fb.red.length)) << s_Fb.red.offset |
		 (g >> (8 - s_Fb.green.length)) << s_Fb.green.offset |
		 (b >> (8 - s_Fb.blue.length)) << s_Fb.blue.offset;


	switch (s_Fb.bytes_per_pixel) {
	case 1:
		*(u8 *)adr = pixel;
		break;
	case 2:
		*(u16 *)adr = pixel;
		break;
	case 4:
		*(u32 *)adr = pixel;
		break;
	}

	adr += s_Fb.bytes_per_pixel;
    }

}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetWindowTitle (char *title)
{
	DG_SetWindowTitle(title);
}

void I_GraphicsCheckCommandLine (void)
{
}

void I_SetGrabMouseCallback (grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk(void)
{
}

void I_BindVideoVariables (void)
{
}

void I_DisplayFPSDots (boolean dots_on)
{
}

void I_CheckIsScreensaver (void)
{
}
