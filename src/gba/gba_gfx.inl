/*============================================================
	GBA GFX
============================================================ */
static u32 map_widths[]  = { 256, 512, 256, 512 };
static u32 map_heights[] = { 256, 256, 512, 512 };

#define __SIGNEXTOPT__

static INLINE void gfxDrawTextScreen(bool process_layer0, bool process_layer1, bool process_layer2, bool process_layer3)
{
	bool  process_layers[4] = {process_layer0, process_layer1, process_layer2, process_layer3};
	u16	  control_layers[4] = {READ_REG(REG_BG0CNT), READ_REG(REG_BG1CNT), READ_REG(REG_BG2CNT), READ_REG(REG_BG3CNT)};
	u16	  hofs_layers[4]	= {READ_REG(REG_BG0HOFS), READ_REG(REG_BG1HOFS), READ_REG(REG_BG2HOFS), READ_REG(REG_BG3HOFS)};
	u16	  vofs_layers[4]	= {READ_REG(REG_BG0VOFS), READ_REG(REG_BG1VOFS), READ_REG(REG_BG2VOFS), READ_REG(REG_BG3VOFS)};
	u32  *line_layers[4]	= {line[0], line[1], line[2], line[3]};
	
	for(int i = 0; i < 4; i++)
	{
		if(!process_layers[i])
			continue;

		u16 control	= control_layers[i];
		u16 hofs	= hofs_layers[i];
		u16 vofs	= vofs_layers[i];
		u32 * line	= line_layers[i];

		u16 *palette = (u16 *)paletteRAM;
		u8 *charBase = &vram[((control >> 2) & 0x03) << 14];
		u16 *screenBase = (u16 *)&vram[((control >> 8) & 0x1f) << 11];
		u32 prio = ((control & BG_PRIO)<<25) + 0x1000000;

		u32 map_size = (control >> 14) & 3;
		u32 sizeX = map_widths[map_size];
		u32 sizeY = map_heights[map_size];

		int maskX = sizeX-1;
		int maskY = sizeY-1;

		int xxx = hofs & maskX;
		int yyy = (vofs + READ_REG(REG_VCOUNT)) & maskY;
		int mosaicX = (READ_REG(REG_MOSAIC) & 0x000F)+1;
		int mosaicY = ((READ_REG(REG_MOSAIC) & 0x00F0)>>4)+1;

		bool mosaicOn = (control & BG_MOSAIC) ? true : false;

		if(mosaicOn && ((READ_REG(REG_VCOUNT) % mosaicY) != 0))
		{
			mosaicY = READ_REG(REG_VCOUNT) - (READ_REG(REG_VCOUNT) % mosaicY);
			yyy = (vofs + mosaicY) & maskY;
		}

		if(yyy > 255 && sizeY > 256)
		{
			yyy &= 255;
			screenBase += 0x400;
			if(sizeX > 256)
				screenBase += 0x400;
		}

		int yshift = ((yyy>>3)<<5);

		u16 *screenSource = screenBase + ((xxx>>8) << 10) + ((xxx & 255)>>3) + yshift;
		if((control) & 0x80)
		{
			for(u32 x = 0; x < 240u; x++)
			{
				u16 data = READ16LE(screenSource);

				int tile = data & 0x3FF;
				int tileX = (xxx & 7);
				int tileY = yyy & 7;

				if(tileX == 7)
					++screenSource;

				if(data & 0x0400)
					tileX = 7 - tileX;
				if(data & 0x0800)
					tileY = 7 - tileY;

				u8 color = charBase[(tile<<6)  + (tileY<<3) + tileX];

				line[x] = color ? (READ16LE(&palette[color]) | prio): 0x80000000;

				if(++xxx == 256)
				{
					screenSource = screenBase + yshift;
					if(sizeX > 256)
						screenSource += 0x400;
					else
						xxx = 0;
				}
				else if(xxx >= sizeX)
				{
					xxx = 0;
					screenSource = screenBase + yshift;
				}
			}
		}
		else
		{ 
			for(u32 x = 0; x < 240u; ++x)
			{
				u16 data = READ16LE(screenSource);

				int tile = data & 0x3FF;
				int tileX = (xxx & 7);
				int tileY = yyy & 7;

				if(tileX == 7)
					++screenSource;

				if(data & 0x0400)
					tileX = 7 - tileX;
				if(data & 0x0800)
					tileY = 7 - tileY;

				u8 color = charBase[(tile<<5) + (tileY<<2) + (tileX>>1)];

				(tileX & 1) ? color >>= 4 : color &= 0x0F;

				int pal = (data>>8) & 0xF0;
				line[x] = color ? (READ16LE(&palette[pal + color])|prio): 0x80000000;

				if(++xxx == 256)
				{
					screenSource = screenBase + yshift;
					if(sizeX > 256)
						screenSource += 0x400;
					else
						xxx = 0;
				}
				else if(xxx >= sizeX)
				{
					xxx = 0;
					screenSource = screenBase + yshift;
				}
			}
		}

		if(mosaicOn && (mosaicX > 1))
		{
			int m = 1;
			for(u32 i = 0; i < 239u; ++i)
			{
				line[i+1] = line[i];
				++m;
				if(m == mosaicX)
				{
					m = 1;
					++i;
				}
			}
		}
	}
}

