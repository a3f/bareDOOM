#include "doomkeys.h"

#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define KEYQUEUE_SIZE 16

static unsigned char convertToDoomKey(unsigned int key)
{
	switch (key)
	{
	case BB_KEY_ENTER:
		key = KEY_ENTER;
		break;
	case '\b':
		key = KEY_ESCAPE;
		break;
	case BB_KEY_LEFT:
		key = KEY_LEFTARROW;
		break;
	case BB_KEY_RIGHT:
		key = KEY_RIGHTARROW;
		break;
	case BB_KEY_UP:
		key = KEY_UPARROW;
		break;
	case BB_KEY_DOWN:
		key = KEY_DOWNARROW;
		break;
	case 'z':
		key = KEY_FIRE;
		break;
	case ' ':
		key = KEY_USE;
		break;
	case 'e':
		key = KEY_RSHIFT;
		break;
	default:
		key = tolower(key);
		break;
	}

	return key;
}

void DG_Init()
{
}


void DG_DrawFrame()
{
	memcpy(FrameBuffer, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	//printf("frame\n");
}

void DG_SleepMs(uint32_t ms)
{
	mdelay(ms)
}

uint32_t DG_GetTicksMs()
{
	return get_time_ns() / 1000000;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	return 0;
}

void DG_SetWindowTitle(const char * title)
{
}
