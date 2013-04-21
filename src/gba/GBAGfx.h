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
extern void gfxDrawTextScreen(u16 control, u16 hofs, u16 vofs, u32 *line);

static void gfxDrawRotScreen(u16,
			     u16, u16,
			     u16, u16,
			     u16, u16,
			     u16, u16,
			     int&, int&,
			     int,
			     u32*);
static void gfxDrawRotScreen16Bit(u16,
				  u16, u16,
				  u16, u16,
				  u16, u16,
				  u16, u16,
				  int&, int&,
				  int,
				  u32*);
static void gfxDrawRotScreen256(u16,
				u16, u16,
				u16, u16,
				u16, u16,
				u16, u16,
				int&, int&,
				int,
				u32*);
static void gfxDrawRotScreen16Bit160(u16,
				     u16, u16,
				     u16, u16,
				     u16, u16,
				     u16, u16,
				     int&, int&,
				     int,
				     u32*);
//static void gfxDrawSprites(u32 *);
extern void gfxDrawSprites(u32 *lineOBJ);
static void gfxIncreaseBrightness(u32 *line, int coeff);
static void gfxDecreaseBrightness(u32 *line, int coeff);
static void gfxAlphaBlend(u32 *ta, u32 *tb, int ca, int cb);

void mode0RenderLine(u32 *);
void mode0RenderLineNoWindow(u32 *);
void mode0RenderLineAll(u32 *);

void mode1RenderLine(u32 *);
void mode1RenderLineNoWindow(u32 *);
void mode1RenderLineAll(u32 *);

void mode2RenderLine(u32 *);
void mode2RenderLineNoWindow(u32 *);
void mode2RenderLineAll(u32 *);

void mode3RenderLine(u32 *);
void mode3RenderLineNoWindow(u32 *);
void mode3RenderLineAll(u32 *);

void mode4RenderLine(u32 *);
void mode4RenderLineNoWindow(u32 *);
void mode4RenderLineAll(u32 *);

void mode5RenderLine(u32 *);
void mode5RenderLineNoWindow(u32 *);
void mode5RenderLineAll(u32 *);

extern int coeff[32];
extern u32 line0[240];
extern u32 line1[240];
extern u32 line2[240];
extern u32 line3[240];
extern u32 lineOBJ[240];
extern u32 lineOBJWin[240];
extern bool gfxInWin0[240];
extern bool gfxInWin1[240];
extern int lineOBJpixleft[128];

extern int gfxBG2Changed;
extern int gfxBG3Changed;

extern int gfxBG2X;
extern int gfxBG2Y;
extern int gfxBG3X;
extern int gfxBG3Y;
extern int gfxLastVCOUNT;

extern void gfxClearArray(u32 *array);

static inline void gfxDrawRotScreen(u16 control,
				    u16 x_l, u16 x_h,
				    u16 y_l, u16 y_h,
				    u16 pa,  u16 pb,
				    u16 pc,  u16 pd,
				    int& currentX, int& currentY,
				    int changed,
				    u32 *line)
{
  u16 *palette = (u16 *)paletteRAM;
  u8 *charBase = &vram[((control >> 2) & 0x03) * 0x4000];
  u8 *screenBase = (u8 *)&vram[((control >> 8) & 0x1f) * 0x800];
  int prio = ((control & 3) << 25) + 0x1000000;

  int sizeX = 128;
  int sizeY = 128;
  switch((control >> 14) & 3) {
  case 0:
    break;
  case 1:
    sizeX = sizeY = 256;
    break;
  case 2:
    sizeX = sizeY = 512;
    break;
  case 3:
    sizeX = sizeY = 1024;
    break;
  }

  int maskX = sizeX-1;
  int maskY = sizeY-1;

  int yshift = ((control >> 14) & 3)+4;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = (VCOUNT % mosaicY);
    realX -= y*dmx;
    realY -= y*dmy;
  }

  if(control & 0x2000) {
    for(int x = 0; x < 240; x++) {
      int xxx = (realX >> 8) & maskX;
      int yyy = (realY >> 8) & maskY;

      int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

      int tileX = (xxx & 7);
      int tileY = yyy & 7;

      u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

      line[x] = color ? (READ16LE(&palette[color])|prio): 0x80000000;

      realX += dx;
      realY += dy;
    }
  } else {
    for(int x = 0; x < 240; x++) {
      int xxx = (realX >> 8);
      int yyy = (realY >> 8);

      if(xxx < 0 ||
         yyy < 0 ||
         xxx >= sizeX ||
         yyy >= sizeY) {
        line[x] = 0x80000000;
      } else {
        int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

        int tileX = (xxx & 7);
        int tileY = yyy & 7;

        u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

        line[x] = color ? (READ16LE(&palette[color])|prio): 0x80000000;
      }
      realX += dx;
      realY += dy;
    }
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
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

static inline void gfxDrawRotScreen16Bit(u16 control,
					 u16 x_l, u16 x_h,
					 u16 y_l, u16 y_h,
					 u16 pa,  u16 pb,
					 u16 pc,  u16 pd,
					 int& currentX, int& currentY,
					 int changed,
					 u32 *line)
{
  u16 *screenBase = (u16 *)&vram[0];
  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 240;
  int sizeY = 160;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800)
    startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800)
    startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else
    currentX += dmx;

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = (VCOUNT % mosaicY);
    realX -= y*dmx;
    realY -= y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 ||
       yyy < 0 ||
       xxx >= sizeX ||
       yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
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

