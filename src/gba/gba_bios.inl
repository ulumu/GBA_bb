/*============================================================
	BIOS
============================================================ */
#include <math.h>

#ifdef __DEBUG
#define BIOS_SLOG SLOG
#else
#define BIOS_SLOG(fmt, ...)
#endif

s16 sineTable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};


static bool isAddressValid(u32 source, u32 dest)
{
	switch(source & 0x0F000000)
	{
	case 0x01000000:
	case 0x0F000000:
		SLOG("Source address exceed allowable range: %X", source);
		return false;

	default:
		break;
	}

	switch(dest & 0x0F000000)
	{
	case 0x00000000:
	case 0x01000000:
	case 0x08000000:
	case 0x09000000:
	case 0x0A000000:
	case 0x0B000000:
	case 0x0C000000:
	case 0x0F000000:
		SLOG("Dest address exceed allowable range: %X", dest);
		return false;

	default:
		break;
	}

	return true;
}

/*-----------------------------------------------------------------
 SWI(0x05) - VBlankIntrWait
  Continues to wait in Halt state until one (or more) of the specified interrupt(s) do occur.
  The function forcefully sets IME=1. When using multiple interrupts at the same time,
  this function is having less overhead than repeatedly calling the Halt function
-----------------------------------------------------------------*/
#define BIOS_VBLANK_INTR_WAIT()  BIOS_IntrWait(true,1)


/*-----------------------------------------------------------------
 SWI(0x06) - Div
  Signed Division, r0/r1.

    r0  signed 32bit Number
    r1  signed 32bit Denom

  Return:

    r0  Number DIV Denom ;signed
    r1  Number MOD Denom ;signed
    r3  ABS (Number DIV Denom) ;unsigned

  For example, incoming -1234, 10 should return -123, -4, +123.
  The function usually gets caught in an endless loop upon division by zero.
-----------------------------------------------------------------*/
static void BIOS_Div (void)
{
	int number = bus.reg[0].I;
	int denom  = bus.reg[1].I;

	if(denom != 0)
	{
		bus.reg[0].I = number / denom;
		bus.reg[1].I = number % denom;
		s32 temp = (s32)bus.reg[0].I;
		bus.reg[3].I = temp < 0 ? (u32)-temp : (u32)temp;
	}
}

/*-----------------------------------------------------------------
 SWI(0x07) - DivArm
  Same as above (SWI 06h Div), but incoming parameters are exchanged, r1/r0 (r0=Denom, r1=number).
  For compatibility with ARM's library.
-----------------------------------------------------------------*/
static void BIOS_DivArm (void)
{
	int number = bus.reg[1].I;
	int denom  = bus.reg[0].I;

	if(denom != 0)
	{
		bus.reg[0].I = number / denom;
		bus.reg[1].I = number % denom;
		s32 temp = (s32)bus.reg[0].I;
		bus.reg[3].I = temp < 0 ? (u32)-temp : (u32)temp;
	}
}


/*-----------------------------------------------------------------
 SWI(0x08) - Sqrt
  Calculate square root.
-----------------------------------------------------------------*/
#define BIOS_SQRT() bus.reg[0].I = (u32)sqrt((double)bus.reg[0].I);


/*-----------------------------------------------------------------
 SWI(0x09) - ArcTan
  Calculates the arc tangent.
-----------------------------------------------------------------*/
static void BIOS_ArcTan (void)
{
	s32 a =  -(((s32)(bus.reg[0].I*bus.reg[0].I)) >> 14);
	s32 b = ((0xA9 * a) >> 14) + 0x390;
	b = ((b * a) >> 14) + 0x91C;
	b = ((b * a) >> 14) + 0xFB6;
	b = ((b * a) >> 14) + 0x16AA;
	b = ((b * a) >> 14) + 0x2081;
	b = ((b * a) >> 14) + 0x3651;
	b = ((b * a) >> 14) + 0xA2F9;
	a = ((s32)bus.reg[0].I * b) >> 16;
	bus.reg[0].I = a;
}