static u32 map_sizes_rot[] = { 128, 256, 512, 1024 };

static INLINE void gfxDrawRotScreen(u16 control, u16 x_l, u16 x_h, u16 y_l, u16 y_h,
u16 pa,  u16 pb, u16 pc,  u16 pd, int& currentX, int& currentY, int changed, u32 *line)
{
	u16 *palette = (u16 *)paletteRAM;
	u8 *charBase = &vram[((control >> 2) & 0x03) << 14];
	u8 *screenBase = (u8 *)&vram[((control >> 8) & 0x1f) << 11];
	int prio = ((control & 3) << 25) + 0x1000000;

	u32 map_size = (control >> 14) & 3;
	u32 sizeX = map_sizes_rot[map_size];
	u32 sizeY = map_sizes_rot[map_size];

	int maskX = sizeX-1;
	int maskY = sizeY-1;

	int yshift = ((control >> 14) & 3)+4;

#ifdef __SIGNEXTOPT__
	int16 dx  = ((int16)(pa));
	int16 dmx = ((int16)(pb));
	int16 dy  = ((int16)(pc));
	int16 dmy = ((int16)(pd));
#else
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
#endif

	if(READ_REG(REG_VCOUNT) == 0)
		changed = 3;

	currentX += dmx;
	currentY += dmy;

	if(changed & 1)
	{
		currentX = (x_l) | ((x_h & 0x07FF)<<16);
		if(x_h & 0x0800)
			currentX |= 0xF8000000;
	}

	if(changed & 2)
	{
		currentY = (y_l) | ((y_h & 0x07FF)<<16);
		if(y_h & 0x0800)
			currentY |= 0xF8000000;
	}

	int realX = currentX;
	int realY = currentY;

	if(control & 0x40)
	{
		int mosaicY = ((READ_REG(REG_MOSAIC) & 0xF0)>>4) + 1;
		int y = (READ_REG(REG_VCOUNT) % mosaicY);
		realX -= y*dmx;
		realY -= y*dmy;
	}

	memset(line, -1, 240 * sizeof(u32));
	if(control & 0x2000)
	{
		for(u32 x = 0; x < 240u; ++x)
		{
			int xxx = (realX >> 8) & maskX;
			int yyy = (realY >> 8) & maskY;

			int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

			int tileX = (xxx & 7);
			int tileY = yyy & 7;

			u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

			if(color)
				line[x] = (READ16LE(&palette[color])|prio);

			realX += dx;
			realY += dy;
		}
	}
	else
	{
		for(u32 x = 0; x < 240u; ++x)
		{
			unsigned xxx = (realX >> 8);
			unsigned yyy = (realY >> 8);

			if(xxx < sizeX && yyy < sizeY)
			{
				int tile = screenBase[(xxx>>3) + ((yyy>>3)<<yshift)];

				int tileX = (xxx & 7);
				int tileY = yyy & 7;

				u8 color = charBase[(tile<<6) + (tileY<<3) + tileX];

				if(color)
					line[x] = (READ16LE(&palette[color])|prio);
			}

			realX += dx;
			realY += dy;
		}
	}

	if(control & 0x40)
	{
		int mosaicX = (READ_REG(REG_MOSAIC) & 0xF) + 1;
		if(mosaicX > 1)
		{
			int m = 1;
			for(u32 i = 0; i < 239u; ++i)
			{
				line[i+1] = line[i];
				if(++m == mosaicX)
				{
					m = 1;
					++i;
				}
			}
		}
	}
}

