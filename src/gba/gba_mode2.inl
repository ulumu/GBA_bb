/*
Mode 2 is a 256 colour tiled graphics mode which supports scaling and rotation.
There is no background layer 0 or 1 in this mode. Only background layers 2 and 3.
There are 256 tiles available.
It does not support flipping.

These routines only render a single line at a time, because of the way the GBA does events.
*/
#ifdef GBA_USE_RGBA8888
static void mode2RenderLine (u32 *lineMix)
#else
static void mode2RenderLine (u16 *lineMix)
#endif
{
#ifdef REPORT_VIDEO_MODES
	fprintf(stderr, "MODE 2: Render Line\n");
#endif
	u16 *palette = (u16*)paletteRAM;

	if(LAYER_ENABLE_REG & 0x0400) {
		int changed = gfxBG2Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG2CNT), READ_REG(REG_BG2X_L), READ_REG(REG_BG2X_H), READ_REG(REG_BG2Y_L), READ_REG(REG_BG2Y_H),
				READ_REG(REG_BG2PA), READ_REG(REG_BG2PB), READ_REG(REG_BG2PC), READ_REG(REG_BG2PD), gfxBG2X, gfxBG2Y,
				changed, line[2]);
	}

	if(LAYER_ENABLE_REG & 0x0800) {
		int changed = gfxBG3Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG3CNT), READ_REG(REG_BG3X_L), READ_REG(REG_BG3X_H), READ_REG(REG_BG3Y_L), READ_REG(REG_BG3Y_H),
				READ_REG(REG_BG3PA), READ_REG(REG_BG3PB), READ_REG(REG_BG3PC), READ_REG(REG_BG3PD), gfxBG3X, gfxBG3Y,
				changed, line[3]);
	}

	uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

	for(int x = 0; x < 240; ++x) {
		uint32_t color = backdrop;
		uint8_t top = 0x20;

		uint8_t li2 = (uint8_t)(line[2][x]>>24);
		uint8_t li3 = (uint8_t)(line[3][x]>>24);
		uint8_t li4 = (uint8_t)(line[4][x]>>24);	

		uint8_t r = 	(li3 < li2) ? (li3) : (li2);

		if(li4 < r){
			r = 	(li4);
		}

		if(r < (uint8_t)(color >> 24)) {
			if(r == li2){
				color = line[2][x];
				top = 0x04;
			}else if(r == li3){
				color = line[3][x];
				top = 0x08;
			}else if(r == li4){
				color = line[4][x];
				top = 0x10;

				if(color & 0x00010000) {
					// semi-transparent OBJ
					uint32_t back = backdrop;
					uint8_t top2 = 0x20;

					uint8_t li2 = (uint8_t)(line[2][x]>>24);
					uint8_t li3 = (uint8_t)(line[3][x]>>24);
					uint8_t r = 	(li3 < li2) ? (li3) : (li2);

					if(r < (uint8_t)(back >> 24)) {
						if(r == li2){
							back = line[2][x];
							top2 = 0x04;
						}else if(r == li3){
							back = line[3][x];
							top2 = 0x08;
						}
					}

					alpha_blend_brightness_switch();
				}
			}
		}


		CONVERT_COLOR(lineMix, x, color);
	}
	gfxBG2Changed = 0;
	gfxBG3Changed = 0;
	//gfxLastVCOUNT = READ_REG(REG_VCOUNT);
}


