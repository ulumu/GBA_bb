#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

#include "../common/Port.h"
#include "../System.h"

#define  TRACE_LOG_ENTRY(x, y)
#define  TRACE_LOG_EXIT(x, y)

int coeff[32] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

u32 line0[240];
u32 line1[240];
u32 line2[240];
u32 line3[240];
u32 lineOBJ[240];
u32 lineOBJWin[240];
bool gfxInWin0[240];
bool gfxInWin1[240];
int lineOBJpixleft[128];

int gfxBG2Changed = 0;
int gfxBG3Changed = 0;

int gfxBG2X = 0;
int gfxBG2Y = 0;
int gfxBG3X = 0;
int gfxBG3Y = 0;
int gfxLastVCOUNT = 0;

static const int tileXindex[16]   = {0/2, 1/2, 2/2, 3/2, 4/2, 5/2, 6/2, 7/2, 0/2, 1/2, 2/2, 3/2, 4/2, 5/2, 6/2, 7/2};
static const int tileXflipIdx[16] = {7/2, 6/2, 5/2, 4/2, 3/2, 2/2, 1/2, 0/2, 7/2, 6/2, 5/2, 4/2, 3/2, 2/2, 1/2, 0/2};
static const int tileX8bppindex[16]   = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
static const int tileX8bppflipIdx[16] = {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 0};
static const u8 colorPriority[256] =
{
    0x80,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

#define PALETTE_LOOKUP(l) \
		l->W.W0 = READ16LE(&palette[color]);   \
		l->B.B3 = colorPriority[color] + prio; \
		l++;

static inline void tile4bppX256draw(u16 *screenBase, int xxx, int yyy, int yshift, int prio, u8 *charBaseSrc, u16 *paletteSrc, color_t *line)
{
	const u16 *screenSource = screenBase + 0x400*(xxx>>8)+((xxx&255)>>3) + yshift;
	u16        data    = READ16LE(screenSource);
	int        tile    = (data & 0x3FF) * 32;
	const int *pTileX  = &tileXindex[xxx & 7];
	int        tileY   = yyy & 7;
	const u16 *palette = &paletteSrc[(data>>8) & 0xF0]; // 4BPP palette location setup
	int        shift, pixelCnt;
	u8         color;
	const u8  *charBase;

	if(data & 0x0400)		pTileX = &tileXflipIdx[xxx & 7];
	if(data & 0x0800)		tileY  = 7 - tileY;

	tileY *= 4;
	tile  += tileY;
	charBase = &charBaseSrc[tile];

	shift = (*pTileX & 1) ? 4 : 0;

	pixelCnt = 240;

	if ((xxx & 7) > 0)
	{
		while (xxx & 7)
		{
			color = (charBase[(*pTileX++)] >> shift) & 0x0F;

			shift ^= 4;

			PALETTE_LOOKUP(line);

			xxx++;
			pixelCnt--;
		}

		screenSource++;
		if(xxx == 256)
		{
			screenSource = screenBase + yshift;
			xxx = 0;
		}

		data    = READ16LE(screenSource);
		tile    = (data & 0x3FF) * 32;
		tileY   = yyy & 7;
		palette = &paletteSrc[(data>>8) & 0xF0]; // 4BPP palette location setup

		pTileX = &tileXindex[0];
		if(data & 0x0400)		pTileX = &tileXflipIdx[0];
		if(data & 0x0800)		tileY  = 7 - tileY;

		tileY *= 4;
		tile  += tileY;
		shift  = (*pTileX & 1) ? 4 : 0;

		charBase = &charBaseSrc[tile];
	}

	int blockPixel = (pixelCnt >> 3) * 8;

	for(int x = 0; x < blockPixel; x+=8)
	{
		u8 colorBase = charBase[pTileX[0]];
		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		colorBase = charBase[pTileX[2]];
		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		colorBase = charBase[pTileX[4]];
		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		colorBase = charBase[pTileX[6]];
		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		color = (colorBase >> shift) & 0x0F;
		shift ^= 4;
		PALETTE_LOOKUP(line);

		xxx += 8;
		screenSource = screenBase + ((xxx & 0xFF)>>3) + yshift;

		data    = READ16LE(screenSource);
		tile    = (data & 0x3FF) * 32;
		tileY   = yyy & 7;
		palette = &paletteSrc[(data>>8) & 0xF0]; // 4BPP palette location setup

		pTileX = &tileXindex[0];
		if(data & 0x0400)		pTileX = &tileXflipIdx[0];
		if(data & 0x0800)		tileY  = 7 - tileY;

		tileY *= 4;
		tile  += tileY;
		shift = (*pTileX & 1) ? 4 : 0;

		charBase = &charBaseSrc[tile];
	}

	while (blockPixel < pixelCnt)
	{
		color = (charBase[(*pTileX++)] >> shift) & 0x0F;

		shift ^= 4;

		PALETTE_LOOKUP(line);
		blockPixel++;
	}

}


static inline void tile4bppX512draw(u16 *screenBase, int xxx, int yyy, int yshift, int prio, u8 *charBase, u16 *paletteSrc, color_t *line)
{
	u16 *screenSource  = screenBase + 0x400*(xxx>>8)+((xxx&255)>>3) + yshift;
	u16        data    = READ16LE(screenSource);
	int        tile    = (data & 0x3FF) * 32;
	const int *pTileX  = &tileXindex[xxx & 7];
	int        tileY   = yyy & 7;
	u16       *palette = &paletteSrc[(data>>8) & 0xF0]; // 4BPP palette location setup
	int        shift;

	if(data & 0x0400)		pTileX = &tileXflipIdx[xxx & 7];
	if(data & 0x0800)		tileY  = 7 - tileY;

	tileY *= 4;
	tile  += tileY;

	shift = (*pTileX & 1) ? 4 : 0;

	for(int x = 0; x < 240; x++)
	{
		u8 color = (charBase[tile + (*pTileX++)] >> shift) & 0x0F;

		shift ^= 4;

		PALETTE_LOOKUP(line);

		xxx++;

		if((xxx & 7) == 0)
		{
			screenSource++;

			if(xxx == 256)
			{
				screenSource = screenBase + 0x400 + yshift;
			}
			else if(xxx >= 512)
			{
				xxx = 0;
				screenSource = screenBase + yshift;
			}

			data    = READ16LE(screenSource);
			tile    = (data & 0x3FF) * 32;
			tileY   = yyy & 7;
			palette = &paletteSrc[(data>>8) & 0xF0]; // 4BPP palette location setup

			pTileX = &tileXindex[0];
			if(data & 0x0400)		pTileX = &tileXflipIdx[0];
			if(data & 0x0800)		tileY  = 7 - tileY;

			tileY *= 4;
			tile  += tileY;
			shift = (*pTileX & 1) ? 4 : 0;
		}
	}
}

static inline void tile8bppdraw(u16 *screenBase, int xxx, int yyy, int yshift, int prio, u8 *charBaseSrc, u16 *palette, color_t *line, int sizeX)
{
	u16 *screenSource  = screenBase + 0x400*(xxx>>8)+((xxx&255)>>3) + yshift;
	u16        data    = READ16LE(screenSource);
	int        tile    = (data & 0x3FF) * 64;
	const int *pTileX  = &tileX8bppindex[xxx & 7];
	int        tileY   = yyy & 7;
	const u8  *charBase;
	u8         color;

	if(data & 0x0400)		pTileX = &tileX8bppflipIdx[xxx & 7];
	if(data & 0x0800)		tileY  = 7 - tileY;

	tileY *= 8;
	tile  += tileY;
	charBase = &charBaseSrc[tile];

	int pixelCnt = 240;

	if ((xxx & 7) > 0)
	{
		while (xxx & 7)
		{
			color = charBase[(*pTileX++)];
			PALETTE_LOOKUP(line);

			xxx++;
			pixelCnt--;
		}

		screenSource++;
		if(xxx == 256) {
			if(sizeX > 256)
				screenSource = screenBase + 0x400 + yshift;
			else {
				screenSource = screenBase + yshift;
				xxx = 0;
			}
		} else if(xxx >= sizeX) {
			xxx = 0;
			screenSource = screenBase + yshift;
		}

		data    = READ16LE(screenSource);
		tile    = (data & 0x3FF) * 64;
		tileY   = yyy & 7;

		pTileX = &tileX8bppindex[0];
		if(data & 0x0400)		pTileX = &tileX8bppflipIdx[0];
		if(data & 0x0800)		tileY  = 7 - tileY;

		tileY *= 8;
		tile  += tileY;
		charBase = &charBaseSrc[tile];
	}

	int blockPixel = (pixelCnt >> 3) * 8;

	for(int x = 0; x < blockPixel; x+=8)
	{
		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);

		xxx += 8;

		screenSource++;

		if(xxx == 256) {
			if(sizeX > 256)
				screenSource = screenBase + 0x400 + yshift;
			else {
				screenSource = screenBase + yshift;
				xxx = 0;
			}
		} else if(xxx >= sizeX) {
			xxx = 0;
			screenSource = screenBase + yshift;
		}

		data    = READ16LE(screenSource);
		tile    = (data & 0x3FF) * 64;
		tileY   = yyy & 7;

		pTileX = &tileX8bppindex[0];
		if(data & 0x0400)		pTileX = &tileX8bppflipIdx[0];
		if(data & 0x0800)		tileY  = 7 - tileY;

		tileY *= 8;
		tile  += tileY;
		charBase = &charBaseSrc[tile];
	}

	while (blockPixel < pixelCnt)
	{
		color = charBase[(*pTileX++)];
		PALETTE_LOOKUP(line);
		blockPixel++;
	}

}


