#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#include "i_video.h"

#include <inttypes.h>

#define DOOMGENERIC_RESX (s_Fb.xres)
#define DOOMGENERIC_RESY (s_Fb.yres)

extern uint32_t* DG_ScreenBuffer;

void DG_RunDoom(void *arg);
void DG_DrawFrame(void);
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs(void);
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char * title);

#endif //DOOM_GENERIC