static INLINE void gfxDrawRotScreen16Bit( int& currentX,  int& currentY, int changed)
{
	u16 *screenBase = (u16 *)&vram[0];
	int prio = ((READ_REG(REG_BG2CNT) & 3) << 25) + 0x1000000;

	u32 sizeX = 240;
	u32 sizeY = 160;

//	int startX = (READ_REG(REG_BG2X_L)) | ((READ_REG(REG_BG2X_H) & 0x07FF)<<16);
//	if(READ_REG(REG_BG2X_H) & 0x0800)
//		startX |= 0xF8000000;
//	int startY = (READ_REG(REG_BG2Y_L)) | ((READ_REG(REG_BG2Y_H) & 0x07FF)<<16);
//	if(READ_REG(REG_BG2Y_H) & 0x0800)
//		startY |= 0xF8000000;

#ifdef __SIGNEXTOPT__
	int16 dx  = ((int16)(READ_SREG(REG_BG2PA)));
	int16 dmx = ((int16)(READ_SREG(REG_BG2PB)));
	int16 dy  = ((int16)(READ_SREG(REG_BG2PC)));
	int16 dmy = ((int16)(READ_SREG(REG_BG2PD)));
#else
	int dx = READ_REG(REG_BG2PA) & 0x7FFF;
	if(READ_REG(REG_BG2PA) & 0x8000)
		dx |= 0xFFFF8000;
	int dmx = READ_REG(REG_BG2PB) & 0x7FFF;
	if(READ_REG(REG_BG2PB) & 0x8000)
		dmx |= 0xFFFF8000;
	int dy = READ_REG(REG_BG2PC) & 0x7FFF;
	if(READ_REG(REG_BG2PC) & 0x8000)
		dy |= 0xFFFF8000;
	int dmy = READ_REG(REG_BG2PD) & 0x7FFF;
	if(READ_REG(REG_BG2PD) & 0x8000)
		dmy |= 0xFFFF8000;
#endif

	if(READ_REG(REG_VCOUNT) == 0)
		changed = 3;

	currentX += (int)dmx;
	currentY += (int)dmy;

	if(changed & 1)
	{
		currentX = (READ_REG(REG_BG2X_L)) | ((READ_REG(REG_BG2X_H) & 0x07FF)<<16);
		if(READ_REG(REG_BG2X_H) & 0x0800)
			currentX |= 0xF8000000;
	}

	if(changed & 2)
	{
		currentY = (READ_REG(REG_BG2Y_L)) | ((READ_REG(REG_BG2Y_H) & 0x07FF)<<16);
		if(READ_REG(REG_BG2Y_H) & 0x0800)
			currentY |= 0xF8000000;
	}

	int realX = currentX;
	int realY = currentY;

	if(READ_REG(REG_BG2CNT) & 0x40) {
		int mosaicY = ((READ_REG(REG_MOSAIC) & 0xF0)>>4) + 1;
		int y = (READ_REG(REG_VCOUNT) % mosaicY);
		realX -= y* ((int)dmx);
		realY -= y* ((int)dmy);
	}

	unsigned xxx = (realX >> 8);
	unsigned yyy = (realY >> 8);

	memset(line[2], -1, 240 * sizeof(u32));
	for(u32 x = 0; x < 240u; ++x)
	{
		if(xxx < sizeX && yyy < sizeY)
			line[2][x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);

		realX += (int)dx;
		realY += (int)dy;

		xxx = (realX >> 8);
		yyy = (realY >> 8);
	}

	if(READ_REG(REG_BG2CNT) & 0x40) {
		int mosaicX = (READ_REG(REG_MOSAIC) & 0xF) + 1;
		if(mosaicX > 1) {
			int m = 1;
			for(u32 i = 0; i < 239u; ++i)
			{
				line[2][i+1] = line[2][i];
				if(++m == mosaicX)
				{
					m = 1;
					++i;
				}
			}
		}
	}
}

static INLINE void gfxDrawRotScreen256(int &currentX, int& currentY, int changed)
{
	u16 *palette = (u16 *)paletteRAM;
	u8 *screenBase = (READ_REG(REG_DISPCNT) & 0x0010) ? &vram[0xA000] : &vram[0x0000];
	int prio = ((READ_REG(REG_BG2CNT) & 3) << 25) + 0x1000000;
	u32 sizeX = 240;
	u32 sizeY = 160;

	int startX = (READ_REG(REG_BG2X_L)) | ((READ_REG(REG_BG2X_H) & 0x07FF)<<16);
	if(READ_REG(REG_BG2X_H) & 0x0800)
		startX |= 0xF8000000;
	int startY = (READ_REG(REG_BG2Y_L)) | ((READ_REG(REG_BG2Y_H) & 0x07FF)<<16);
	if(READ_REG(REG_BG2Y_H) & 0x0800)
		startY |= 0xF8000000;

#ifdef __SIGNEXTOPT__
	int16 dx  = ((int16)(READ_SREG(REG_BG2PA)));
	int16 dmx = ((int16)(READ_SREG(REG_BG2PB)));
	int16 dy  = ((int16)(READ_SREG(REG_BG2PC)));
	int16 dmy = ((int16)(READ_SREG(REG_BG2PD)));
#else
	int dx = READ_REG(REG_BG2PA) & 0x7FFF;
	if(READ_REG(REG_BG2PA) & 0x8000)
		dx |= 0xFFFF8000;
	int dmx = READ_REG(REG_BG2PB) & 0x7FFF;
	if(READ_REG(REG_BG2PB) & 0x8000)
		dmx |= 0xFFFF8000;
	int dy = READ_REG(REG_BG2PC) & 0x7FFF;
	if(READ_REG(REG_BG2PC) & 0x8000)
		dy |= 0xFFFF8000;
	int dmy = READ_REG(REG_BG2PD) & 0x7FFF;
	if(READ_REG(REG_BG2PD) & 0x8000)
		dmy |= 0xFFFF8000;
#endif

	if(READ_REG(REG_VCOUNT) == 0)
		changed = 3;

	currentX += (int)dmx;
	currentY += (int)dmy;

	if(changed & 1)
	{
		currentX = (READ_REG(REG_BG2X_L)) | ((READ_REG(REG_BG2X_H) & 0x07FF)<<16);
		if(READ_REG(REG_BG2X_H) & 0x0800)
			currentX |= 0xF8000000;
	}

	if(changed & 2)
	{
		currentY = (READ_REG(REG_BG2Y_L)) | ((READ_REG(REG_BG2Y_H) & 0x07FF)<<16);
		if(READ_REG(REG_BG2Y_H) & 0x0800)
			currentY |= 0xF8000000;
	}

	int realX = currentX;
	int realY = currentY;

	if(READ_REG(REG_BG2CNT) & 0x40) {
		int mosaicY = ((READ_REG(REG_MOSAIC) & 0xF0)>>4) + 1;
		int y = READ_REG(REG_VCOUNT) - (READ_REG(REG_VCOUNT) % mosaicY);
		realX = startX + y* ((int)dmx);
		realY = startY + y* ((int)dmy);
	}

	int xxx = (realX >> 8);
	int yyy = (realY >> 8);

	memset(line[2], -1, 240 * sizeof(u32));
	for(u32 x = 0; x < 240 && yyy >= 0; ++x)
	{
		u8 color = screenBase[yyy * 240 + xxx];
		if(unsigned(xxx) < sizeX && unsigned(yyy) < sizeY && color)
			line[2][x] = (READ16LE(&palette[color])|prio);
		realX += (int)dx;
		realY += (int)dy;

		xxx = (realX >> 8);
		yyy = (realY >> 8);
	}

	if(READ_REG(REG_BG2CNT) & 0x40)
	{
		int mosaicX = (READ_REG(REG_MOSAIC) & 0xF) + 1;
		if(mosaicX > 1)
		{
			int m = 1;
			for(u32 i = 0; i < 239u; ++i)
			{
				line[2][i+1] = line[2][i];
				if(++m == mosaicX)
				{
					m = 1;
					++i;
				}
			}
		}
	}
}