/*-----------------------------------------------------------------
 SWI(0x0A) - ArcTan2
  Calculates the arc tangent after correction processing.
-----------------------------------------------------------------*/
static void BIOS_ArcTan2 (void)
{
	s32 x = bus.reg[0].I;
	s32 y = bus.reg[1].I;
	u32 res = 0;
	if (y == 0)
		res = ((x>>16) & 0x8000);
	else
	{
		if (x == 0)
			res = ((y>>16) & 0x8000) + 0x4000;
		else
		{
			if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x<0) && (y<0)))))
			{
				bus.reg[1].I = x;
				bus.reg[0].I = y << 14;
				BIOS_Div();
				BIOS_ArcTan();
				if (x < 0)
					res = 0x8000 + bus.reg[0].I;
				else
					res = (((y>>16) & 0x8000)<<1) + bus.reg[0].I;
			}
			else
			{
				bus.reg[0].I = x << 14;
				BIOS_Div();
				BIOS_ArcTan();
				res = (0x4000 + ((y>>16) & 0x8000)) - bus.reg[0].I;
			}
		}
	}
	bus.reg[0].I = res;
}


/*-----------------------------------------------------------------
 SWI(0x0B) - CpuSet
  Memory copy/fill in units of 4 bytes or 2 bytes.
  Memcopy is implemented as repeated LDMIA/STMIA [Rb]!,r3 or LDRH/STRH r3,[r0,r5] instructions.
  Memfill as single LDMIA or LDRH followed by repeated STMIA [Rb]!,r3 or STRH r3,[r0,r5].
  The length must be a multiple of 4 bytes (32bit mode) or 2 bytes (16bit mode).
  The (half)wordcount in r2 must be length/4 (32bit mode) or length/2 (16bit mode),
  ie. length in word/halfword units rather than byte units.
-----------------------------------------------------------------*/
static void BIOS_CpuSet (void)
{
	u32 source = bus.reg[0].I;
	u32 dest   = bus.reg[1].I;
	u32 cnt    = bus.reg[2].I;
	int count  = cnt & 0x1FFFFF;

	if (false == isAddressValid(source, dest))
		return;

	// 32-bit ?
	if((cnt >> 26) & 1)
	{
		// needed for 32-bit mode!
		source &= 0xFFFFFFFC;
		dest   &= 0xFFFFFFFC;

		// fill ?
		if((cnt >> 24) & 1) {
			u32 destEnd = dest + count * 4;
			u32 value   = (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source));
			while(dest < destEnd) {
				CPUWriteMem32Direct(dest, value);
				dest += 4;
			}
		} else {
			// copy
			u32 sourceEnd = source + count * 4;
			u32 destEnd   = dest   + count * 4;

			if (sourceEnd > 0x0EFFFFFF) sourceEnd = 0x0EFFFFFF;

			while(source < sourceEnd) {
				CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
				source += 4;
				dest   += 4;
			}

			while (dest < destEnd)
			{
				CPUWriteMem32Direct(dest, 0x1CAD1CAD);
				dest += 4;
			}
		}
	}
	else
	{
		// 16-bit fill?
		if((cnt >> 24) & 1) {
			u32 destEnd = dest + count * 2;
			u16 value   = (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source));
			while(dest < destEnd) {
				CPUWriteMem16Direct(dest, value);
				dest += 2;
			}
		} else {
			// copy
			u32 sourceEnd = source + count * 2;
			u32 destEnd   = dest   + count * 2;

			if (sourceEnd > 0x0EFFFFFF) sourceEnd = 0x0EFFFFFF;

			while(source < sourceEnd) {
				CPUWriteMem16Direct(dest, CPUReadMem16Direct(source));
				source += 2;
				dest   += 2;
			}

			while (dest < destEnd)
			{
				CPUWriteMem16Direct(dest, 0x1CAD);
				dest += 2;
			}
		}
	}
}

