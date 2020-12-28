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

struct FB_ScreenInfo s_Fb = {
    .xres = 640,
    .yres = 480,
    .bits_per_pixel = 32,

    .blue.length = 8,
    .green.length = 8,
    .red.length = 8,
    .transp.length = 8,

    .blue.offset = 0,
    .green.offset = 8,
    .red.offset = 16,
    .transp.offset = 24,
};

int fb_scaling = 1;

struct color {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};

static struct color colors[256];

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Gamma correction level to use

int usegamma = 0;

typedef struct
{
	byte r;
	byte g;
	byte b;
} col_t;

// Palette converted to RGB565

static uint16_t rgb565_palette[256];

static void memsetw(void *s, uint16_t c, size_t n)
{
	size_t i;
	uint16_t* tmp = s;

	for (i = 0; i < n; i++)
		*tmp++ = c;
}

static void memsetl(void *s, uint32_t c, size_t n)
{
	size_t i;
	uint32_t* tmp = s;

	for (i = 0; i < n; i++)
		*tmp++ = c;
}

__attribute__((unused))
static void cmap_to_fb_argb32(uint8_t * out, uint8_t * in, int in_pixels)
{
    int i;
    struct color c;
    uint32_t pix;
    uint16_t r, g, b;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in];  /* R:8 G:8 B:8 format! */
        r = (uint16_t)c.r;
        g = (uint16_t)c.g;
        b = (uint16_t)c.b;
        pix = r << 16;
        pix |= g << 8;
        pix |= b << 0;

	memsetl(out, pix, 2);

	out += 32 * 2 / 8;

        in++;
    }
}

static void cmap_to_fb_generic(uint8_t * out, uint8_t * in, int in_pixels)
{
    int i;
    struct color c;
    uint32_t pix;
    uint16_t r, g, b;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in];  /* R:8 G:8 B:8 format! */
        r = (uint16_t)(c.r >> (8 - s_Fb.red.length));
        g = (uint16_t)(c.g >> (8 - s_Fb.green.length));
        b = (uint16_t)(c.b >> (8 - s_Fb.blue.length));
        pix = r << s_Fb.red.offset;
        pix |= g << s_Fb.green.offset;
        pix |= b << s_Fb.blue.offset;

	switch(s_Fb.bits_per_pixel)
	{
	case 8:
		memset(out, pix, fb_scaling);
		break;
	case 16:
		memsetw(out, pix, fb_scaling);
		break;
	case 32:
	case 24:
		memsetl(out, pix, fb_scaling);
		break;
	}

	out += s_Fb.bits_per_pixel * fb_scaling / 8;

        in++;
    }
}

void (*cmap_to_fb)(uint8_t * out, uint8_t * in, int in_pixels);

void I_InitGraphics (void)
{
    int i;

    printf("I_InitGraphics: framebuffer: x_res: %d, y_res: %d, bpp: %d\n",
            s_Fb.xres, s_Fb.yres, s_Fb.bits_per_pixel);

    printf("I_InitGraphics: framebuffer: RGBA: %d%d%d%d, red_off: %d, green_off: %d, blue_off: %d, transp_off: %d\n",
            s_Fb.red.length, s_Fb.green.length, s_Fb.blue.length, s_Fb.transp.length, s_Fb.red.offset, s_Fb.green.offset, s_Fb.blue.offset, s_Fb.transp.offset);

    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);


    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0) {
        i = atoi(myargv[i + 1]);
        fb_scaling = i;
        printf("I_InitGraphics: Scaling factor: %d\n", fb_scaling);
    } else {
        fb_scaling = s_Fb.xres / SCREENWIDTH;
        if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
            fb_scaling = s_Fb.yres / SCREENHEIGHT;
        printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);
    }


#ifdef __LITTLE_ENDIAN
    if (s_Fb.red.length == 8 && s_Fb.green.length == 8 &&
	s_Fb.blue.length == 8 && s_Fb.red.offset == 16 &&
	s_Fb.green.offset == 8 && s_Fb.blue.offset == 0 &&
	s_Fb.bits_per_pixel == 32 && fb_scaling == 2)
	    cmap_to_fb = cmap_to_fb_argb32;
    else
#endif
	    cmap_to_fb = cmap_to_fb_generic;

        printf("I_InitGraphics: Using: %pS\n", cmap_to_fb);

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

void I_FinishUpdate (void)
{
    int y;
    int x_offset, y_offset, x_offset_end;
    unsigned char *line_in, *line_out;

    /* Offsets in case FB is bigger than DOOM */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 2048 =s_Fb width, 320 screenwidth */
    y_offset     = (((s_Fb.yres - (SCREENHEIGHT * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2;
    x_offset     = (((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2; // XXX: siglent FB hack: /4 instead of /2, since it seems to handle the resolution in a funny way
    //x_offset     = 0;
    x_offset_end = ((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8) - x_offset;

    /* DRAW SCREEN */
    line_in  = (unsigned char *) I_VideoBuffer;
    line_out = (unsigned char *) DG_ScreenBuffer;

    y = SCREENHEIGHT;

    while (y--)
    {
        int i;
        for (i = 0; i < fb_scaling; i++) {
            line_out += x_offset;
            cmap_to_fb((void*)line_out, (void*)line_in, SCREENWIDTH);
            line_out += (SCREENWIDTH * fb_scaling * (s_Fb.bits_per_pixel/8)) + x_offset_end;
        }
        line_in += SCREENWIDTH;
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

//
// I_SetPalette
//
#define GFX_RGB565(r, g, b)			((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))
#define GFX_RGB565_R(color)			((0xF800 & color) >> 11)
#define GFX_RGB565_G(color)			((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color)			(0x001F & color)

void I_SetPalette (byte* palette)
{
	int i;
	//col_t* c;

	//for (i = 0; i < 256; i++)
	//{
	//	c = (col_t*)palette;

	//	rgb565_palette[i] = GFX_RGB565(gammatable[usegamma][c->r],
	//								   gammatable[usegamma][c->g],
	//								   gammatable[usegamma][c->b]);

	//	palette += 3;
	//}
    

    /* performance boost:
     * map to the right pixel format over here! */

    for (i=0; i<256; ++i ) {
        colors[i].a = 0;
        colors[i].r = gammatable[usegamma][*palette++];
        colors[i].g = gammatable[usegamma][*palette++];
        colors[i].b = gammatable[usegamma][*palette++];
    }
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex (int r, int g, int b)
{
    int best, best_diff, diff;
    int i;
    col_t color;

    printf("I_GetPaletteIndex\n");

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
    	color.r = GFX_RGB565_R(rgb565_palette[i]);
    	color.g = GFX_RGB565_G(rgb565_palette[i]);
    	color.b = GFX_RGB565_B(rgb565_palette[i]);

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
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