static INLINE void gfxDrawRotScreen16Bit160(int& currentX, int& currentY, int changed)
{
	u16 *screenBase = (READ_REG(REG_DISPCNT) & 0x0010) ? (u16 *)&vram[0xa000] :
		(u16 *)&vram[0];
	int prio = ((READ_REG(REG_BG2CNT) & 3) << 25) + 0x1000000;
	u32 sizeX = 160;
	u32 sizeY = 128;

	int startX = (READ_REG(REG_BG2X_L)) | ((READ_REG(REG_BG2X_H) & 0x07FF)<<16);
	if(READ_REG(REG_BG2X_H) & 0x0800)
		startX |= 0xF8000000;
	int startY = (READ_REG(REG_BG2Y_L)) | ((READ_REG(REG_BG2Y_H) & 0x07FF)<<16);
	if(READ_REG(REG_BG2Y_H) & 0x0800)
		startY |= 0xF8000000;

#ifdef __SIGNEXTOPT__
	int16 dx  = ((int16)(READ_SREG(REG_BG2PA)));
	int16 dmx = ((int16)(READ_SREG(REG_BG2PB)));
	int16 dy  = ((int16)(READ_SREG(REG_BG2PC)));
	int16 dmy = ((int16)(READ_SREG(REG_BG2PD)));
#else
	int dx = READ_REG(REG_BG2PA) & 0x7FFF;
	if(READ_REG(REG_BG2PA) & 0x8000)
		dx |= 0xFFFF8000;
	int dmx = READ_REG(REG_BG2PB) & 0x7FFF;
	if(READ_REG(REG_BG2PB) & 0x8000)
		dmx |= 0xFFFF8000;
	int dy = READ_REG(REG_BG2PC) & 0x7FFF;
	if(READ_REG(REG_BG2PC) & 0x8000)
		dy |= 0xFFFF8000;
	int dmy = READ_REG(REG_BG2PD) & 0x7FFF;
	if(READ_REG(REG_BG2PD) & 0x8000)
		dmy |= 0xFFFF8000;
#endif

	if(READ_REG(REG_VCOUNT) == 0)
		changed = 3;

	currentX += (int)dmx;
	currentY += (int)dmy;

	if(changed & 1)
	{
		currentX = (IO_REG[REG_BG2X_L]) | ((IO_REG[REG_BG2X_H] & 0x07FF)<<16);
		if(IO_REG[REG_BG2X_H] & 0x0800)
			currentX |= 0xF8000000;
	}

	if(changed & 2)
	{
		currentY = (IO_REG[REG_BG2Y_L]) | ((IO_REG[REG_BG2Y_H] & 0x07FF)<<16);
		if(IO_REG[REG_BG2Y_H] & 0x0800)
			currentY |= 0xF8000000;
	}

	int realX = currentX;
	int realY = currentY;

	if(READ_REG(REG_BG2CNT) & 0x40) {
		int mosaicY = ((READ_REG(REG_MOSAIC) & 0xF0)>>4) + 1;
		int y = READ_REG(REG_VCOUNT) - (READ_REG(REG_VCOUNT) % mosaicY);
		realX = startX + y* ((int)dmx);
		realY = startY + y* ((int)dmy);
	}

	int xxx = (realX >> 8);
	int yyy = (realY >> 8);

	memset(line[2], -1, 240 * sizeof(u32));
	for(u32 x = 0; x < 240u; ++x)
	{
		if(unsigned(xxx) < sizeX && unsigned(yyy) < sizeY)
			line[2][x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);

		realX += (int)dx;
		realY += (int)dy;

		xxx = (realX >> 8);
		yyy = (realY >> 8);
	}


	int mosaicX = (READ_REG(REG_MOSAIC) & 0xF) + 1;
	if( (READ_REG(REG_BG2CNT) & 0x40) && (mosaicX > 1))
	{
		int m = 1;
		for(u32 i = 0; i < 239u; ++i)
		{
			line[2][i+1] = line[2][i];
			if(++m == mosaicX)
			{
				m = 1;
				++i;
			}
		}
	}
}