/*-----------------------------------------------------------------
 SWI(0x0C) - CpuFastSet
  Memory copy/fill in units of 32 bytes. Memcopy is implemented as repeated LDMIA/STMIA [Rb]!,r2-r9 instructions.
  Memfill as single LDR followed by repeated STMIA [Rb]!,r2-r9.
  After processing all 32-byte-blocks, the NDS additionally processes the remaining words as 4-byte blocks.

  The length is specified as wordcount, ie. the number of bytes divided by 4.
  On the GBA, the length must be a multiple of 8 words (32 bytes).
-----------------------------------------------------------------*/
static void BIOS_CpuFastSet (void)
{
	u32 source = bus.reg[0].I;
	u32 dest   = bus.reg[1].I;
	u32 cnt    = bus.reg[2].I;
	int count  = cnt & 0x1FFFFF;

	// needed for 32-bit mode!
	source &= 0xFFFFFFFC;
	dest   &= 0xFFFFFFFC;

	// This check is added because problem is seen in Super Mario Advanced 4 during startup screen.
	if (false == isAddressValid(source, dest))
		return;

	// fill?
	if((cnt >> 24) & 1) {
		u32 destEnd = dest + count * 4;
		u32 value   = (source>0x0EFFFFFF ? 0xBAFFFFFB : CPUReadMemory(source));

		while(dest < destEnd) {
			// BIOS always transfers 32 bytes at a time
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
			CPUWriteMem32Direct(dest, value);
			dest += 4;
		}
	} else {
		// copy
		u32 srcEnd  = source + count * 4;
		u32 destEnd = dest + count * 4;

		if (srcEnd > 0x0EFFFFFF) srcEnd = 0x0EFFFFFF;

		while(source < srcEnd) {
			// BIOS always transfers 32 bytes at a time
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
			CPUWriteMem32Direct(dest, CPUReadMem32Direct(source));
			source += 4;
			dest   += 4;
		}
		while (dest < destEnd)
		{
			CPUWriteMem32Direct(dest, 0xBAFFFFFB);
			dest += 4;
		}

	}
}

/*-----------------------------------------------------------------
 SWI(0x0D) - GetBiosChecksum
  Calculates the checksum of the BIOS ROM (by reading in 32bit units, and adding up these values).
  IRQ and FIQ are disabled during execution.
  The checksum is BAAE187Fh (GBA and GBA SP), or BAAE1880h (DS in GBA mode, whereas the only difference
  is that the byte at [3F0Ch] is changed from 00h to 01h, otherwise the BIOS is 1:1 same as GBA BIOS,
  it does even include multiboot code).
-----------------------------------------------------------------*/
#define BIOS_GET_BIOS_CHECKSUM()	bus.reg[0].I=0xBAAE187F;

/*-----------------------------------------------------------------
 SWI(0x0E) - BgAffineSet
  Used to calculate BG Rotation/Scaling parameters.
-----------------------------------------------------------------*/
static void BIOS_BgAffineSet (void)
{
	u32 src  = bus.reg[0].I;
	u32 dest = bus.reg[1].I;
	int num  = bus.reg[2].I;

	if (false == isAddressValid(src, dest))
		return;

	while(num--)
	{
		s32 cx    = CPUReadMem32Direct(src);
		src+=4;
		s32 cy    = CPUReadMem32Direct(src);
		src+=4;
		s16 dispx = CPUReadMem16Direct(src);
		src+=2;
		s16 dispy = CPUReadMem16Direct(src);
		src+=2;
		s16 rx    = CPUReadMem16Direct(src);
		src+=2;
		s16 ry    = CPUReadMem16Direct(src);
		src+=2;
		u16 theta = CPUReadMem16Direct(src)>>8;
		src+=4; // keep structure alignment
		s32 a = sineTable[(theta+0x40)&255];
		s32 b = sineTable[theta];

		s16 dx  =  (rx * a)>>14;
		s16 dmx =  (rx * b)>>14;
		s16 dy  =  (ry * b)>>14;
		s16 dmy =  (ry * a)>>14;

		CPUWriteMem16Direct(dest, dx);
		dest += 2;
		CPUWriteMem16Direct(dest, -dmx);
		dest += 2;
		CPUWriteMem16Direct(dest, dy);
		dest += 2;
		CPUWriteMem16Direct(dest, dmy);
		dest += 2;

		s32 startx = cx - dx * dispx + dmx * dispy;
		s32 starty = cy - dy * dispx - dmy * dispy;

		CPUWriteMem32Direct(dest, startx);
		dest += 4;
		CPUWriteMem32Direct(dest, starty);
		dest += 4;
	}
}