static inline void gfxDrawRotScreen256(u16 control,
				       u16 x_l, u16 x_h,
				       u16 y_l, u16 y_h,
				       u16 pa,  u16 pb,
				       u16 pc,  u16 pd,
				       int &currentX, int& currentY,
				       int changed,
				       u32 *line)
{
  u16 *palette = (u16 *)paletteRAM;
  u8 *screenBase = (DISPCNT & DISPCNT_PS) ? &vram[0xA000] : &vram[0x0000];
  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 240;
  int sizeY = 160;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800)
    startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800)
    startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = VCOUNT - (VCOUNT % mosaicY);
    realX = startX + y*dmx;
    realY = startY + y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 ||
         yyy < 0 ||
       xxx >= sizeX ||
       yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      u8 color = screenBase[yyy * 240 + xxx];

      line[x] = color ? (READ16LE(&palette[color])|prio): 0x80000000;
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
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

static inline void gfxDrawRotScreen16Bit160(u16 control,
					    u16 x_l, u16 x_h,
					    u16 y_l, u16 y_h,
					    u16 pa,  u16 pb,
					    u16 pc,  u16 pd,
					    int& currentX, int& currentY,
					    int changed,
					    u32 *line)
{
  u16 *screenBase = (DISPCNT & DISPCNT_PS) ? (u16 *)&vram[0xa000] :
    (u16 *)&vram[0];
  int prio = ((control & 3) << 25) + 0x1000000;
  int sizeX = 160;
  int sizeY = 128;

  int startX = (x_l) | ((x_h & 0x07FF)<<16);
  if(x_h & 0x0800)
    startX |= 0xF8000000;
  int startY = (y_l) | ((y_h & 0x07FF)<<16);
  if(y_h & 0x0800)
    startY |= 0xF8000000;

  int dx = pa & 0x7FFF;
  if(pa & 0x8000)
    dx |= 0xFFFF8000;
  int dmx = pb & 0x7FFF;
  if(pb & 0x8000)
    dmx |= 0xFFFF8000;
  int dy = pc & 0x7FFF;
  if(pc & 0x8000)
    dy |= 0xFFFF8000;
  int dmy = pd & 0x7FFF;
  if(pd & 0x8000)
    dmy |= 0xFFFF8000;

  if(VCOUNT == 0)
    changed = 3;

  if(changed & 1) {
    currentX = (x_l) | ((x_h & 0x07FF)<<16);
    if(x_h & 0x0800)
      currentX |= 0xF8000000;
  } else {
    currentX += dmx;
  }

  if(changed & 2) {
    currentY = (y_l) | ((y_h & 0x07FF)<<16);
    if(y_h & 0x0800)
      currentY |= 0xF8000000;
  } else {
    currentY += dmy;
  }

  int realX = currentX;
  int realY = currentY;

  if(control & 0x40) {
    int mosaicY = ((MOSAIC & 0xF0)>>4) + 1;
    int y = VCOUNT - (VCOUNT % mosaicY);
    realX = startX + y*dmx;
    realY = startY + y*dmy;
  }

  int xxx = (realX >> 8);
  int yyy = (realY >> 8);

  for(int x = 0; x < 240; x++) {
    if(xxx < 0 ||
       yyy < 0 ||
       xxx >= sizeX ||
       yyy >= sizeY) {
      line[x] = 0x80000000;
    } else {
      line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
    }
    realX += dx;
    realY += dy;

    xxx = (realX >> 8);
    yyy = (realY >> 8);
  }

  if(control & 0x40) {
    int mosaicX = (MOSAIC & 0xF) + 1;
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


static inline void gfxDrawOBJWin(u32 *lineOBJWin)
{
  gfxClearArray(lineOBJWin);
  if((layerEnable & 0x9000) == 0x9000) {
    u16 *sprites = (u16 *)oam;
    // u16 *spritePalette = &((u16 *)paletteRAM)[256];
    for(int x = 0; x < 128 ; x++) {
      int lineOBJpix = lineOBJpixleft[x];
      u16 a0 = READ16LE(sprites++);
      u16 a1 = READ16LE(sprites++);
      u16 a2 = READ16LE(sprites++);
      sprites++;

      if (lineOBJpix<=0)
        continue;

      // ignores non OBJ-WIN and disabled OBJ-WIN
      if(((a0 & 0x0c00) != 0x0800) || ((a0 & 0x0300) == 0x0200))
        continue;

      if ((a0 & 0x0c00) == 0x0c00)
        a0 &=0xF3FF;

      if ((a0>>14) == 3)
      {
        a0 &= 0x3FFF;
        a1 &= 0x3FFF;
      }

      int sizeX = 8<<(a1>>14);
      int sizeY = sizeX;

      if ((a0>>14) & 1)
      {
        if (sizeX<32)
          sizeX<<=1;
        if (sizeY>8)
          sizeY>>=1;
      }
      else if ((a0>>14) & 2)
      {
        if (sizeX>8)
          sizeX>>=1;
        if (sizeY<32)
          sizeY<<=1;
      }

      int sy = (a0 & 255);

      if(a0 & 0x0100) {
        int fieldX = sizeX;
        int fieldY = sizeY;
        if(a0 & 0x0200) {
          fieldX <<= 1;
          fieldY <<= 1;
        }
        if((sy+fieldY) > 256)
          sy -= 256;
        int t = VCOUNT - sy;
        if((t >= 0) && (t < fieldY)) {
          int sx = (a1 & 0x1FF);
          int startpix = 0;
          if ((sx+fieldX)> 512)
          {
            startpix=512-sx;
          }
          if((sx < 240) || startpix) {
            lineOBJpix-=8;
            // int t2 = t - (fieldY >> 1);
            int rot = (a1 >> 9) & 0x1F;
            u16 *OAM = (u16 *)oam;
            int dx = READ16LE(&OAM[3 + (rot << 4)]);
            if(dx & 0x8000)
              dx |= 0xFFFF8000;
            int dmx = READ16LE(&OAM[7 + (rot << 4)]);
            if(dmx & 0x8000)
              dmx |= 0xFFFF8000;
            int dy = READ16LE(&OAM[11 + (rot << 4)]);
            if(dy & 0x8000)
              dy |= 0xFFFF8000;
            int dmy = READ16LE(&OAM[15 + (rot << 4)]);
            if(dmy & 0x8000)
              dmy |= 0xFFFF8000;

            int realX = ((sizeX) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx
              + t * dmx;
            int realY = ((sizeY) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy
              + t * dmy;

            // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

            if(a0 & 0x2000) {
              int c = (a2 & 0x3FF);
              if((DISPCNT & DISPCNT_MODE) > 2 && (c < 512))
                continue;
              int inc = 32;
              if(DISPCNT & DISPCNT_OM)
                inc = sizeX >> 2;
              else
                c &= 0x3FE;
              for(int x = 0; x < fieldX; x++) {
                if (x >= startpix)
                  lineOBJpix-=2;
                if (lineOBJpix<0)
                  continue;
                int xxx = realX >> 8;
                int yyy = realY >> 8;

                if(xxx < 0 || xxx >= sizeX ||
                   yyy < 0 || yyy >= sizeY ||
                   sx >= 240) {
                } else {
                  u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
                                    + ((yyy & 7)<<3) + ((xxx >> 3)<<6) +
                                   (xxx & 7))&0x7fff)];
                  if(color) {
                    lineOBJWin[sx] = 1;
                  }
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
              // int palette = (a2 >> 8) & 0xF0;
              for(int x = 0; x < fieldX; x++) {
                if (x >= startpix)
                  lineOBJpix-=2;
                if (lineOBJpix<0)
                  continue;
                int xxx = realX >> 8;
                int yyy = realY >> 8;

                //              if(x == 0 || x == (sizeX-1) ||
                //                 t == 0 || t == (sizeY-1)) {
                //                lineOBJ[sx] = 0x001F | prio;
                //              } else {
                  if(xxx < 0 || xxx >= sizeX ||
                     yyy < 0 || yyy >= sizeY ||
                     sx >= 240) {
                  } else {
                    u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
                                     + ((yyy & 7)<<2) + ((xxx >> 3)<<5) +
                                     ((xxx & 7)>>1))&0x7fff)];
                    if(xxx & 1)
                      color >>= 4;
                    else
                      color &= 0x0F;

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
                  //            }
                sx = (sx+1)&511;
                realX += dx;
                realY += dy;
              }
            }
          }
        }
      } else {
        if((sy+sizeY) > 256)
          sy -= 256;
        int t = VCOUNT - sy;
        if((t >= 0) && (t < sizeY)) {
          int sx = (a1 & 0x1FF);
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
              int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
                + ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7))&0x7fff);
              if(a1 & 0x1000)
                xxx = 7;
              // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
              for(int xx = 0; xx < sizeX; xx++) {
                if (xx >= startpix)
                  lineOBJpix--;
                if (lineOBJpix<0)
                  continue;
                if(sx < 240) {
                  u8 color = vram[address];
                  if(color) {
                    lineOBJWin[sx] = 1;
                  }
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
              int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
                + ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7fff);
              // u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
              // int palette = (a2 >> 8) & 0xF0;
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

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
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

                    if(color) {
                      lineOBJWin[sx] = 1;
                    }
                  }
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
    }
  }
}

