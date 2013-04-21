#ifndef GLOBALS_H
#define GLOBALS_H

#include "../common/Types.h"
#include "GBA.h"

#define VERBOSE_SWI                  1
#define VERBOSE_UNALIGNED_MEMORY     2
#define VERBOSE_ILLEGAL_WRITE        4
#define VERBOSE_ILLEGAL_READ         8
#define VERBOSE_DMA0                16
#define VERBOSE_DMA1                32
#define VERBOSE_DMA2                64
#define VERBOSE_DMA3               128
#define VERBOSE_UNDEFINED          256
#define VERBOSE_AGBPRINT           512
#define VERBOSE_SOUNDOUTPUT       1024

// DISPCNT @ 0400:0000h
// F	E	D	C	B	A	9	8	7	6	5	4	3	2 1 0
// OW	W1	W0	Obj	BG3	BG2	BG1	BG0	FB	OM	HB	PS	GB	Mode
//---------------------------------------------------------------------------
//bits	name        define                          description
//0-2	Mode		DCNT_MODEx. DCNT_MODE#			Sets video mode. 0, 1, 2 are tiled modes; 3, 4, 5 are bitmap modes.
//  3	GB			DCNT_GB							Is set if cartridge is a GBC game. Read-only.
//  4	PS			DCNT_PAGE						Page select. Modes 4 and 5 can use page flipping for smoother animation. This bit selects the displayed page (and allowing the other one to be drawn on without artifacts).
//  5	HB			DCNT_OAM_HBL					Allows access to OAM in an HBlank. OAM is normally locked in VDraw. Will reduce the amount of sprite pixels rendered per line.
//  6	OM			DCNT_OBJ_1D						Object mapping mode. Tile memory can be seen as a 32x32 matrix of tiles. When sprites are composed of multiple tiles high, this bit tells whether the next row of tiles lies beneath the previous, in correspondence with the matrix structure (2D mapping, OM=0), or right next to it, so that memory is arranged as an array of sprites (1D mapping OM=1). More on this in the sprite chapter.
//  7	FB			DCNT_BLANK						Force a screen blank.
//  8-B	BG0-BG3,Obj	DCNT_BGx,DCNT_OBJ,DCNT_LAYER#	Enables rendering of the corresponding background and sprites.
//  D-F	W0-OW		DCNT_WINx, DCNT_WINOBJ			Enables the use of windows 0, 1 and Object window, respectively. Windows can be used to mask out certain areas (like the lamp did in Zelda:LTTP).
#define DISPCNT_MODE		0x7
#define DISPCNT_GB			(1 << 3)  // 0x08
#define DISPCNT_PS			(1 << 4)  // 0x10
#define DISPCNT_HB			(1 << 5)  // 0x20
#define DISPCNT_OM			(1 << 6)  // 0x40
#define DISPCNT_FB			(1 << 7)  // 0x80
#define DISPCNT_BG0			(1 << 8)  // 0x100
#define DISPCNT_BG1			(1 << 9)  // 0x200
#define DISPCNT_BG2			(1 << 10) // 0x400
#define DISPCNT_BG3			(1 << 11) // 0x800
#define DISPCNT_OBJ			(1 << 12) // 0x1000
#define DISPCNT_W0          (1 << 13) // 0x2000
#define DISPCNT_W1          (1 << 14) // 0x4000
#define DISPCNT_OW          (1 << 15) // 0x8000


// DISPSTAT @ 0400:0004h
// F E D C B A 9 8	7 6	 5	 4	 3	 2 	 1	 0
//        VcT		 -	 VcI HbI VbI VcS HbS VbS
//---------------------------------------------------------------------------
// bits	name	define			description
// 0	VbS		DSTAT_IN_VBL	VBlank status, read only. Will be set inside VBlank, clear in VDraw.
// 1	HbS		DSTAT_IN_HBL	HBlank status, read only. Will be set inside HBlank.
// 2	VcS		DSTAT_IN_VCT	VCount trigger status. Set if the current scanline matches the scanline trigger ( REG_VCOUNT == REG_DISPSTAT{8-F} )
// 3	VbI		DSTAT_VBL_IRQ	VBlank interrupt request. If set, an interrupt will be fired at VBlank.
// 4	HbI		DSTAT_HBL_IRQ	HBlank interrupt request.
// 5	VcI		DSTAT_VCT_IRQ	VCount interrupt request. Fires interrupt if current scanline matches trigger value.
// 8-F	VcT		DSTAT_VCT#		VCount trigger value. If the current scanline is at this value, bit 2 is set and an interrupt is fired if requested.
#define DSTAT_IN_VBL        (1 << 0)  // 0x01
#define DSTAT_IN_HBL        (1 << 1)  // 0x02
#define DSTAT_IN_VCT        (1 << 2)  // 0x04
#define DSTAT_VBL_IRQ       (1 << 3)  // 0x08
#define DSTAT_HBL_IRQ       (1 << 4)  // 0x10
#define DSTAT_VCT_IRQ       (1 << 5)  // 0x20
#define DSTAT_VCT_NUM       (0xFF00)  // 0xFF00


