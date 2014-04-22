#define brightness_switch() \
		switch((READ_U8REG(REG_BLDMOD) >> 6) & 3) \
		{ \
			case 2: \
				color = gfxIncreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]); \
				break; \
			case 3: \
				color = gfxDecreaseBrightness(color, coeff[READ_U8REG(REG_COLY) & 0x1F]); \
				break; \
		}

#define alpha_blend_brightness_switch() \
		if(READ_U8REG(REG_BLDMOD) & top) \
		{ \
			brightness_switch(); \
		} \
		else if(top2 & (READ_U8REG(REG_BLDMOD+1))) \
		{ \
			if(color < 0x80000000) \
			{ \
				GFX_ALPHA_BLEND(color, back, coeff[READ_U8REG(REG_COLEV) & 0x1F], coeff[(READ_U8REG(REG_COLEV+1)) & 0x1F]); \
			} \
		}