/* lineOBJpix is used to keep track of the drawn OBJs
   and to stop drawing them if the 'maximum number of OBJ per line'
   has been reached. */

static INLINE void gfxDrawSprites (void)
{
	unsigned lineOBJpix, m;

	lineOBJpix = (READ_REG(REG_DISPCNT) & 0x20) ? 954 : 1226;
	m = 0;

	u16 *sprites = (u16 *)oam;
	u16 *spritePalette = &((u16 *)paletteRAM)[256];
	int mosaicY = ((READ_REG(REG_MOSAIC) & 0xF000)>>12) + 1;
	int mosaicX = ((READ_REG(REG_MOSAIC) & 0xF00)>>8) + 1;
	for(u32 x = 0; x < 128; x++)
	{
		u16 a0 = READ16LE(sprites++);
		u16 a1 = READ16LE(sprites++);
		u16 a2 = READ16LE(sprites++);
		++sprites;

		lineOBJpixleft[x]=lineOBJpix;

		lineOBJpix-=2;
		if (lineOBJpix<=0)
			return;

		if ((a0 & 0x0c00) == 0x0c00)
			a0 &=0xF3FF;

		u16 a0val = a0>>14;

		if (a0val == 3)
		{
			a0 &= 0x3FFF;
			a1 &= 0x3FFF;
		}

		u32 sizeX = 8<<(a1>>14);
		u32 sizeY = sizeX;


		if (a0val & 1)
		{
			if (sizeX<32)
				sizeX<<=1;
			if (sizeY>8)
				sizeY>>=1;
		}
		else if (a0val & 2)
		{
			if (sizeX>8)
				sizeX>>=1;
			if (sizeY<32)
				sizeY<<=1;
		}


		int sy = (a0 & 255);
		int sx = (a1 & 0x1FF);

		// computes ticks used by OBJ-WIN if OBJWIN is enabled
		if (((a0 & 0x0c00) == 0x0800) && (LAYER_ENABLE_REG & 0x8000))
		{
			if ((a0 & 0x0300) == 0x0300)
			{
				sizeX<<=1;
				sizeY<<=1;
			}

			if((sy+sizeY) > 256)
				sy -= 256;
			if ((sx+sizeX)> 512)
				sx -= 512;

			if (sx < 0)
			{
				sizeX+=sx;
				sx = 0;
			}
			else if ((sx+sizeX)>240)
				sizeX=240-sx;

			if ((READ_REG(REG_VCOUNT)>=sy) && (READ_REG(REG_VCOUNT)<sy+sizeY) && (sx<240))
			{
				lineOBJpix -= (sizeX-2);

				if (a0 & 0x0100)
					lineOBJpix -= (10+sizeX); 
			}
			continue;
		}

		// else ignores OBJ-WIN if OBJWIN is disabled, and ignored disabled OBJ
		else if(((a0 & 0x0c00) == 0x0800) || ((a0 & 0x0300) == 0x0200))
			continue;

		if(a0 & 0x0100)
		{
			u32 fieldX = sizeX;
			u32 fieldY = sizeY;
			if(a0 & 0x0200)
			{
				fieldX <<= 1;
				fieldY <<= 1;
			}
			if((sy+fieldY) > 256)
				sy -= 256;
			int t = READ_REG(REG_VCOUNT) - sy;
			if(unsigned(t) < fieldY)
			{
				u32 startpix = 0;
				if ((sx+fieldX)> 512)
					startpix=512-sx;

				if (lineOBJpix && ((sx < 240) || startpix))
				{
					lineOBJpix-=8;
					int rot = (((a1 >> 9) & 0x1F) << 4);
					u16 *OAM = (u16 *)oam;
#ifdef __SIGNEXTOPT__
					int16 dx  = *(int16 *)(&OAM[3 + rot]);
					int16 dmx = *(int16 *)(&OAM[7 + rot]);
					int16 dy  = *(int16 *)(&OAM[11 + rot]);
					int16 dmy = *(int16 *)(&OAM[15 + rot]);
#else
					int dx = READ16LE(&OAM[3 + rot]);
					if(dx & 0x8000)
						dx |= 0xFFFF8000;
					int dmx = READ16LE(&OAM[7 + rot]);
					if(dmx & 0x8000)
						dmx |= 0xFFFF8000;
					int dy = READ16LE(&OAM[11 + rot]);
					if(dy & 0x8000)
						dy |= 0xFFFF8000;
					int dmy = READ16LE(&OAM[15 + rot]);
					if(dmy & 0x8000)
						dmy |= 0xFFFF8000;
#endif
					if(a0 & 0x1000)
						t -= (t % mosaicY);

					int realX = ((sizeX) << 7) - (fieldX >> 1)*dx + ((t - (fieldY>>1))* dmx);
					int realY = ((sizeY) << 7) - (fieldX >> 1)*dy + ((t - (fieldY>>1))* dmy);

					u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

					int c = (a2 & 0x3FF);
					if((READ_REG(REG_DISPCNT) & 7) > 2 && (c < 512))
						continue;

					if(a0 & 0x2000)
					{
						int inc = 32;
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 2;
						else
							c &= 0x3FE;
						for(u32 x = 0; x < fieldX; x++)
						{
							if (x >= startpix)
								lineOBJpix-=2;
							unsigned xxx = realX >> 8;
							unsigned yyy = realY >> 8;
							if(xxx < sizeX && yyy < sizeY && sx < 240)
							{

								u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
								+ ((yyy & 7)<<3) + ((xxx >> 3)<<6) + (xxx & 7))&0x7FFF)];

								if ((color==0) && (((prio >> 25)&3) < ((line[4][sx]>>25)&3)))
								{
									line[4][sx] = (line[4][sx] & 0xF9FFFFFF) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}
								else if((color) && (prio < (line[4][sx]&0xFF000000)))
								{
									line[4][sx] = READ16LE(&spritePalette[color]) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}

								if ((a0 & 0x1000) && ((m+1) == mosaicX))
									m = 0;
							}
							sx = (sx+1)&511;
							realX += dx;
							realY += dy;
						}
					}
					else
					{
						int inc = 32;
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 3;
						int palette = (a2 >> 8) & 0xF0;
						for(u32 x = 0; x < fieldX; ++x)
						{
							if (x >= startpix)
								lineOBJpix-=2;
							unsigned xxx = realX >> 8;
							unsigned yyy = realY >> 8;
							if(xxx < sizeX && yyy < sizeY && sx < 240)
							{

								u32 color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
											+ ((yyy & 7)<<2) + ((xxx >> 3)<<5)
											+ ((xxx & 7)>>1))&0x7FFF)];
								if(xxx & 1)
									color >>= 4;
								else
									color &= 0x0F;

								if ((color==0) && (((prio >> 25)&3) <
											((line[4][sx]>>25)&3)))
								{
									line[4][sx] = (line[4][sx] & 0xF9FFFFFF) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}
								else if((color) && (prio < (line[4][sx]&0xFF000000)))
								{
									line[4][sx] = READ16LE(&spritePalette[palette+color]) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}
							}
							if((a0 & 0x1000) && m)
							{
								if (++m==mosaicX)
									m=0;
							}

							sx = (sx+1)&511;
							realX += dx;
							realY += dy;

						}
					}
				}
			}
		}
		else
		{
			if(sy+sizeY > 256)
				sy -= 256;
			int t = READ_REG(REG_VCOUNT) - sy;
			if(unsigned(t) < sizeY)
			{
				u32 startpix = 0;
				if ((sx+sizeX)> 512)
					startpix=512-sx;

				if((sx < 240) || startpix)
				{
					lineOBJpix+=2;

					if(a1 & 0x2000)
						t = sizeY - t - 1;

					int c = (a2 & 0x3FF);
					if((READ_REG(REG_DISPCNT) & 7) > 2 && (c < 512))
						continue;

					int inc = 32;
					int xxx = 0;
					if(a1 & 0x1000)
						xxx = sizeX-1;

					if(a0 & 0x1000)
						t -= (t % mosaicY);

					if(a0 & 0x2000)
					{
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 2;
						else
							c &= 0x3FE;

						int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
									+ ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7)) & 0x7FFF);

						if(a1 & 0x1000)
							xxx = 7;
						u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);

						for(u32 xx = 0; xx < sizeX; xx++)
						{
							if (xx >= startpix)
								--lineOBJpix;
							if(sx < 240)
							{
								u8 color = vram[address];
								if ((color==0) && (((prio >> 25)&3) <
											((line[4][sx]>>25)&3)))
								{
									line[4][sx] = (line[4][sx] & 0xF9FFFFFF) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}
								else if((color) && (prio < (line[4][sx]&0xFF000000)))
								{
									line[4][sx] = READ16LE(&spritePalette[color]) | prio;
									if((a0 & 0x1000) && m)
										line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
								}

								if ((a0 & 0x1000) && ((m+1) == mosaicX))
									m = 0;
							}

							sx = (sx+1) & 511;
							if(a1 & 0x1000)
							{
								--address;
								if(--xxx == -1)
								{
									address -= 56;
									xxx = 7;
								}
								if(address < 0x10000)
									address += 0x8000;
							}
							else
							{
								++address;
								if(++xxx == 8)
								{
									address += 56;
									xxx = 0;
								}
								if(address > 0x17fff)
									address -= 0x8000;
							}
						}
					}
					else
					{
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 3;

						int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
									+ ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7FFF);

						u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
						int palette = (a2 >> 8) & 0xF0;
						if(a1 & 0x1000)
						{
							xxx = 7;
							int xx = sizeX - 1;
							do
							{
								if (xx >= (int)(startpix))
									--lineOBJpix;
								//if (lineOBJpix<0)
								//  continue;
								if(sx < 240)
								{
									u8 color = vram[address];
									if(xx & 1)
										color >>= 4;
									else
										color &= 0x0F;

									if ((color==0) && (((prio >> 25)&3) <
												((line[4][sx]>>25)&3)))
									{
										line[4][sx] = (line[4][sx] & 0xF9FFFFFF) | prio;
										if((a0 & 0x1000) && m)
											line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
									}
									else if((color) && (prio < (line[4][sx]&0xFF000000)))
									{
										line[4][sx] = READ16LE(&spritePalette[palette + color]) | prio;
										if((a0 & 0x1000) && m)
											line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
									}
								}

								if ((a0 & 0x1000) && ((m+1) == mosaicX))
									m=0;

								sx = (sx+1) & 511;
								if(!(xx & 1))
									--address;
								if(--xxx == -1)
								{
									xxx = 7;
									address -= 28;
								}
								if(address < 0x10000)
									address += 0x8000;
							}while(--xx >= 0);
						}
						else
						{
							for(u32 xx = 0; xx < sizeX; ++xx)
							{
								if (xx >= startpix)
									--lineOBJpix;
								//if (lineOBJpix<0)
								//  continue;
								if(sx < 240)
								{
									u8 color = vram[address];
									if(xx & 1)
										color >>= 4;
									else
										color &= 0x0F;

									if ((color==0) && (((prio >> 25)&3) <
												((line[4][sx]>>25)&3)))
									{
										line[4][sx] = (line[4][sx] & 0xF9FFFFFF) | prio;
										if((a0 & 0x1000) && m)
											line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;
									}
									else if((color) && (prio < (line[4][sx]&0xFF000000)))
									{
										line[4][sx] = READ16LE(&spritePalette[palette + color]) | prio;
										if((a0 & 0x1000) && m)
											line[4][sx]=(line[4][sx-1] & 0xF9FFFFFF) | prio;

									}
								}
								if ((a0 & 0x1000) && ((m+1) == mosaicX))
									m=0;

								sx = (sx+1) & 511;
								if(xx & 1)
									++address;
								if(++xxx == 8)
								{
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

static INLINE void gfxDrawOBJWin (void)
{
	u16 *sprites = (u16 *)oam;
	for(int x = 0; x < 128 ; x++)
	{
		int lineOBJpix = lineOBJpixleft[x];
		u16 a0 = READ16LE(sprites++);
		u16 a1 = READ16LE(sprites++);
		u16 a2 = READ16LE(sprites++);
		sprites++;

		if (lineOBJpix<=0)
			return;

		// ignores non OBJ-WIN and disabled OBJ-WIN
		if(((a0 & 0x0c00) != 0x0800) || ((a0 & 0x0300) == 0x0200))
			continue;

		u16 a0val = a0>>14;

		if ((a0 & 0x0c00) == 0x0c00)
			a0 &=0xF3FF;

		if (a0val == 3)
		{
			a0 &= 0x3FFF;
			a1 &= 0x3FFF;
		}

		int sizeX = 8<<(a1>>14);
		int sizeY = sizeX;

		if (a0val & 1)
		{
			if (sizeX<32)
				sizeX<<=1;
			if (sizeY>8)
				sizeY>>=1;
		}
		else if (a0val & 2)
		{
			if (sizeX>8)
				sizeX>>=1;
			if (sizeY<32)
				sizeY<<=1;
		}

		int sy = (a0 & 255);

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
			int t = READ_REG(REG_VCOUNT) - sy;
			if((t >= 0) && (t < fieldY))
			{
				int sx = (a1 & 0x1FF);
				int startpix = 0;
				if ((sx+fieldX)> 512)
					startpix=512-sx;

				if((sx < 240) || startpix)
				{
					lineOBJpix-=8;
					// int t2 = t - (fieldY >> 1);
					int rot = (a1 >> 9) & 0x1F;
					u16 *OAM = (u16 *)oam;
#ifdef __SIGNEXTOPT__
					int16 dx  = *(int16 *)(&OAM[3 + (rot << 4)]);
					int16 dmx = *(int16 *)(&OAM[7 + (rot << 4)]);
					int16 dy  = *(int16 *)(&OAM[11 + (rot << 4)]);
					int16 dmy = *(int16 *)(&OAM[15 + (rot << 4)]);
#else
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
#endif
					int realX = ((sizeX) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + t * dmx;
					int realY = ((sizeY) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + t * dmy;

					int c = (a2 & 0x3FF);
					if((READ_REG(REG_DISPCNT) & 7) > 2 && (c < 512))
						continue;

					int inc = 32;
					bool condition1 = a0 & 0x2000;

					if(READ_REG(REG_DISPCNT) & 0x40)
						inc = sizeX >> 3;

					for(int x = 0; x < fieldX; x++)
					{
						bool cont = true;
						if (x >= startpix)
							lineOBJpix-=2;
						if (lineOBJpix<0)
							continue;
						int xxx = realX >> 8;
						int yyy = realY >> 8;

						if(xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
							cont = false;

						if(cont)
						{
							u32 color;
							if(condition1)
								color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
											+ ((yyy & 7)<<3) + ((xxx >> 3)<<6) +
											(xxx & 7))&0x7fff)];
							else
							{
								color = vram[0x10000 + ((((c + (yyy>>3) * inc)<<5)
											+ ((yyy & 7)<<2) + ((xxx >> 3)<<5) +
											((xxx & 7)>>1))&0x7fff)];
								if(xxx & 1)
									color >>= 4;
								else
									color &= 0x0F;
							}

							if(color)
								line[5][sx] = 1;
						}
						sx = (sx+1)&511;
						realX += dx;
						realY += dy;
					}
				}
			}
		}
		else
		{
			if((sy+sizeY) > 256)
				sy -= 256;
			int t = READ_REG(REG_VCOUNT) - sy;
			if((t >= 0) && (t < sizeY))
			{
				int sx = (a1 & 0x1FF);
				int startpix = 0;
				if ((sx+sizeX)> 512)
					startpix=512-sx;

				if((sx < 240) || startpix)
				{
					lineOBJpix+=2;
					if(a1 & 0x2000)
						t = sizeY - t - 1;
					int c = (a2 & 0x3FF);
					if((READ_REG(REG_DISPCNT) & 7) > 2 && (c < 512))
						continue;
					if(a0 & 0x2000)
					{

						int inc = 32;
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 2;
						else
							c &= 0x3FE;

						int xxx = 0;
						if(a1 & 0x1000)
							xxx = sizeX-1;
						int address = 0x10000 + ((((c+ (t>>3) * inc) << 5)
									+ ((t & 7) << 3) + ((xxx>>3)<<6) + (xxx & 7))&0x7fff);
						if(a1 & 0x1000)
							xxx = 7;
						for(int xx = 0; xx < sizeX; xx++)
						{
							if (xx >= startpix)
								lineOBJpix--;
							if (lineOBJpix<0)
								continue;
							if(sx < 240)
							{
								u8 color = vram[address];
								if(color)
									line[5][sx] = 1;
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
					}
					else
					{
						int inc = 32;
						if(READ_REG(REG_DISPCNT) & 0x40)
							inc = sizeX >> 3;
						int xxx = 0;
						if(a1 & 0x1000)
							xxx = sizeX - 1;
						int address = 0x10000 + ((((c + (t>>3) * inc)<<5)
									+ ((t & 7)<<2) + ((xxx>>3)<<5) + ((xxx & 7) >> 1))&0x7fff);
						// u32 prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00)<<6);
						// int palette = (a2 >> 8) & 0xF0;
						if(a1 & 0x1000)
						{
							xxx = 7;
							for(int xx = sizeX - 1; xx >= 0; xx--)
							{
								if (xx >= startpix)
									lineOBJpix--;
								if (lineOBJpix<0)
									continue;
								if(sx < 240)
								{
									u8 color = vram[address];
									if(xx & 1)
										color = (color >> 4);
									else
										color &= 0x0F;

									if(color)
										line[5][sx] = 1;
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
						}
						else
						{
							for(int xx = 0; xx < sizeX; xx++)
							{
								if (xx >= startpix)
									lineOBJpix--;
								if (lineOBJpix<0)
									continue;
								if(sx < 240)
								{
									u8 color = vram[address];
									if(xx & 1)
										color = (color >> 4);
									else
										color &= 0x0F;

									if(color)
										line[5][sx] = 1;
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

static INLINE u32 gfxIncreaseBrightness(u32 color, int coeff)
{
   color = (((color & 0xffff) << 16) | (color & 0xffff)) & 0x3E07C1F;
   color += ((((0x3E07C1F - color) * coeff) >> 4) & 0x3E07C1F);
   return (color >> 16) | color;
}

static INLINE u32 gfxDecreaseBrightness(u32 color, int coeff)
{
   color = (((color & 0xffff) << 16) | (color & 0xffff)) & 0x3E07C1F;
   color -= (((color * coeff) >> 4) & 0x3E07C1F);
   return (color >> 16) | color;
}

static u32 AlphaClampLUT[64] = 
{
 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F
};  

#define GFX_ALPHA_BLEND(color, color2, ca, cb) \
	int r = AlphaClampLUT[(((color & 0x1F) * ca) >> 4) + (((color2 & 0x1F) * cb) >> 4)]; \
	int g = AlphaClampLUT[((((color >> 5) & 0x1F) * ca) >> 4) + ((((color2 >> 5) & 0x1F) * cb) >> 4)]; \
	int b = AlphaClampLUT[((((color >> 10) & 0x1F) * ca) >> 4) + ((((color2 >> 10) & 0x1F) * cb) >> 4)]; \
	color = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