/*-----------------------------------------------------------------
 SWI(0x0F) - ObjAffineSet
  Calculates and sets the OBJ's affine parameters from the scaling ratio and angle of rotation.
  The affine parameters are calculated from the parameters set in Srcp.
  The four affine parameters are set every Offset bytes, starting from the Destp address.
  If the Offset value is 2, the parameters are stored contiguously. If the value is 8, they match the structure of OAM.
  When Srcp is arrayed, the calculation can be performed continuously by specifying Num.
-----------------------------------------------------------------*/
static void BIOS_ObjAffineSet (void)
{
	u32 src    = bus.reg[0].I;
	u32 dest   = bus.reg[1].I;
	int num    = bus.reg[2].I;
	int offset = bus.reg[3].I;

	if (false == isAddressValid(src, dest))
		return;

	for(int i = 0; i < num; i++) {
		s16 rx    = CPUReadMem16Direct(src);
		src+=2;
		s16 ry    = CPUReadMem16Direct(src);
		src+=2;
		u16 theta = CPUReadMem16Direct(src);
		theta >>= 8;
		src+=4; // keep structure alignment

		s32 a = (s32)sineTable[(theta+0x40)&255];
		s32 b = (s32)sineTable[theta];

		s16 dx =  ((s32)rx * a)>>14;
		s16 dmx = ((s32)rx * b)>>14;
		s16 dy =  ((s32)ry * b)>>14;
		s16 dmy = ((s32)ry * a)>>14;

		CPUWriteMem16Direct(dest, dx);
		dest += offset;
		CPUWriteMem16Direct(dest, -dmx);
		dest += offset;
		CPUWriteMem16Direct(dest, dy);
		dest += offset;
		CPUWriteMem16Direct(dest, dmy);
		dest += offset;
	}
}