static inline u32 gfxIncreaseBrightness(u32 color, int coeff)
{
  color &= 0xffff;
  color = ((color << 16) | color) & 0x3E07C1F;

  color = color + (((0x3E07C1F - color) * coeff) >> 4);
  color &= 0x3E07C1F;

  return (color >> 16) | color;
}

static inline void gfxIncreaseBrightness(u32 *line, int coeff)
{
  for(int x = 0; x < 240; x++) {
    u32 color = *line;
    int r = (color & 0x1F);
    int g = ((color >> 5) & 0x1F);
    int b = ((color >> 10) & 0x1F);

    r = r + (((31 - r) * coeff) >> 4);
    g = g + (((31 - g) * coeff) >> 4);
    b = b + (((31 - b) * coeff) >> 4);
    if(r > 31)
      r = 31;
    if(g > 31)
      g = 31;
    if(b > 31)
      b = 31;
    *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
  }
}

static inline u32 gfxDecreaseBrightness(u32 color, int coeff)
{
  color &= 0xffff;
  color = ((color << 16) | color) & 0x3E07C1F;

  color = color - (((color * coeff) >> 4) & 0x3E07C1F);

  return (color >> 16) | color;
}

static inline void gfxDecreaseBrightness(u32 *line, int coeff)
{
  for(int x = 0; x < 240; x++) {
    u32 color = *line;
    int r = (color & 0x1F);
    int g = ((color >> 5) & 0x1F);
    int b = ((color >> 10) & 0x1F);

    r = r - ((r * coeff) >> 4);
    g = g - ((g * coeff) >> 4);
    b = b - ((b * coeff) >> 4);
    if(r < 0)
      r = 0;
    if(g < 0)
      g = 0;
    if(b < 0)
      b = 0;
    *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
  }
}

