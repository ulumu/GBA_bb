#ifndef GFX_H
#define GFX_H

#include "GBA.h"
#include "Globals.h"

#include "../common/Port.h"

//#define SPRITE_DEBUG

typedef union {
	struct {
#ifdef WORDS_BIGENDIAN
		u8 B3;
		u8 B2;
		u8 B1;
		u8 B0;
#else
		u8 B0;
		u8 B1;
		u8 B2;
		u8 B3;
#endif
	} B;
	struct {
#ifdef WORDS_BIGENDIAN
		u16 W1;
		u16 W0;
#else
		u16 W0;
		u16 W1;
#endif
	} W;
#ifdef WORDS_BIGENDIAN
	volatile u32 I;
#else
	u32 DW;
#endif
} color_t;

//static void gfxDrawTextScreen(u16, u16, u16, u32 *);
//extern void gfxDrawTextScreen(u16 control, u16 hofs, u16 vofs, u32 *line);
//static void gfxDrawSprites(u32 *);
//extern void gfxDrawSprites(u32 *lineOBJ);
extern int gfxBG2Changed;
extern int gfxBG3Changed;

extern int gfxBG2X;
extern int gfxBG2Y;
extern int gfxBG3X;
extern int gfxBG3Y;
extern int gfxLastVCOUNT;

extern void gfxClearArray(u32 *array);

#ifdef GBA_USE_RGBA8888
#define CONVERT_COLOR(line, idx, color) line[idx] = systemColorMap32[color & 0xFFFF]
#else
#define CONVERT_COLOR(line, idx, color) line[idx] = systemColorMap16[color & 0xFFFF]
#endif


#endif // GFX_H