void gfxClearArray(u32 *array)
{
	for(int i = 0; i < 240; i++) {
		*array++ = 0x80000000;
	}
}


void gfxDrawSprites(u32 *lineOBJ)
{
	// lineOBJpix is used to keep track of the drawn OBJs
	// and to stop drawing them if the 'maximum number of OBJ per line'
	// has been reached.
	int lineOBJpix = (DISPCNT & DISPCNT_HB) ? 954 : 1226;
	int m=0;

	TRACE_LOG_ENTRY(lineOBJpix, layerEnable);

	gfxClearArray(lineOBJ);

	if(layerEnable & 0x1000)
	{
		u16 *sprites = (u16 *)oam;
		u16 *spritePalette = &((u16 *)paletteRAM)[256];
		int mosaicY = ((MOSAIC & 0xF000)>>12) + 1;
		int mosaicX = ((MOSAIC & 0xF00)>>8) + 1;

		for(int x = 0; x < 128 ; x++)
		{
			u16 a0 = READ16LE(sprites++);
			u16 a1 = READ16LE(sprites++);
			u16 a2 = READ16LE(sprites++);
			sprites++;

			lineOBJpixleft[x]=lineOBJpix;

			lineOBJpix-=2;

			if (lineOBJpix<=0)
				continue;

			if ((a0 & 0x0c00) == 0x0c00)
				a0 &= (~0x0c00);

			if ((a0>>14) == 3)
			{
				a0 &= 0x3FFF;
				a1 &= 0x3FFF;
			}

			int sizeX = 8<<(a1>>14);
			int sizeY = sizeX;

			if ((a0) & (1 << 14))
			{
				if (sizeX<32)
					sizeX<<=1;
				if (sizeY>8)
					sizeY>>=1;
			}
			else if ((a0) & (2 << 14))
			{
				if (sizeX>8)
					sizeX>>=1;
				if (sizeY<32)
					sizeY<<=1;
			}

#ifdef SPRITE_DEBUG
			int maskX = sizeX-1;
			int maskY = sizeY-1;
#endif

			int sy = (a0 & 255);
			int sx = (a1 & 0x1FF);

			// computes ticks used by OBJ-WIN if OBJWIN is enabled
			if (((a0 & 0x0c00) == 0x0800) && (layerEnable & 0x8000))
			{
				if ((a0 & 0x0300) == 0x0300)
				{
					sizeX<<=1;
					sizeY<<=1;
				}
				if((sy+sizeY) > 256)
					sy -= 256;
				if ((sx+sizeX)> 512)
					sx-=512;
				if (sx<0)
				{
					sizeX+=sx;
					sx = 0;
				}
				else if ((sx+sizeX)>240)
					sizeX=240-sx;

				if ((VCOUNT>=sy) && (VCOUNT<sy+sizeY) && (sx<240))
				{
					if (a0 & 0x0100)
						lineOBJpix-=8+2*sizeX;
					else
						lineOBJpix-=sizeX-2;
				}

				continue;
			}
			// else ignores OBJ-WIN if OBJWIN is disabled, and ignored disabled OBJ
			else if(((a0 & 0x0c00) == 0x0800) || ((a0 & 0x0300) == 0x0200))
				continue;

			if (lineOBJpix<0)
				continue;

			if(a0 & 0x0100)
			{
				int fieldX = sizeX;
				int fieldY = sizeY;

				if(a0 & 0x0200)
				{
					fieldX <<= 1;
					fieldY <<= 1;
				}
				if((sy+fieldY) > 256)
					sy -= 256;

				int t = VCOUNT - sy;

				if((t >= 0) && (t < fieldY))
				{
					int startpix = 0;
					if ((sx+fieldX) > 512)
					{
						startpix=512-sx;
					}
					if (lineOBJpix>0)
						if((sx < 240) || startpix)
						{
							lineOBJpix-=8;
							// int t2 = t - (fieldY >> 1);
							int rot  = ((a1 >> 9) & 0x1F) * 16;
							u16 *OAM = (u16 *)oam + rot;
							s16 dx   = READ16LE(&OAM[3]);
							s16 dmx  = READ16LE(&OAM[7]);
							s16 dy   = READ16LE(&OAM[11]);
							s16 dmy  = READ16LE(&OAM[15]);

							if(a0 & 0x1000) {
								t -= (t % mosaicY);
							}

							int realX = ((sizeX) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + t * dmx;
							int realY = ((sizeY) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + t * dmy;

							u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

							if(a0 & 0x2000)
							{
								int c = (a2 & 0x3FF);
								if((DISPCNT & DISPCNT_MODE) > 2 && (c < 512))
									continue;
								int inc = 32;

								if(DISPCNT & DISPCNT_OM)
									inc = sizeX >> 2;
								else
									c &= 0x3FE;

								for(int x = 0; x < fieldX; x++)
								{
									if (x >= startpix)
										lineOBJpix-=2;
									if (lineOBJpix<0)
										continue;
									int xxx = realX >> 8;
									int yyy = realY >> 8;

									if(xxx < 0 || xxx >= sizeX ||
											yyy < 0 || yyy >= sizeY ||
											sx >= 240)
									{ }
									else
									{
										u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
												+ ((yyy & 7)<<3) + ((xxx >> 3)<<6) +
												(xxx & 7))&0x7FFF)];
										if ((color==0) && (((prio >> 25)&3) <
												((lineOBJ[sx]>>25)&3))) {
											lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
											lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										}

										if (a0 & 0x1000) {
											m++;
											if (m==mosaicX)
												m=0;
										}
#ifdef SPRITE_DEBUG
                  if(t == 0 || t == maskY || x == 0 || x == maskX)
                    lineOBJ[sx] = 0x001F;
#endif
									}
									sx = (sx+1)&511;
									realX += dx;
									realY += dy;
								}
							} else {
								int c = (a2 & 0x3FF);
								if((DISPCNT & DISPCNT_MODE) > 2 && (c < 512))
									continue;

								int inc = 32;
								if(DISPCNT & DISPCNT_OM)
									inc = sizeX >> 3;
								int palette = (a2 >> 8) & 0xF0;
								for(int x = 0; x < fieldX; x++) {
									if (x >= startpix)
										lineOBJpix-=2;
									if (lineOBJpix<0)
										continue;
									int xxx = realX >> 8;
									int yyy = realY >> 8;
									if(xxx < 0 || xxx >= sizeX ||
											yyy < 0 || yyy >= sizeY ||
											sx >= 240)
									{ }
									else {
										u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
												+ ((yyy & 7)<<2) + ((xxx >> 3)<<5) +
												((xxx & 7)>>1))&0x7FFF)];
										if(xxx & 1)
											color >>= 4;
										else
											color &= 0x0F;

										if ((color==0) && (((prio >> 25)&3) <
												((lineOBJ[sx]>>25)&3))) {
											lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
											lineOBJ[sx] = READ16LE(&spritePalette[palette+color]) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										}
									}
									if((a0 & 0x1000) && m) {
										m++;
										if (m==mosaicX)
											m=0;
									}

#ifdef SPRITE_DEBUG
								  if(t == 0 || t == maskY || x == 0 || x == maskX)
									lineOBJ[sx] = 0x001F;
#endif
								  sx = (sx+1)&511;
								  realX += dx;
								  realY += dy;

								}
							}
						}
				}
			} // if(a0 & 0x0100)
			else
			{
				if(sy+sizeY > 256)
					sy -= 256;
				int t = VCOUNT - sy;
				if((t >= 0) && (t < sizeY)) {
					int startpix = 0;
					if ((sx+sizeX)> 512)
					{
						startpix=512-sx;
					}
					if((sx < 240) || startpix) {
						lineOBJpix+=2;
						if(a0 & 0x2000) {
							if(a1 & 0x2000)
								t = sizeY - t - 1;
							int c = (a2 & 0x3FF);
							if((DISPCNT & DISPCNT_MODE) > 2 && (c < 512))
								continue;

							int inc = 32;
							if(DISPCNT & DISPCNT_OM) {
								inc = sizeX >> 2;
							} else {
								c &= 0x3FE;
							}
							int xxx = 0;
							if(a1 & 0x1000)
								xxx = sizeX-1;

							if(a0 & 0x1000) {
								t -= (t % mosaicY);
							}

							int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
									+ ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7)) & 0x7FFF);

							if(a1 & 0x1000)
								xxx = 7;
							u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

							for(int xx = 0; xx < sizeX; xx++) {
								if (xx >= startpix)
									lineOBJpix--;
								if (lineOBJpix<0)
									continue;
								if(sx < 240) {
									u8 color = vram[address];
									if ((color==0) && (((prio >> 25)&3) <
											((lineOBJ[sx]>>25)&3))) {
										lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
										if((a0 & 0x1000) && m)
											lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
									} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
										lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
										if((a0 & 0x1000) && m)
											lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
									}

									if (a0 & 0x1000) {
										m++;
										if (m==mosaicX)
											m=0;
									}

#ifdef SPRITE_DEBUG
								  if(t == 0 || t == maskY || xx == 0 || xx == maskX)
									lineOBJ[sx] = 0x001F;
#endif
								}

								sx = (sx+1) & 511;
								if(a1 & 0x1000) {
									xxx--;
									address--;
									if(xxx == -1) {
										address -= 56;
										xxx = 7;
									}
									if(address < 0x10000)
										address += 0x8000;
								} else {
									xxx++;
									address++;
									if(xxx == 8) {
										address += 56;
										xxx = 0;
									}
									if(address > 0x17fff)
										address -= 0x8000;
								}
							}
						} else {
							if(a1 & 0x2000)
								t = sizeY - t - 1;
							int c = (a2 & 0x3FF);
							if((DISPCNT & DISPCNT_MODE) > 2 && (c < 512))
								continue;

							int inc = 32;
							if(DISPCNT & DISPCNT_OM) {
								inc = sizeX >> 3;
							}
							int xxx = 0;
							if(a1 & 0x1000)
								xxx = sizeX - 1;

							if(a0 & 0x1000) {
								t -= (t % mosaicY);
							}

							int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
									+ ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7FFF);
							u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
							int palette = (a2 >> 8) & 0xF0;
							if(a1 & 0x1000) {
								xxx = 7;
								for(int xx = sizeX - 1; xx >= 0; xx--) {
									if (xx >= startpix)
										lineOBJpix--;
									if (lineOBJpix<0)
										continue;
									if(sx < 240) {
										u8 color = vram[address];
										if(xx & 1) {
											color = (color >> 4);
										} else
											color &= 0x0F;

										if ((color==0) && (((prio >> 25)&3) <
												((lineOBJ[sx]>>25)&3))) {
											lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
											lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										}
									}
									if (a0 & 0x1000) {
										m++;
										if (m==mosaicX)
											m=0;
									}
#ifdef SPRITE_DEBUG
								  if(t == 0 || t == maskY || xx == 0 || xx == maskX)
									lineOBJ[sx] = 0x001F;
#endif
								  sx = (sx+1) & 511;
								  xxx--;
								  if(!(xx & 1))
									  address--;
								  if(xxx == -1) {
									  xxx = 7;
									  address -= 28;
								  }
								  if(address < 0x10000)
									  address += 0x8000;
								}
							} else {
								for(int xx = 0; xx < sizeX; xx++) {
									if (xx >= startpix)
										lineOBJpix--;
									if (lineOBJpix<0)
										continue;
									if(sx < 240) {
										u8 color = vram[address];
										if(xx & 1) {
											color = (color >> 4);
										} else
											color &= 0x0F;

										if ((color==0) && (((prio >> 25)&3) <
												((lineOBJ[sx]>>25)&3))) {
											lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;
										} else if((color) && (prio < (lineOBJ[sx]&0xFF000000))) {
											lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
											if((a0 & 0x1000) && m)
												lineOBJ[sx]=(lineOBJ[sx-1] & 0xF9FFFFFF) | prio;

										}
									}
									if (a0 & 0x1000) {
										m++;
										if (m==mosaicX)
											m=0;
									}
#ifdef SPRITE_DEBUG
									if(t == 0 || t == maskY || xx == 0 || xx == maskX)
										lineOBJ[sx] = 0x001F;
#endif
									sx = (sx+1) & 511;
									xxx++;
									if(xx & 1)
										address++;
									if(xxx == 8) {
										address += 28;
										xxx = 0;
									}
									if(address > 0x17fff)
										address -= 0x8000;
								}
							}
						}
					}
				}
			}
		} // for(int x = 0; x < 128 ; x++)
	} // if(layerEnable & 0x1000)

	TRACE_LOG_EXIT(0, 0);
}


