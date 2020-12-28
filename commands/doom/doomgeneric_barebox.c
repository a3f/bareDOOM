#include "doomkeys.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <input/input.h>
#include <input/keyboard.h>
#include <dt-bindings/input/linux-event-codes.h>
#include <linux/math64.h>
#include <fb.h>
#include <gui/gui.h>
#include <gui/graphic_utils.h>
#include <i_video.h>
#include <bthread.h>
#include <asm/setjmp.h>

#include "doomgeneric.h"
#include "z_zone.h"
#include "doom.h"

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static int convertToDoomKey(int key)
{
	switch (key)
	{
	case KEY_ENTER:
		return DOOM_KEY_ENTER;
	case KEY_LEFT:
		return DOOM_KEY_LEFTARROW;
	case KEY_RIGHT:
		return DOOM_KEY_RIGHTARROW;
	case KEY_UP:
		return DOOM_KEY_UPARROW;
	case KEY_DOWN:
		return DOOM_KEY_DOWNARROW;
	case KEY_RIGHTCTRL:
	case KEY_LEFTCTRL:
	case KEY_Z:
		return DOOM_KEY_FIRE;
	case KEY_SPACE:
		return DOOM_KEY_USE;
	case KEY_E:
	case KEY_RIGHTSHIFT:
		return DOOM_KEY_RSHIFT;
	case KEY_ESC:
	case KEY_BACKSPACE:
		return DOOM_KEY_ESCAPE;
	default:
		if (key > NR_KEYS)
			break;
		key = keycode_bb_keys[key];
		if (isprint(key))
			return key;
	}

	return -1;
}

static void addKeyToQueue(int keyData)
{
        s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
        s_KeyQueueWriteIndex++;
        s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void key_input_notify(struct input_notifier *in,
			     struct input_event *ev)
{
	int keyData = convertToDoomKey(ev->code);

	if (keyData < 0)
		return;

	keyData |= ev->value << 8;

	addKeyToQueue(keyData);
}

static struct input_notifier notifier = { key_input_notify };
static struct console_device *input;

static struct screen *sc;

jmp_buf exit_jmpbuf;

static int DG_Init(void)
{
	struct fb_info *info;

	sc = fb_open("/dev/fb0");
	if (IS_ERR(sc)) {
		printf("fb_open: error opening /dev/fb0\n");
		return -ENOENT;
	}

	info = sc->info;

	fb_enable(info);

	DG_ScreenBuffer = info->screen_base;

	s_Fb.xres = info->xres;
	s_Fb.yres = info->yres;
	s_Fb.bits_per_pixel = info->bits_per_pixel;

	s_Fb.blue.length = info->blue.length;
	s_Fb.green.length = info->green.length;
	s_Fb.red.length = info->red.length;
	s_Fb.transp.length = info->transp.length;

	s_Fb.blue.offset = info->blue.offset;
	s_Fb.green.offset = info->green.offset;
	s_Fb.red.offset = info->red.offset;
	s_Fb.transp.offset = info->transp.offset;

	if (IS_ENABLED(CONFIG_INPUT)) {
		input = console_get_by_name("input");
		if (input && console_open(input) == 0)
			console_set_active(input, 0);

		input_register_notfier(&notifier);
	}

	return 0;
}

static void DG_Exit(void)
{
	if (IS_ENABLED(CONFIG_INPUT)) {
		input_unregister_notfier(&notifier);
		if (input)
			console_set_active(input, CONSOLE_STDIN);
	}

	fb_close(sc);

        Z_FreeMemory();

        pr_notice("DOOM port doesn't release all resources. State now:\n");
        malloc_stats();
}

void DG_RunDoom(void *arg)
{
    int ret;

    ret = DG_Init();
    if (WARN_ON(ret < 0))
	    return;

    ret = setjmp(exit_jmpbuf);
    if (!ret)
	    D_DoomMain();

    DG_Exit();

    WARN_ON(ret == EXIT_SUCCESS ? 0 : ret);
}

extern int showfps;

void DG_DrawFrame(void)
{
	bthread_reschedule();
}

void DG_SleepMs(uint32_t ms)
{
	mdelay(ms);
}

uint32_t DG_GetTicksMs()
{
	return div_u64(get_time_ns(), 1000000);
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{

	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		//key queue is empty

		return 0;
	}
	else
	{
		unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;

		return 1;
	}
}

void DG_SetWindowTitle(const char * title)
{
}
