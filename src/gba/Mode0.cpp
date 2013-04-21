#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

#define  TRACE_LOG_ENTRY(x, y)
#define  TRACE_LOG_EXIT(x, y)

void mode0RenderLine(u32 *lineDest)
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & DISPCNT_FB) {
    for(int x = 0; x < 240; x++) {
    	lineDest[x] = systemColorMap32[0x7FFF];
    }
    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  }

  if(layerEnable & 0x0800) {
    gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  }

  gfxDrawSprites(lineOBJ);

  color_t backdrop;
  if(customBackdropColor == -1) {
    backdrop.W.W0 = (READ16LE(&palette[0]));
    backdrop.B.B3 = 0x30;
  } else {
    backdrop.W.W0 = (customBackdropColor & 0x7FFF);
    backdrop.B.B3 = 0x30;
  }

  color_t color;
  for(int x = 0; x < 240; x++) {
    color = backdrop;
    u8 top = 0x20;

    if(line0[x] < color.DW) {
      color.DW = line0[x];
      top = 0x01;
    }

    if((u8)(line1[x]>>24) < (u8)(color.B.B3)) {
      color.DW = line1[x];
      top = 0x02;
    }

    if((u8)(line2[x]>>24) < (u8)(color.B.B3)) {
      color.DW = line2[x];
      top = 0x04;
    }

    if((u8)(line3[x]>>24) < (u8)(color.B.B3)) {
      color.DW = line3[x];
      top = 0x08;
    }

    if((u8)(lineOBJ[x]>>24) < (u8)(color.B.B3)) {
      color.DW = lineOBJ[x];
      top = 0x10;
    }

    if((top & 0x10) && (color.B.B2 & 0x01)) {
      // semi-transparent OBJ
      u32 back = backdrop.DW;
      u8 top2 = 0x20;

      if((u8)(line0[x]>>24) < (u8)(back >> 24)) {
        back = line0[x];
        top2 = 0x01;
      }

      if((u8)(line1[x]>>24) < (u8)(back >> 24)) {
        back = line1[x];
        top2 = 0x02;
      }

      if((u8)(line2[x]>>24) < (u8)(back >> 24)) {
        back = line2[x];
        top2 = 0x04;
      }

      if((u8)(line3[x]>>24) < (u8)(back >> 24)) {
        back = line3[x];
        top2 = 0x08;
      }

      if(top2 & (BLDMOD>>8))
        color.DW = gfxAlphaBlend(color.DW, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color.DW = gfxIncreaseBrightness(color.DW, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color.DW = gfxDecreaseBrightness(color.DW, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineDest[x] = systemColorMap32[color.W.W0];
  }

}

void mode0RenderLineNoWindow(u32 *lineDest)
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & DISPCNT_FB) {
    for(int x = 0; x < 240; x++) {
    	lineDest[x] = systemColorMap32[0x7FFF];
    }
    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  }

  if(layerEnable & 0x0800) {
    gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  }

  gfxDrawSprites(lineOBJ);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  int effect = (BLDMOD >> 6) & 3;

  for(int x = 0; x < 240; x++) {
    u32 color = backdrop;
    u8 top = 0x20;

    if(line0[x] < color) {
      color = line0[x];
      top = 0x01;
    }

    if(line1[x] < (color & 0xFF000000)) {
      color = line1[x];
      top = 0x02;
    }

    if(line2[x] < (color & 0xFF000000)) {
      color = line2[x];
      top = 0x04;
    }

    if(line3[x] < (color & 0xFF000000)) {
      color = line3[x];
      top = 0x08;
    }

    if(lineOBJ[x] < (color & 0xFF000000)) {
      color = lineOBJ[x];
      top = 0x10;
    }

    if(!(color & 0x00010000)) {
      switch(effect) {
      case 0:
        break;
      case 1:
        {
          if(top & BLDMOD) {
            u32 back = backdrop;
            u8 top2 = 0x20;
            if(line0[x] < back) {
              if(top != 0x01) {
                back = line0[x];
                top2 = 0x01;
              }
            }

            if(line1[x] < (back & 0xFF000000)) {
              if(top != 0x02) {
                back = line1[x];
                top2 = 0x02;
              }
            }

            if(line2[x] < (back & 0xFF000000)) {
              if(top != 0x04) {
                back = line2[x];
                top2 = 0x04;
              }
            }

            if(line3[x] < (back & 0xFF000000)) {
              if(top != 0x08) {
                back = line3[x];
                top2 = 0x08;
              }
            }

            if(lineOBJ[x] < (back & 0xFF000000)) {
              if(top != 0x10) {
                back = lineOBJ[x];
                top2 = 0x10;
              }
            }

            if(top2 & (BLDMOD>>8))
              color = gfxAlphaBlend(color, back,
                                    coeff[COLEV & 0x1F],
                                    coeff[(COLEV >> 8) & 0x1F]);

          }
        }
        break;
      case 2:
        if(BLDMOD & top)
          color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      case 3:
        if(BLDMOD & top)
          color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      }
    } else {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20;

      if(line0[x] < back) {
        back = line0[x];
        top2 = 0x01;
      }

      if(line1[x] < (back & 0xFF000000)) {
        back = line1[x];
        top2 = 0x02;
      }

      if(line2[x] < (back & 0xFF000000)) {
        back = line2[x];
        top2 = 0x04;
      }

      if(line3[x] < (back & 0xFF000000)) {
        back = line3[x];
        top2 = 0x08;
      }

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineDest[x] = systemColorMap32[color & 0xFFFF];
  }

}

void mode0RenderLineAll(u32 *lineDest)
{
	u16 *palette = (u16 *)paletteRAM;

	if(DISPCNT & DISPCNT_FB) {
		for(int x = 0; x < 240; x++) {
			lineDest[x] = systemColorMap32[0x7FFF];
		}
		return;
	}

	bool inWindow0 = false;
	bool inWindow1 = false;

	if(layerEnable & 0x2000)
	{
		u8 v0 = WIN0V >> 8;
		u8 v1 = WIN0V & 255;
		inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
		if(v1 >= v0)
			inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
		else
			inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
	}

	if(layerEnable & 0x4000)
	{
		u8 v0 = WIN1V >> 8;
		u8 v1 = WIN1V & 255;
		inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
		if(v1 >= v0)
			inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
		else
			inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
	}

	if((layerEnable & 0x0100)) {
		gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
	}

	if((layerEnable & 0x0200)) {
		gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
	}

	if((layerEnable & 0x0400)) {
		gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
	}

	if((layerEnable & 0x0800)) {
		gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
	}

	gfxDrawSprites(lineOBJ);
	gfxDrawOBJWin(lineOBJWin);

	color_t *cline0, *cline1, *cline2, *cline3, *clineOBJ;
	color_t backdrop;

	if(customBackdropColor == -1) {
		backdrop.W.W0 = READ16LE(&palette[0]);
	} else {
		backdrop.W.W0 = customBackdropColor & 0x7FFF;
	}
	backdrop.B.B3 = 0x30;

	u8 inWin0Mask = WININ & 0xFF;
	u8 inWin1Mask = WININ >> 8;
	u8 outMask    = WINOUT & 0xFF;

	cline0   = (color_t *)line0;
	cline1   = (color_t *)line1;
	cline2   = (color_t *)line2;
	cline3   = (color_t *)line3;
	clineOBJ = (color_t *)lineOBJ;

	for(int x = 0; x < 240; x++)
	{
		color_t color = backdrop;
		u8 top = 0x20;
		u8 mask = outMask;

		if(!(lineOBJWin[x] & 0x80000000)) {
			mask = WINOUT >> 8;
		}

		if(inWindow1) {
			if(gfxInWin1[x])
				mask = inWin1Mask;
		}

		if(inWindow0) {
			if(gfxInWin0[x]) {
				mask = inWin0Mask;
			}
		}

		if((mask & 1) && (cline0[x].DW < color.DW)) {
			color.DW = cline0[x].DW;
			top = 0x01;
		}

		if((mask & 2) && ((u8)(cline1[x].B.B3) < (u8)(color.B.B3))) {
			color.DW = cline1[x].DW;
			top = 0x02;
		}

		if((mask & 4) && ((u8)(cline2[x].B.B3) < (u8)(color.B.B3))) {
			color.DW = cline2[x].DW;
			top = 0x04;
		}

		if((mask & 8) && ((u8)(cline3[x].B.B3) < (u8)(color.B.B3))) {
			color.DW = cline3[x].DW;
			top = 0x08;
		}

		if((mask & 16) && ((u8)(clineOBJ[x].B.B3) < (u8)(color.B.B3))) {
			color.DW = clineOBJ[x].DW;
			top = 0x10;
		}

		if(color.W.W1 & 0x0001)
		{
			// semi-transparent OBJ
			color_t back = backdrop;
			u8 top2 = 0x20;

			if((mask & 1) && ((u8)(cline0[x].B.B3) < (u8)(back.B.B3))) {
				back.DW = cline0[x].DW;
				top2 = 0x01;
			}

			if((mask & 2) && ((u8)(cline1[x].B.B3) < (u8)(back.B.B3))) {
				back.DW = cline1[x].DW;
				top2 = 0x02;
			}

			if((mask & 4) && ((u8)(cline2[x].B.B3) < (u8)(back.B.B3))) {
				back.DW = cline2[x].DW;
				top2 = 0x04;
			}

			if((mask & 8) && ((u8)(cline3[x].B.B3) < (u8)(back.B.B3))) {
				back.DW = cline3[x].DW;
				top2 = 0x08;
			}

			if(top2 & (BLDMOD>>8))
				color.DW = gfxAlphaBlend(color.DW, back.DW,
						coeff[COLEV & 0x1F],
						coeff[(COLEV >> 8) & 0x1F]);
			else {
				switch((BLDMOD >> 6) & 3) {
				case 2:
					if(BLDMOD & top)
						color.DW = gfxIncreaseBrightness(color.DW, coeff[COLY & 0x1F]);
					break;
				case 3:
					if(BLDMOD & top)
						color.DW = gfxDecreaseBrightness(color.DW, coeff[COLY & 0x1F]);
					break;
				}
			}
		}
		else if(mask & 32)
		{
			// special FX on in the window
			switch((BLDMOD >> 6) & 3) {
			case 0:
				break;
			case 1:
			{
				if(top & BLDMOD) {
					color_t back = backdrop;
					u8 top2 = 0x20;
					if((mask & 1) && (u8)(cline0[x].B.B3) < (u8)(back.B.B3)) {
						if(top != 0x01) {
							back.DW = cline0[x].DW;
							top2 = 0x01;
						}
					}

					if((mask & 2) && (u8)(cline1[x].B.B3) < (u8)(back.B.B3)) {
						if(top != 0x02) {
							back.DW = cline1[x].DW;
							top2 = 0x02;
						}
					}

					if((mask & 4) && (u8)(cline2[x].B.B3) < (u8)(back.B.B3)) {
						if(top != 0x04) {
							back.DW = cline2[x].DW;
							top2 = 0x04;
						}
					}

					if((mask & 8) && (u8)(cline3[x].B.B3) < (u8)(back.B.B3)) {
						if(top != 0x08) {
							back.DW = cline3[x].DW;
							top2 = 0x08;
						}
					}

					if((mask & 16) && (u8)(clineOBJ[x].B.B3) < (u8)(back.B.B3)) {
						if(top != 0x10) {
							back.DW = clineOBJ[x].DW;
							top2 = 0x10;
						}
					}

					if(top2 & (BLDMOD>>8))
						color.DW = gfxAlphaBlend(color.DW, back.DW,
								coeff[COLEV & 0x1F],
								coeff[(COLEV >> 8) & 0x1F]);
				}
			}
			break;
			case 2:
				if(BLDMOD & top)
					color.DW = gfxIncreaseBrightness(color.DW, coeff[COLY & 0x1F]);
				break;
			case 3:
				if(BLDMOD & top)
					color.DW = gfxDecreaseBrightness(color.DW, coeff[COLY & 0x1F]);
				break;
			}
		}

		lineDest[x] = systemColorMap32[color.W.W0];
	}

}