// BGxCNT @ 0400:0008 + 2x
// F E	D	 C B A 9 8	7	 6	 5 4   3 2	 1 0
// Sz	Wr	    SBB		CM	Mos	 -	   CBB	 Pr
//---------------------------------------------------------------------------
// bits	name	define				description
// 0-1	Pr		BG_PRIO#			Priority. Determines drawing order of backgrounds.
// 2-3	CBB		BG_CBB#				Character Base Block. Sets the charblock that serves as the base for character/tile indexing. Values: 0-3.
// 6	Mos		BG_MOSAIC			Mosaic flag. Enables mosaic effect.
// 7	CM		BG_4BPP, BG_8BPP	Color Mode. 16 colors (4bpp) if cleared; 256 colors (8bpp) if set.
// 8-C	SBB		BG_SBB#				Screen Base Block. Sets the screenblock that serves as the base for screen-entry/map indexing. Values: 0-31.
// D	Wr		BG_WRAP				Affine Wrapping flag. If set, affine background wrap around at their edges. Has no effect on regular backgrounds as they wrap around by default.
// E-F	Sz		BG_SIZE#, 			See below Background Size. Regular and affine backgrounds have different sizes available to them. The sizes, in tiles and in pixels, can be found in table 9.5.
//
// regular bg sizes
// Sz-flag	define			(tiles)	(pixels)
// 00		BG_REG_32x32	 32x32	 256x256
// 01		BG_REG_64x32	 64x32	 512x256
// 10		BG_REG_32x64	 32x64	 256x512
// 11		BG_REG_64x64	 64x64	 512x512
//
// affine bg sizes
// Sz-flag	define			(tiles)	(pixels)
// 00		BG_AFF_16x16	 16x16	 128x128
// 01		BG_AFF_32x32	 32x32	 256x256
// 10		BG_AFF_64x64	 64x64	 512x512
// 11		BG_AFF_128x128	128x128	1024x1024
#define BG_PRIO				((1 << 0)|(1 << 1))
#define BG_CBB              ((1 << 2)|(1 << 3))
#define BG_MOSAIC           (1 << 6)
#define BG_8BPP             (1 << 7)
#define BG_SBB              (0x1F00)
#define BG_WRAP             (1 << 13)
#define BG_SIZE             (0xC000)

extern reg_pair reg[45];
extern bool ioReadable[0x400];
extern bool N_FLAG;
extern bool C_FLAG;
extern bool Z_FLAG;
extern bool V_FLAG;
extern bool armState;
extern bool armIrqEnable;
extern u32 armNextPC;
extern int armMode;
extern u32 stop;
extern int saveType;
extern bool useBios;
extern bool skipBios;
extern int frameSkip;
extern bool speedup;
extern bool synchronize;
extern bool cpuDisableSfx;
extern bool cpuIsMultiBoot;
extern bool parseDebug;
extern int layerSettings;
extern int layerEnable;
extern bool speedHack;
extern int cpuSaveType;
extern bool cheatsEnabled;
extern bool mirroringEnable;
extern bool skipSaveGameBattery; // skip battery data when reading save states
extern bool skipSaveGameCheats;  // skip cheat list data when reading save states
extern int customBackdropColor;

extern u8 *bios;
extern u8 *rom;
extern u8 *internalRAM;
extern u8 *workRAM;
extern u8 *paletteRAM;
extern u8 *vram;
extern u8 *pix;
extern u8 *oam;
extern u8 *ioMem;

extern u16 DISPCNT;
extern u16 DISPSTAT;
extern u16 VCOUNT;
extern u16 BG0CNT;
extern u16 BG1CNT;
extern u16 BG2CNT;
extern u16 BG3CNT;
extern u16 BG0HOFS;
extern u16 BG0VOFS;
extern u16 BG1HOFS;
extern u16 BG1VOFS;
extern u16 BG2HOFS;
extern u16 BG2VOFS;
extern u16 BG3HOFS;
extern u16 BG3VOFS;
extern u16 BG2PA;
extern u16 BG2PB;
extern u16 BG2PC;
extern u16 BG2PD;
extern u16 BG2X_L;
extern u16 BG2X_H;
extern u16 BG2Y_L;
extern u16 BG2Y_H;
extern u16 BG3PA;
extern u16 BG3PB;
extern u16 BG3PC;
extern u16 BG3PD;
extern u16 BG3X_L;
extern u16 BG3X_H;
extern u16 BG3Y_L;
extern u16 BG3Y_H;
extern u16 WIN0H;
extern u16 WIN1H;
extern u16 WIN0V;
extern u16 WIN1V;
extern u16 WININ;
extern u16 WINOUT;
extern u16 MOSAIC;
extern u16 BLDMOD;
extern u16 COLEV;
extern u16 COLY;
extern u16 DM0SAD_L;
extern u16 DM0SAD_H;
extern u16 DM0DAD_L;
extern u16 DM0DAD_H;
extern u16 DM0CNT_L;
extern u16 DM0CNT_H;
extern u16 DM1SAD_L;
extern u16 DM1SAD_H;
extern u16 DM1DAD_L;
extern u16 DM1DAD_H;
extern u16 DM1CNT_L;
extern u16 DM1CNT_H;
extern u16 DM2SAD_L;
extern u16 DM2SAD_H;
extern u16 DM2DAD_L;
extern u16 DM2DAD_H;
extern u16 DM2CNT_L;
extern u16 DM2CNT_H;
extern u16 DM3SAD_L;
extern u16 DM3SAD_H;
extern u16 DM3DAD_L;
extern u16 DM3DAD_H;
extern u16 DM3CNT_L;
extern u16 DM3CNT_H;
extern u16 TM0D;
extern u16 TM0CNT;
extern u16 TM1D;
extern u16 TM1CNT;
extern u16 TM2D;
extern u16 TM2CNT;
extern u16 TM3D;
extern u16 TM3CNT;
extern u16 P1;
extern u16 IE;
extern u16 IF;
extern u16 IME;

#endif // GLOBALS_H