void gfxDrawTextScreen(u16 control, u16 hofs, u16 vofs, u32 *colorRow)
{
	u16 *palette    = (u16 *)paletteRAM;
	u8  *charBase   = &vram[((control >> 2) & 0x03) * 0x4000];
	u16 *screenBase = (u16 *)&vram[((control >> 8) & 0x1f) * 0x800];
	u8 prio         = ((control & BG_PRIO) << 1) + 0x1;
	int sizeX       = 256;
	int sizeY       = 256;
	color_t *line;

	line = (color_t *)&colorRow[0];

	switch((control >> 14) & 3) {
	case 0:
		break;
	case 1:
		sizeX = 512;
		break;
	case 2:
		sizeY = 512;
		break;
	case 3:
		sizeX = 512;
		sizeY = 512;
		break;
	}

	int maskX = sizeX-1;
	int maskY = sizeY-1;

	bool mosaicOn = (control & BG_MOSAIC) ? true : false;

	int xxx     = hofs & maskX;
	int yyy     = (vofs + VCOUNT) & maskY;
	int mosaicX = (MOSAIC & 0x000F)+1;
	int mosaicY = ((MOSAIC & 0x00F0)>>4)+1;

	if(mosaicOn) {
		if((VCOUNT % mosaicY) != 0) {
			mosaicY = VCOUNT - (VCOUNT % mosaicY);
			yyy = (vofs + mosaicY) & maskY;
		}
	}

	if(yyy > 255 && sizeY > 256) {
		yyy &= 255;
		screenBase += 0x400;
		if(sizeX > 256)
			screenBase += 0x400;
	}

	int yshift = ((yyy>>3)<<5);

	if((control) & BG_8BPP)
	{
		tile8bppdraw(screenBase, xxx, yyy, yshift, prio, charBase, palette, line, sizeX);
	}
	else
	{
		if (sizeX == 256) tile4bppX256draw(screenBase, xxx, yyy, yshift, prio, charBase, palette, line);
		else              tile4bppX512draw(screenBase, xxx, yyy, yshift, prio, charBase, palette, line);
	}

	if(mosaicOn) {
		if(mosaicX > 1) {
			int m = 1;
			for(int i = 0; i < 239; i++) {
				line[i+1] = line[i];
				m++;
				if(m == mosaicX) {
					m = 1;
					i++;
				}
			}
		}
	}
}