#ifdef GBA_USE_RGBA8888
static void mode2RenderLineNoWindow (u32 *lineMix)
#else
static void mode2RenderLineNoWindow (u16 *lineMix)
#endif
{
#ifdef REPORT_VIDEO_MODES
	fprintf(stderr, "MODE 2: Render Line No Window\n");
#endif
	u16 *palette = (u16*)paletteRAM;

	if(LAYER_ENABLE_REG & 0x0400) {
		int changed = gfxBG2Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG2CNT), READ_REG(REG_BG2X_L), READ_REG(REG_BG2X_H), READ_REG(REG_BG2Y_L), READ_REG(REG_BG2Y_H),
				READ_REG(REG_BG2PA), READ_REG(REG_BG2PB), READ_REG(REG_BG2PC), READ_REG(REG_BG2PD), gfxBG2X, gfxBG2Y,
				changed, line[2]);
	}

	if(LAYER_ENABLE_REG & 0x0800) {
		int changed = gfxBG3Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG3CNT), READ_REG(REG_BG3X_L), READ_REG(REG_BG3X_H), READ_REG(REG_BG3Y_L), READ_REG(REG_BG3Y_H),
				READ_REG(REG_BG3PA), READ_REG(REG_BG3PB), READ_REG(REG_BG3PC), READ_REG(REG_BG3PD), gfxBG3X, gfxBG3Y,
				changed, line[3]);
	}

	uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

	for(int x = 0; x < 240; ++x) {
		uint32_t color = backdrop;
		uint8_t top = 0x20;

		uint8_t li2 = (uint8_t)(line[2][x]>>24);
		uint8_t li3 = (uint8_t)(line[3][x]>>24);
		uint8_t li4 = (uint8_t)(line[4][x]>>24);	

		uint8_t r = 	(li3 < li2) ? (li3) : (li2);

		if(li4 < r){
			r = 	(li4);
		}

		if(r < (uint8_t)(color >> 24)) {
			if(r == li2){
				color = line[2][x];
				top = 0x04;
			}else if(r == li3){
				color = line[3][x];
				top = 0x08;
			}else if(r == li4){
				color = line[4][x];
				top = 0x10;
			}
		}

		if(!(color & 0x00010000)) {
			switch((READ_U8REG(REG_BLDMOD) >> 6) & 3) {
				case 0:
					break;
				case 1:
					if(top & READ_U8REG(REG_BLDMOD))
					{
						uint32_t back = backdrop;
						uint8_t top2 = 0x20;

						if((top != 0x04) && (uint8_t)(line[2][x]>>24) < (uint8_t)(back >> 24)) {
							back = line[2][x];
							top2 = 0x04;
						}

						if((top != 0x08) && (uint8_t)(line[3][x]>>24) < (uint8_t)(back >> 24)) {
							back = line[3][x];
							top2 = 0x08;
						}

						if((top != 0x10) && (uint8_t)(line[4][x]>>24) < (uint8_t)(back >> 24)) {
							back = line[4][x];
							top2 = 0x10;
						}

						if( (top2 & (READ_U8REG(REG_BLDMOD+1))) && color < 0x80000000)
						{
							GFX_ALPHA_BLEND(color, back, coeff[READ_U8REG(REG_COLEV) & 0x1F], coeff[(READ_U8REG(REG_COLEV+1)) & 0x1F]);
						}
					}
					break;
				case 2:
					if(READ_U8REG(REG_BLDMOD) & top)
						color = gfxIncreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]);
					break;
				case 3:
					if(READ_U8REG(REG_BLDMOD) & top)
						color = gfxDecreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]);
					break;
			}
		} else {
			// semi-transparent OBJ
			uint32_t back = backdrop;
			uint8_t top2 = 0x20;

			uint8_t li2 = (uint8_t)(line[2][x]>>24);
			uint8_t li3 = (uint8_t)(line[3][x]>>24);
			uint8_t r = 	(li3 < li2) ? (li3) : (li2);

			if(r < (uint8_t)(back >> 24)) {
				if(r == li2){
					back = line[2][x];
					top2 = 0x04;
				}else if(r == li3){
					back = line[3][x];
					top2 = 0x08;
				}
			}

			alpha_blend_brightness_switch();
		}

		CONVERT_COLOR(lineMix, x, color);
	}
	gfxBG2Changed = 0;
	gfxBG3Changed = 0;
	//gfxLastVCOUNT = READ_REG(REG_VCOUNT);
}