/*-----------------------------------------------------------------
 SWI(0x10) - BitUnPack
  Used to increase the color depth of bitmaps or tile data. For example, to convert a 1bit
  monochrome font into 4bit or 8bit GBA tiles.
  The Unpack Info is specified separately, allowing to convert the same source data into different formats.
-----------------------------------------------------------------*/
static void BIOS_BitUnPack (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;
	u32 header = bus.reg[2].I;

	int len = CPUReadHalfWord(header);
	// check address
	if(((source & 0xe000000) == 0) ||
			((source + len) & 0xe000000) == 0)
		return;

	int bits = CPUReadByte(header+2);
	int revbits = 8 - bits;
	// u32 value = 0;
	u32 base = CPUReadMemory(header+4);
	bool addBase = (base & 0x80000000) ? true : false;
	base &= 0x7fffffff;
	int dataSize = CPUReadByte(header+3);

	int data = 0;
	int bitwritecount = 0;
	while(1)
	{
		len -= 1;
		if(len < 0)
			break;
		int mask = 0xff >> revbits;
		u8 b = CPUReadByte(source);
		source++;
		int bitcount = 0;
		while(1) {
			if(bitcount >= 8)
				break;
			u32 d = b & mask;
			u32 temp = d >> bitcount;
			if(d || addBase) {
				temp += base;
			}
			data |= temp << bitwritecount;
			bitwritecount += dataSize;
			if(bitwritecount >= 32) {
				CPUWriteMemory(dest, data);
				dest += 4;
				data = 0;
				bitwritecount = 0;
			}
			mask <<= bits;
			bitcount += bits;
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x11) - LZ77UnCompWram
  Expands LZ77-compressed data. The Wram function is faster, and writes in units of 8bits.
  For the Vram function the destination must be halfword aligned, data is written in units of 16bits.
  If the size of the compressed data is not a multiple of 4, please adjust it as much as possible
  by padding with 0.

  Align the source address to a 4-Byte boundary.
-----------------------------------------------------------------*/
static void BIOS_LZ77UnCompWram (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int len = header >> 8;

	while(len > 0) {
		u8 d = CPUReadByte(source);
		source++;

		if(d) {
			for(int i = 0; i < 8; i++) {
				if(d & 0x80) {
					u16 data = CPUReadByte(source) << 8;
					source++;
					data |= CPUReadByte(source);
					source++;
					int length = (data >> 12) + 3;
					int offset = (data & 0x0FFF);
					u32 windowOffset = dest - offset - 1;
					CPUWriteMem8Setup(dest);
					CPUReadMem8Setup(windowOffset);
					for(int i2 = 0; i2 < length; i2++) {
						CPUWriteMemFast(dest, CPUReadMemFast(windowOffset));
						windowOffset++;
						dest++;
						len--;
						if(len == 0)
							return;
					}
				} else {
					CPUWriteByte(dest, CPUReadByte(source));
					dest++;
					source++;
					len--;
					if(len == 0)
						return;
				}
				d <<= 1;
			}
		} else {
			CPUReadMem8Setup(source);
			CPUWriteMem8Setup(dest);
			for(int i = 0; i < 8; i++) {
				CPUWriteMemFast(dest, CPUReadMemFast(source));
				dest++;
				source++;
				len--;
				if(len == 0)
					return;
			}
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x12) - LZ77UnCompVram
  Expands LZ77-compressed data. The Wram function is faster, and writes in units of 8bits.
  For the Vram function the destination must be halfword aligned, data is written in units of 16bits.
  If the size of the compressed data is not a multiple of 4, please adjust it as much as possible
  by padding with 0.

  Align the source address to a 4-Byte boundary.
-----------------------------------------------------------------*/
static void BIOS_LZ77UnCompVram (void)
{

	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int byteCount = 0;
	int byteShift = 0;
	u32 writeValue = 0;

	int len = header >> 8;

	while(len > 0) {
		u8 d = CPUReadByte(source);
		source++;

		if(d) {
			for(int i = 0; i < 8; i++) {
				if(d & 0x80) {
					u16 data = CPUReadByte(source) << 8;
					source++;
					data |= CPUReadByte(source);
					source++;
					int length = (data >> 12) + 3;
					int offset = (data & 0x0FFF);
					u32 windowOffset = dest + byteCount - offset - 1;
					CPUReadMem8Setup(windowOffset);
					CPUWriteMem16Setup(dest);
					for(int i2 = 0; i2 < length; i2++) {
						writeValue |= (CPUReadMemFast(windowOffset) << byteShift);
						windowOffset++;
						byteShift += 8;
						byteCount++;

						if(byteCount == 2) {
							CPUWriteMemFast(dest, writeValue);
							dest += 2;
							byteCount = 0;
							byteShift = 0;
							writeValue = 0;
						}
						len--;
						if(len == 0)
							return;
					}
				} else {
					writeValue |= (CPUReadByte(source) << byteShift);
					source++;
					byteShift += 8;
					byteCount++;
					if(byteCount == 2) {
						CPUWriteHalfWord(dest, writeValue);
						dest += 2;
						byteCount = 0;
						byteShift = 0;
						writeValue = 0;
					}
					len--;
					if(len == 0)
						return;
				}
				d <<= 1;
			}
		} else {
			CPUReadMem8Setup(source);
			CPUWriteMem16Setup(dest);
			for(int i = 0; i < 8; i++) {
				writeValue |= (CPUReadMemFast(source) << byteShift);
				source++;
				byteShift += 8;
				byteCount++;
				if(byteCount == 2) {
					CPUWriteMemFast(dest, writeValue);
					dest += 2;
					byteShift = 0;
					byteCount = 0;
					writeValue = 0;
				}
				len--;
				if(len == 0)
					return;
			}
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x13) - HuffUnComp
  The decoder starts in root node, the separate bits in the bitstream specify if the next node is node0 or node1,
  if that node is a data node, then the data is stored in memory, and the decoder is reset to the root node.
  The most often used data should be as close to the root node as possible. For example, the 4-byte string "Huff"
  could be compressed to 6 bits: 10-11-0-0, with root.0 pointing directly to data "f", and root.1 pointing to a
  child node, whose nodes point to data "H" and data "u".

  Data is written in units of 32bits, if the size of the compressed data is not a multiple of 4,
  please adjust it as much as possible by padding with 0.
  Align the source address to a 4Byte boundary.
-----------------------------------------------------------------*/
static void BIOS_HuffUnComp (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
	((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	u8 treeSize = CPUReadByte(source);
	source++;

	u32 treeStart = source;

	source += ((treeSize+1)<<1)-1; // minus because we already skipped one byte

	int len = header >> 8;

	u32 mask = 0x80000000;
	u32 data = CPUReadMemory(source);
	source += 4;

	int pos = 0;
	u8 rootNode = CPUReadByte(treeStart);
	u8 currentNode = rootNode;
	bool writeData = false;
	int byteShift = 0;
	int byteCount = 0;
	u32 writeValue = 0;

	if((header & 0x0F) == 8) {
		while(len > 0) {
			// take left
			if(pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3F)+1)<<1);

			if(data & mask) {
				// right
				if(currentNode & 0x40)
					writeData = true;
				currentNode = CPUReadByte(treeStart+pos+1);
			} else {
				// left
				if(currentNode & 0x80)
					writeData = true;
				currentNode = CPUReadByte(treeStart+pos);
			}

			if(writeData) {
				writeValue |= (currentNode << byteShift);
				byteCount++;
				byteShift += 8;

				pos = 0;
				currentNode = rootNode;
				writeData = false;

				if(byteCount == 4) {
					byteCount = 0;
					byteShift = 0;
					CPUWriteMemory(dest, writeValue);
					writeValue = 0;
					dest += 4;
					len -= 4;
				}
			}
			mask >>= 1;
			if(mask == 0) {
				mask = 0x80000000;
				data = CPUReadMemory(source);
				source += 4;
			}
		}
	} else {
		int halfLen = 0;
		int value = 0;
		while(len > 0) {
			// take left
			if(pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3F)+1)<<1);

			if((data & mask)) {
				// right
				if(currentNode & 0x40)
					writeData = true;
				currentNode = CPUReadByte(treeStart+pos+1);
			} else {
				// left
				if(currentNode & 0x80)
					writeData = true;
				currentNode = CPUReadByte(treeStart+pos);
			}

			if(writeData) {
				if(halfLen == 0)
					value |= currentNode;
				else
					value |= (currentNode<<4);

				halfLen += 4;
				if(halfLen == 8) {
					writeValue |= (value << byteShift);
					byteCount++;
					byteShift += 8;

					halfLen = 0;
					value = 0;

					if(byteCount == 4) {
						byteCount = 0;
						byteShift = 0;
						CPUWriteMemory(dest, writeValue);
						dest += 4;
						writeValue = 0;
						len -= 4;
					}
				}
				pos = 0;
				currentNode = rootNode;
				writeData = false;
			}
			mask >>= 1;
			if(mask == 0) {
				mask = 0x80000000;
				data = CPUReadMemory(source);
				source += 4;
			}
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x14) - RLUnCompWram
  Expands run-length compressed data. The Wram function is faster, and writes in units of 8bits.
  For the Vram function the destination must be halfword aligned, data is written in units of 16bits.
  If the size of the compressed data is not a multiple of 4, please adjust it as much as possible by padding with 0.
  Align the source address to a 4Byte boundary.
-----------------------------------------------------------------*/
static void BIOS_RLUnCompWram (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source & 0xFFFFFFFC);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int len = header >> 8;

	while(len > 0) {
		u8 d = CPUReadByte(source);
		source++;
		int l = d & 0x7F;
		if(d & 0x80) {
			u8 data = CPUReadByte(source);
			source++;
			l += 3;
			for(int i = 0;i < l; i++) {
				CPUWriteByte(dest, data);
				dest++;
				len--;
				if(len == 0)
					return;
			}
		} else {
			l++;
			for(int i = 0; i < l; i++) {
				CPUWriteByte(dest,  CPUReadByte(source));
				source++;
				dest++;
				len--;
				if(len == 0)
					return;
			}
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x15) - RLUnCompVram
  Expands run-length compressed data. The Wram function is faster, and writes in units of 8bits.
  For the Vram function the destination must be halfword aligned, data is written in units of 16bits.
  If the size of the compressed data is not a multiple of 4, please adjust it as much as possible by padding with 0.
  Align the source address to a 4Byte boundary.
-----------------------------------------------------------------*/
static void BIOS_RLUnCompVram (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source & 0xFFFFFFFC);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int len = header >> 8;
	int byteCount = 0;
	int byteShift = 0;
	u32 writeValue = 0;

	while(len > 0)
	{
		u8 d = CPUReadByte(source);
		source++;
		int l = d & 0x7F;
		if(d & 0x80) {
			u8 data = CPUReadByte(source);
			source++;
			l += 3;
			for(int i = 0;i < l; i++) {
				writeValue |= (data << byteShift);
				byteShift += 8;
				byteCount++;

				if(byteCount == 2) {
					CPUWriteHalfWord(dest, writeValue);
					dest += 2;
					byteCount = 0;
					byteShift = 0;
					writeValue = 0;
				}
				len--;
				if(len == 0)
					return;
			}
		} else {
			l++;
			for(int i = 0; i < l; i++) {
				writeValue |= (CPUReadByte(source) << byteShift);
				source++;
				byteShift += 8;
				byteCount++;
				if(byteCount == 2) {
					CPUWriteHalfWord(dest, writeValue);
					dest += 2;
					byteCount = 0;
					byteShift = 0;
					writeValue = 0;
				}
				len--;
				if(len == 0)
					return;
			}
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x16) - Diff8bitUnFilterWram
  These aren't actually real decompression functions, destination data will have exactly the same size as source data.
  However, assume a bitmap or wave form to contain a stream of increasing numbers such like 10..19, the filtered/unfiltered data would be:

  unfiltered:   10  11  12  13  14  15  16  17  18  19
  filtered:     10  +1  +1  +1  +1  +1  +1  +1  +1  +1

  In this case using filtered data (combined with actual compression algorithms) will obviously produce better compression results.
  Data units may be either 8bit or 16bit used with Diff8bit or Diff16bit functions respectively.
  The 8bitVram function allows to write to VRAM directly (which uses 16bit data bus) by writing two 8bit values at once,
  the downside is that it is eventually slower as the 8bitWram function.
-----------------------------------------------------------------*/
static void BIOS_Diff8bitUnFilterWram (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
	(((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0))
		return;

	int len = header >> 8;

	u8 data = CPUReadByte(source);
	source++;

	CPUWriteByte(dest, data);
	dest++;
	len--;

	CPUReadMem8Setup(source);
	CPUWriteMem8Setup(dest);
	while(len > 0) {
		u8 diff = CPUReadMemFast(source);
		data += diff;
		CPUWriteMemFast(dest, data);
		source++;
		dest++;
		len--;
	}
}


/*-----------------------------------------------------------------
 SWI(0x17) - Diff8bitUnFilterVram
  These aren't actually real decompression functions, destination data will have exactly the same size as source data.
  However, assume a bitmap or wave form to contain a stream of increasing numbers such like 10..19, the filtered/unfiltered data would be:

  unfiltered:   10  11  12  13  14  15  16  17  18  19
  filtered:     10  +1  +1  +1  +1  +1  +1  +1  +1  +1

  In this case using filtered data (combined with actual compression algorithms) will obviously produce better compression results.
  Data units may be either 8bit or 16bit used with Diff8bit or Diff16bit functions respectively.
  The 8bitVram function allows to write to VRAM directly (which uses 16bit data bus) by writing two 8bit values at once,
  the downside is that it is eventually slower as the 8bitWram function.
-----------------------------------------------------------------*/
static void BIOS_Diff8bitUnFilterVram (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int len = header >> 8;

	u8 data = CPUReadByte(source);
	source++;

	u16 writeData = data;
	int shift = 8;
	int bytes = 1;

	CPUReadMem8Setup(source);
	CPUWriteMem16Setup(dest);
	while(len >= 2) {
		u8 diff = CPUReadMemFast(source);
		source++;
		data += diff;
		writeData |= (data << shift);
		bytes++;
		shift += 8;
		if(bytes == 2) {
			CPUWriteMemFast(dest, writeData);
			dest += 2;
			len -= 2;
			bytes = 0;
			writeData = 0;
			shift = 0;
		}
	}
}


/*-----------------------------------------------------------------
 SWI(0x18) - Diff16bitUnFilter
  These aren't actually real decompression functions, destination data will have exactly the same size as source data.
  However, assume a bitmap or wave form to contain a stream of increasing numbers such like 10..19, the filtered/unfiltered data would be:

  unfiltered:   10  11  12  13  14  15  16  17  18  19
  filtered:     10  +1  +1  +1  +1  +1  +1  +1  +1  +1

  In this case using filtered data (combined with actual compression algorithms) will obviously produce better compression results.
  Data units may be either 8bit or 16bit used with Diff8bit or Diff16bit functions respectively.
  The 8bitVram function allows to write to VRAM directly (which uses 16bit data bus) by writing two 8bit values at once,
  the downside is that it is eventually slower as the 8bitWram function.
-----------------------------------------------------------------*/
static void BIOS_Diff16bitUnFilter (void)
{
	u32 source = bus.reg[0].I;
	u32 dest = bus.reg[1].I;

	u32 header = CPUReadMemory(source);
	source += 4;

	if(((source & 0xe000000) == 0) ||
			((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return;

	int len = header >> 8;

	u16 data = CPUReadHalfWord(source);
	source += 2;
	CPUWriteHalfWord(dest, data);
	dest += 2;
	len -= 2;

	CPUReadMem16Setup(source);
	CPUWriteMem16Setup(dest);
	while(len >= 2) {
		u16 diff = CPUReadMemFast(source);
		source += 2;
		data += diff;
		CPUWriteMemFast(dest, data);
		dest += 2;
		len -= 2;
	}
}





static void BIOS_RegisterRamReset(u32 flags)
{
	// no need to trace here. this is only called directly from GBA.cpp
	// to emulate bios initialization

	CPUUpdateRegister(0x0, 0x80);

	if(flags)
	{
		if(flags & 0x01)
			memset(workRAM, 0, WORKRAM_SIZE);		// clear work RAM

		if(flags & 0x02)
			memset(internalRAM, 0, 0x7e00);		// don't clear 0x7e00-0x7fff, clear internal RAM

		if(flags & 0x04)
			memset(paletteRAM, 0, PALETTE_SIZE);	// clear palette RAM

		if(flags & 0x08)
			memset(vram, 0, VRAM_SIZE);		// clear VRAM

		if(flags & 0x10)
			memset(oam, 0, OAM_SIZE);			// clean OAM

		if(flags & 0x80) {
			int i;
			for(i = 0; i < 0x10; i++)
				CPUUpdateRegister(0x200+i*2, 0);

			for(i = 0; i < 0xF; i++)
				CPUUpdateRegister(0x4+i*2, 0);

			for(i = 0; i < 0x20; i++)
				CPUUpdateRegister(0x20+i*2, 0);

			for(i = 0; i < 0x18; i++)
				CPUUpdateRegister(0xb0+i*2, 0);

			CPUUpdateRegister(0x130, 0);
			CPUUpdateRegister(0x20, 0x100);
			CPUUpdateRegister(0x30, 0x100);
			CPUUpdateRegister(0x26, 0x100);
			CPUUpdateRegister(0x36, 0x100);
		}

		if(flags & 0x20) {
			int i;
			for(i = 0; i < 8; i++)
				CPUUpdateRegister(0x110+i*2, 0);
			CPUUpdateRegister(0x134, 0x8000);
			for(i = 0; i < 7; i++)
				CPUUpdateRegister(0x140+i*2, 0);
		}

		if(flags & 0x40) {
			int i;
			CPUWriteByte(0x4000084, 0);
			CPUWriteByte(0x4000084, 0x80);
			CPUWriteMemory(0x4000080, 0x880e0000);
			CPUUpdateRegister(0x88, CPUReadHalfWord(0x4000088)&0x3ff);
			CPUWriteByte(0x4000070, 0x70);
			for(i = 0; i < 8; i++)
				CPUUpdateRegister(0x90+i*2, 0);
			CPUWriteByte(0x4000070, 0);
			for(i = 0; i < 8; i++)
				CPUUpdateRegister(0x90+i*2, 0);
			CPUWriteByte(0x4000084, 0);
		}
	}
}



static void BIOS_SoftReset (void)
{
	armState = true;
	armMode = 0x1F;
	armIrqEnable = false;
	C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
	bus.reg[13].I       = 0x03007F00;
	bus.reg[14].I       = 0x00000000;
	bus.reg[16].I       = 0x00000000;
	bus.reg[R13_IRQ].I  = 0x03007FA0;
	bus.reg[R14_IRQ].I  = 0x00000000;
	bus.reg[SPSR_IRQ].I = 0x00000000;
	bus.reg[R13_SVC].I  = 0x03007FE0;
	bus.reg[R14_SVC].I  = 0x00000000;
	bus.reg[SPSR_SVC].I = 0x00000000;
	u8 b = internalRAM[0x7ffa];

	memset(&internalRAM[0x7e00], 0, 0x200);

	if(b) {
		bus.armNextPC = 0x02000000;
		bus.reg[15].I = 0x02000004;
	} else {
		bus.armNextPC = 0x08000000;
		bus.reg[15].I = 0x08000004;
	}
}


#define BIOS_REGISTER_RAM_RESET() BIOS_RegisterRamReset(bus.reg[0].I);

#define BIOS_MIDI_KEY_2_FREQ() \
{ \
	int freq = CPUReadMemory(bus.reg[0].I+4); \
	double tmp; \
	tmp = ((double)(180 - bus.reg[1].I)) - ((double)bus.reg[2].I / 256.f); \
	tmp = pow((double)2.f, tmp / 12.f); \
	bus.reg[0].I = (int)((double)freq / tmp); \
}

#define BIOS_SND_DRIVER_JMP_TABLE_COPY() \
	for(int i = 0; i < 36; i++) \
	{ \
		CPUWriteMemory(bus.reg[0].I, 0x9c); \
		bus.reg[0].I += 4; \
	}