static inline u32 gfxAlphaBlend(u32 color, u32 color2, int ca, int cb)
{
  if(color < 0x80000000) {
    color&=0xffff;
    color2&=0xffff;

    color = ((color << 16) | color) & 0x03E07C1F;
    color2 = ((color2 << 16) | color2) & 0x03E07C1F;
    color = ((color * ca) + (color2 * cb)) >> 4;

    if ((ca + cb)>16)
    {
      if (color & 0x20)
        color |= 0x1f;
      if (color & 0x8000)
        color |= 0x7C00;
      if (color & 0x4000000)
        color |= 0x03E00000;
    }

    color &= 0x03E07C1F;
    color = (color >> 16) | color;
  }
  return color;
}

static inline void gfxAlphaBlend(u32 *ta, u32 *tb, int ca, int cb)
{
  for(int x = 0; x < 240; x++) {
    u32 color = *ta;
    if(color < 0x80000000) {
      int r = (color & 0x1F);
      int g = ((color >> 5) & 0x1F);
      int b = ((color >> 10) & 0x1F);
      u32 color2 = (*tb++);
      int r0 = (color2 & 0x1F);
      int g0 = ((color2 >> 5) & 0x1F);
      int b0 = ((color2 >> 10) & 0x1F);

      r = ((r * ca) + (r0 * cb)) >> 4;
      g = ((g * ca) + (g0 * cb)) >> 4;
      b = ((b * ca) + (b0 * cb)) >> 4;

      if(r > 31)
        r = 31;
      if(g > 31)
        g = 31;
      if(b > 31)
        b = 31;

      *ta++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
    } else {
      ta++;
      tb++;
    }
  }
}

#endif // GFX_H