#ifdef GBA_USE_RGBA8888
static void mode2RenderLineAll (u32 *lineMix)
#else
static void mode2RenderLineAll (u16 *lineMix)
#endif
{
#ifdef REPORT_VIDEO_MODES
	fprintf(stderr, "MODE 2: Render Line All\n");
#endif
	u16 *palette = (u16*)paletteRAM;

	bool inWindow0 = false;
	bool inWindow1 = false;

	if(LAYER_ENABLE_REG & 0x2000)
	{
		uint8_t v0 = READ_U8REG(REG_WIN0V+1); // IO_REG[REG_WIN0V] >> 8;
		uint8_t v1 = READ_U8REG(REG_WIN0V);   // IO_REG[REG_WIN0V] & 255;
		inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
#ifndef ORIGINAL_BRANCHES
		uint32_t condition = v1 >= v0;
		int32_t condition_mask = ((condition) | -(condition)) >> 31;
		inWindow0 = (((inWindow0 | (READ_REG(REG_VCOUNT) >= v0 && READ_REG(REG_VCOUNT) < v1)) & condition_mask) | (((inWindow0 | (READ_REG(REG_VCOUNT) >= v0 || READ_REG(REG_VCOUNT) < v1)) & ~(condition_mask))));
#else
		if(v1 >= v0)
			inWindow0 |= (READ_REG(REG_VCOUNT) >= v0 && READ_REG(REG_VCOUNT) < v1);
		else
			inWindow0 |= (READ_REG(REG_VCOUNT) >= v0 || READ_REG(REG_VCOUNT) < v1);
#endif
	}
	if(LAYER_ENABLE_REG & 0x4000)
	{
		uint8_t v0 = READ_U8REG(REG_WIN1V+1); // IO_REG[REG_WIN1V] >> 8;
		uint8_t v1 = READ_U8REG(REG_WIN1V);   // IO_REG[REG_WIN1V] & 255;
		inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
#ifndef ORIGINAL_BRANCHES
		uint32_t condition = v1 >= v0;
		int32_t condition_mask = ((condition) | -(condition)) >> 31;
		inWindow1 = (((inWindow1 | (READ_REG(REG_VCOUNT) >= v0 && READ_REG(REG_VCOUNT) < v1)) & condition_mask) | (((inWindow1 | (READ_REG(REG_VCOUNT) >= v0 || READ_REG(REG_VCOUNT) < v1)) & ~(condition_mask))));
#else
		if(v1 >= v0)
			inWindow1 |= (READ_REG(REG_VCOUNT) >= v0 && READ_REG(REG_VCOUNT) < v1);
		else
			inWindow1 |= (READ_REG(REG_VCOUNT) >= v0 || READ_REG(REG_VCOUNT) < v1);
#endif
	}

	if(LAYER_ENABLE_REG & 0x0400) {
		int changed = gfxBG2Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG2CNT), READ_REG(REG_BG2X_L), READ_REG(REG_BG2X_H), READ_REG(REG_BG2Y_L), READ_REG(REG_BG2Y_H),
				READ_REG(REG_BG2PA), READ_REG(REG_BG2PB), READ_REG(REG_BG2PC), READ_REG(REG_BG2PD), gfxBG2X, gfxBG2Y,
				changed, line[2]);
	}

	if(LAYER_ENABLE_REG & 0x0800) {
		int changed = gfxBG3Changed;
#if 0
		if(gfxLastVCOUNT > READ_REG(REG_VCOUNT))
			changed = 3;
#endif

		gfxDrawRotScreen(READ_REG(REG_BG3CNT), READ_REG(REG_BG3X_L), READ_REG(REG_BG3X_H), READ_REG(REG_BG3Y_L), READ_REG(REG_BG3Y_H),
				READ_REG(REG_BG3PA), READ_REG(REG_BG3PB), READ_REG(REG_BG3PC), READ_REG(REG_BG3PD), gfxBG3X, gfxBG3Y,
				changed, line[3]);
	}

	uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

	uint8_t inWin0Mask = READ_U8REG(REG_WININ);   // IO_REG[REG_WININ] & 0xFF;
	uint8_t inWin1Mask = READ_U8REG(REG_WININ+1); // IO_REG[REG_WININ] >> 8;
	uint8_t outMask    = READ_U8REG(REG_WINOUT);  // IO_REG[REG_WINOUT] & 0xFF;

	for(int x = 0; x < 240; x++) {
		uint32_t color = backdrop;
		uint8_t top = 0x20;
		uint8_t mask = outMask;

		if(!(line[5][x] & 0x80000000)) {
			mask = READ_U8REG(REG_WINOUT+1);  // IO_REG[REG_WINOUT] >> 8;
		}

		int32_t window1_mask = ((inWindow1 & gfxInWin[1][x]) | -(inWindow1 & gfxInWin[1][x])) >> 31;
		int32_t window0_mask = ((inWindow0 & gfxInWin[0][x]) | -(inWindow0 & gfxInWin[0][x])) >> 31;
		mask = (inWin1Mask & window1_mask) | (mask & ~window1_mask);
		mask = (inWin0Mask & window0_mask) | (mask & ~window0_mask);

		if((mask & 4) && line[2][x] < color) {
			color = line[2][x];
			top = 0x04;
		}

		if((mask & 8) && (uint8_t)(line[3][x]>>24) < (uint8_t)(color >> 24)) {
			color = line[3][x];
			top = 0x08;
		}

		if((mask & 16) && (uint8_t)(line[4][x]>>24) < (uint8_t)(color >> 24)) {
			color = line[4][x];
			top = 0x10;
		}

		if(color & 0x00010000) {
			// semi-transparent OBJ
			uint32_t back = backdrop;
			uint8_t top2 = 0x20;

			if((mask & 4) && line[2][x] < back) {
				back = line[2][x];
				top2 = 0x04;
			}

			if((mask & 8) && (uint8_t)(line[3][x]>>24) < (uint8_t)(back >> 24)) {
				back = line[3][x];
				top2 = 0x08;
			}

			alpha_blend_brightness_switch();
		} else if(mask & 32) {
			// special FX on the window
			switch((READ_U8REG(REG_BLDMOD) >> 6) & 3) {
				case 0:
					break;
				case 1:
					if(top & READ_U8REG(REG_BLDMOD))
					{
						uint32_t back = backdrop;
						uint8_t top2 = 0x20;

						if((mask & 4) && (top != 0x04) && line[2][x] < back) {
							back = line[2][x];
							top2 = 0x04;
						}

						if((mask & 8) && (top != 0x08) && (uint8_t)(line[3][x]>>24) < (uint8_t)(back >> 24)) {
							back = line[3][x];
							top2 = 0x08;
						}

						if((mask & 16) && (top != 0x10) && (uint8_t)(line[4][x]>>24) < (uint8_t)(back >> 24)) {
							back = line[4][x];
							top2 = 0x10;
						}

						if( (top2 & (READ_U8REG(REG_BLDMOD+1))) && color < 0x80000000)
						{
							GFX_ALPHA_BLEND(color, back, coeff[READ_U8REG(REG_COLEV) & 0x1F], coeff[(READ_U8REG(REG_COLEV+1)) & 0x1F]);
						}
					}
					break;
				case 2:
					if(READ_U8REG(REG_BLDMOD) & top)
						color = gfxIncreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]);
					break;
				case 3:
					if(READ_U8REG(REG_BLDMOD) & top)
						color = gfxDecreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]);
					break;
			}
		}

		CONVERT_COLOR(lineMix, x, color);
	}
	gfxBG2Changed = 0;
	gfxBG3Changed = 0;
	//gfxLastVCOUNT = READ_REG(REG_VCOUNT);
}
