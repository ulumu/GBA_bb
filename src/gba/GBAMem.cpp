/*
 * GBAMem.cpp
 *
 *  Created on: Mar 21, 2013
 */

#ifndef __GBAINLINE__

#include "../System.h"
#include "../common/Port.h"
#include "GBA.h"
#include "GBAcpu.h"
#include "Globals.h"
#include "GBALink.h"
#include "GBAinline.h"
#include "GBAMem.h"

//#define __MEMPROFILE__

#ifdef __MEMPROFILE__
u32 memProfileCount[16 * 8];
#define MEMRD8_BASE        0
#define MEMRD16_BASE       16
#define MEMRD32_BASE       32
#define MEMWR8_BASE        48
#define MEMWR16_BASE       64
#define MEMWR32_BASE       80
#define MEMPROFILE(type) ++memProfileCount[type]
#else
#define MEMPROFILE(type)
#endif //__MEMPROFILE__

#if 1 //defined(__X86__)
#define ALIGN_SHIFT32                                                           \
	if(address & 3)                                                             \
	{                                                                           \
		int shift = (address & 3) << 3;                                         \
		value     = (value >> shift) | (value << (32 - shift));                 \
	}
#else
#define ALIGN_SHIFT32                                                           \
	if(address & 3)                                                             \
	{                                                                           \
		__asm__ ( "	and r7, %2, #0x3 \n\t"                                      \
				  "	lsl r7, r7, #3 \n\t"                                        \
				  "	mov %0, %1, ROR r7 \n\t"                                    \
					: "=r" (value)	                                            \
					: "r" (value), "r" (address)                                \
					: "r7" );                                                   \
	}
#endif

#ifdef __OPTIMIZE_MEM__
static u32 rdMem32Default()
{
	if(cpuDmaHack) {
		return cpuDmaLast;
	} else {
		if(armState) {
			return CPUReadMemoryQuick(bus.reg[15].I);
		} else {
			return ( CPUReadHalfWordQuick(bus.reg[15].I) |
					(CPUReadHalfWordQuick(bus.reg[15].I) << 16) );
		}
	}
}

static u32 rdMem32_0(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE);
	u32 value;
	if( (bus.reg[15].I & 0x0FFFFFFF) >> 24 )
	{
		if(address < 0x4000) value = READ32LE(((u32 *)&biosProtected));
		else                 value = rdMem32Default();
	}
	else
	{
#ifdef VIRTUAL_MEM
		value = READ32LE((address+GBAVM_ADDR));
#else
		value = READ32LE(((u32 *)&bios[address & 0x3FFC]));
#endif
	}

	ALIGN_SHIFT32;
	return value;
}

static u32 rdMem32_others(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+0xF);
	u32 value = rdMem32Default();

	ALIGN_SHIFT32;

	return value;
}

static u32 rdMem32_2(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+2);

#ifdef VIRTUAL_MEM
	return *(u32 *)(address);
#else
	u32 value = READ32LE(((u32 *)&workRAM[address & 0x3FFFC]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_3(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+3);
#ifdef VIRTUAL_MEM
	return *(u32 *)(address);
#else
	u32 value = READ32LE(((u32 *)&internalRAM[address & 0x7ffC]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_4(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+4);
	u32 value;
	if((address < 0x4000400) && ioReadable[address & 0x3fc])
	{
		if(ioReadable[(address & 0x3fc) + 2])
		{
			value = READ32LE(((u32 *)&ioMem[address & 0x3fC]));
			if ((address & 0x3fc) == COMM_JOY_RECV_L)
				UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
		} else {
			value = READ16LE(((u16 *)&ioMem[address & 0x3fc]));
		}
	}
	else
		value = rdMem32Default();

	ALIGN_SHIFT32;

	return value;
}

static u32 rdMem32_5(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+5);
#ifdef VIRTUAL_MEM
	return READ32LE(address & (PALETTE_ADDR_BASE + 0x3FC));
#else
	u32 value = READ32LE(((u32 *)&paletteRAM[address & 0x3fC]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_6(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+6);
	u32 value;
	u32 addr = (address & 0x1fffc);
	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
	{
		return 0;
	}
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;
#ifdef VIRTUAL_MEM
	return READ32LE(addr + VRAM_ADDR_BASE);
#else
	value = READ32LE(((u32 *)&vram[addr]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_7(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+7);
#ifdef VIRTUAL_MEM
	return READ32LE(address);
#else
	u32 value = READ32LE(((u32 *)&oam[address & 0x3FC]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_8to12(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+8);
#if 0 //def VIRTUAL_MEM
	return READ32LE(( (address&(0x0F1FFFFC)) + GBAVM_ADDR));
#else
	u32 value = READ32LE(((u32 *)&rom[address&0x1FFFFFC]));

	ALIGN_SHIFT32;

	return value;
#endif
}

static u32 rdMem32_13(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+13);
	if(cpuEEPROMEnabled)
		// no need to swap this
		return eepromRead(address);

	u32 value = rdMem32Default();

	ALIGN_SHIFT32;

	return value;
}

static u32 rdMem32_14(const u32 address)
{
	MEMPROFILE(MEMRD32_BASE+14);
	if(cpuFlashEnabled | cpuSramEnabled)
		// no need to swap this
		return flashRead(address);

	u32 value = rdMem32Default();

	ALIGN_SHIFT32;

	return value;
}



readMemFunc_t rdMem32[16 * 16] = {
		rdMem32_0,
		rdMem32_others,
		rdMem32_2,
		rdMem32_3,
		rdMem32_4,
		rdMem32_5,
		rdMem32_6,
		rdMem32_7,
		rdMem32_8to12,
		rdMem32_8to12,
		rdMem32_8to12,
		rdMem32_8to12,
		rdMem32_8to12,
		rdMem32_13,
		rdMem32_14,
		rdMem32_others,

		rdMem32_0,     rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,

		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,
		rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others,rdMem32_others
};

#endif //__OPTIMIZE_MEM__

#ifndef __OPTIMIZE_MEM__
u32 CPUReadMemory(u32 address)
{
#ifdef GBA_LOGGING
	if(address & 3) {
		if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
			log("Unaligned word read: %08x at %08x\n", address, armMode ?
					armNextPC - 4 : armNextPC - 2);
		}
	}
#endif

	u32 value;
	switch(address >> 24) {
	case 0:
		if(bus.reg[15].I >> 24) {
			if(address < 0x4000) {
#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_ILLEGAL_READ) {
					log("Illegal word read: %08x at %08x\n", address, armMode ?
							armNextPC - 4 : armNextPC - 2);
				}
#endif

				value = READ32LE(((u32 *)&biosProtected));
			}
			else goto unreadable;
		} else
			value = READ32LE(((u32 *)&bios[address & 0x3FFC]));
		break;
	case 2:
		value = READ32LE(((u32 *)&workRAM[address & 0x3FFFC]));
		break;
	case 3:
		value = READ32LE(((u32 *)&internalRAM[address & 0x7ffC]));
		break;
	case 4:
		if((address < 0x4000400) && ioReadable[address & 0x3fc]) {
			if(ioReadable[(address & 0x3fc) + 2]) {
				value = READ32LE(((u32 *)&ioMem[address & 0x3fC]));
				if ((address & 0x3fc) == COMM_JOY_RECV_L)
					UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
			} else {
				value = READ16LE(((u16 *)&ioMem[address & 0x3fc]));
			}
		}
		else
			goto unreadable;
		break;
	case 5:
		value = READ32LE(((u32 *)&paletteRAM[address & 0x3fC]));
		break;
	case 6:
		address = (address & 0x1fffc);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
		{
			value = 0;
			break;
		}
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		value = READ32LE(((u32 *)&vram[address]));
		break;
	case 7:
		value = READ32LE(((u32 *)&oam[address & 0x3FC]));
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		value = READ32LE(((u32 *)&rom[address&0x1FFFFFC]));
		break;
	case 13:
		if(cpuEEPROMEnabled)
			// no need to swap this
			return eepromRead(address);
		goto unreadable;
	case 14:
		if(cpuFlashEnabled | cpuSramEnabled)
			// no need to swap this
			return flashRead(address);
		// default
	default:
unreadable:
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_ILLEGAL_READ) {
			log("Illegal word read: %08x at %08x\n", address, armMode ?
					armNextPC - 4 : armNextPC - 2);
		}
#endif

		if(cpuDmaHack) {
			value = cpuDmaLast;
		} else {
			if(armState) {
				value = CPUReadMemoryQuick(bus.reg[15].I);
			} else {
				value = CPUReadHalfWordQuick(bus.reg[15].I) |
						CPUReadHalfWordQuick(bus.reg[15].I) << 16;
			}
		}
	}

	if(address & 3) {
#ifdef C_CORE
		int shift = (address & 3) << 3;
		value = (value >> shift) | (value << (32 - shift));
#else
#ifdef __GNUC__
		asm("and $3, %%ecx;"
				"shl $3 ,%%ecx;"
				"ror %%cl, %0"
				: "=r" (value)
				  : "r" (value), "c" (address));
#else
		__asm {
			mov ecx, address;
			and ecx, 3;
			shl ecx, 3;
			ror [dword ptr value], cl;
		}
#endif
#endif
	}
	return value;
}
#endif //__OPTIMIZE_MEM__

//---------------------------------------- RD HALF WORD ---------------------------------------

#if 1 //defined(__X86__)
#define ALIGN_SHIFT16                                                           \
	if(address & 1) {                                                           \
		value = (value >> 8) | (value << 24);                                   \
	}
#else
#define ALIGN_SHIFT16                                                           \
	if(address & 1)                                                             \
	{                                                                           \
		__asm__  ("	mov %0, %1, ROR #8 \n\t"                                    \
					: "=r" (value)	                                            \
					: "r"  (value)                                              \
					: );                                                        \
	}
#endif

static u32 rdMem16Default(const u32 address)
{
	if(cpuDmaHack) {
		return (cpuDmaLast & 0xFFFF);
	} else {
		if(armState) {
			return CPUReadHalfWordQuick(bus.reg[15].I + (address & 2));
		} else {
			return CPUReadHalfWordQuick(bus.reg[15].I);
		}
	}
}

static u32 rdMem16_0(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE);
	u32 value;

	if (bus.reg[15].I >> 24)
	{
		if(address < 0x4000) value = READ16LE(((u16 *)&biosProtected[address&2]));
		else                 value = rdMem16Default(address);
	}
	else
	{
#ifdef VIRTUAL_MEM
		value = READ16LE((address+GBAVM_ADDR));
#else
		value = READ16LE(((u16 *)&bios[address & 0x3FFE]));
#endif
	}

	ALIGN_SHIFT16;
	return value;
}

static u32 rdMem16_others(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+0xF);
	u32 value = rdMem16Default(address);

	ALIGN_SHIFT16;

	return value;
}

static u32 rdMem16_2(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+2);

#ifdef VIRTUAL_MEM
	return *(u16 *)(address);
#else
	u32 value = READ16LE(((u16 *)&workRAM[address & 0x3FFFE]));

	ALIGN_SHIFT16;

	return value;
#endif
}

static u32 rdMem16_3(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+3);
#ifdef VIRTUAL_MEM
	return *(u16 *)(address);
#else
	u32 value = READ16LE(((u16 *)&internalRAM[address & 0x7ffe]));

	ALIGN_SHIFT16;

	return value;
#endif
}

static u32 rdMem16_4(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+4);
	u32 value;
	if((address < 0x4000400) && ioReadable[address & 0x3fe])
	{
		value =  READ16LE(((u16 *)&ioMem[address & 0x3fe]));
		if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E))
		{
			if (((address & 0x3fe) == 0x100) && timer0On)
				value = 0xFFFF - ((timer0Ticks-cpuTotalTicks) >> timer0ClockReload);
			else
				if (((address & 0x3fe) == 0x104) && timer1On && !(READ_REG(REG_TM1CNT) & 4))
					value = 0xFFFF - ((timer1Ticks-cpuTotalTicks) >> timer1ClockReload);
				else
					if (((address & 0x3fe) == 0x108) && timer2On && !(READ_REG(REG_TM2CNT) & 4))
						value = 0xFFFF - ((timer2Ticks-cpuTotalTicks) >> timer2ClockReload);
					else
						if (((address & 0x3fe) == 0x10C) && timer3On && !(READ_REG(REG_TM3CNT) & 4))
							value = 0xFFFF - ((timer3Ticks-cpuTotalTicks) >> timer3ClockReload);
		}
	}
	else value = rdMem16Default(address);

	ALIGN_SHIFT16;

	return value;
}

static u32 rdMem16_5(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+5);
#ifdef VIRTUAL_MEM
	return READ16LE(address & (PALETTE_ADDR_BASE + 0x3FE));
#else
	u32 value = READ16LE(((u16 *)&paletteRAM[address & 0x3fe]));

	ALIGN_SHIFT16;

	return value;
#endif
}

static u32 rdMem16_6(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+6);
	u32 value;
	u32 addr = (address & 0x1fffe);

	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
	{
		return 0;
	}
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;

#ifdef VIRTUAL_MEM
	return READ16LE(addr + VRAM_ADDR_BASE);
#else
	value = READ16LE(((u16 *)&vram[addr]));

	ALIGN_SHIFT16;

	return value;
#endif
}

static u32 rdMem16_7(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+7);
#ifdef VIRTUAL_MEM
	return READ16LE(address);
#else
	u32 value = READ16LE(((u16 *)&oam[address & 0x3fe]));

	ALIGN_SHIFT16;

	return value;
#endif
}

static u32 rdMem16_8to12(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+8);
	u32 value;

	if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
		value = rtcRead(address);
	else
#if 0 //def VIRTUAL_MEM
		value = READ16LE((address+GBAVM_ADDR));
#else
		value = READ16LE(((u16 *)&rom[address & 0x1FFFFFE]));

	ALIGN_SHIFT16;
#endif

	return value;
}

static u32 rdMem16_13(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+13);
	if(cpuEEPROMEnabled)
		// no need to swap this
		return  eepromRead(address);

	u32 value = rdMem16Default(address);

	ALIGN_SHIFT16;

	return value;
}

static u32 rdMem16_14(const u32 address)
{
	MEMPROFILE(MEMRD16_BASE+14);
	if(cpuFlashEnabled | cpuSramEnabled)
		// no need to swap this
		return flashRead(address);

	u32 value = rdMem16Default(address);

	ALIGN_SHIFT16;

	return value;
}


readMemFunc_t rdMem16[16 * 16] = {
		rdMem16_0,
		rdMem16_others,
		rdMem16_2,
		rdMem16_3,
		rdMem16_4,
		rdMem16_5,
		rdMem16_6,
		rdMem16_7,
		rdMem16_8to12,
		rdMem16_8to12,
		rdMem16_8to12,
		rdMem16_8to12,
		rdMem16_8to12,
		rdMem16_13,
		rdMem16_14,
		rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,

		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,
		rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others,rdMem16_others
};

#ifndef __OPTIMIZE_MEM__
u32 CPUReadHalfWord(u32 address)
{
#ifdef GBA_LOGGING
	if(address & 1) {
		if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
			log("Unaligned halfword read: %08x at %08x\n", address, armMode ?
					armNextPC - 4 : armNextPC - 2);
		}
	}
#endif

	u32 value;

	switch(address >> 24) {
	case 0:
		if (bus.reg[15].I >> 24) {
			if(address < 0x4000) {
#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_ILLEGAL_READ) {
					log("Illegal halfword read: %08x at %08x\n", address, armMode ?
							armNextPC - 4 : armNextPC - 2);
				}
#endif
				value = READ16LE(((u16 *)&biosProtected[address&2]));
			} else goto unreadable;
		} else
			value = READ16LE(((u16 *)&bios[address & 0x3FFE]));
		break;
	case 2:
		value = READ16LE(((u16 *)&workRAM[address & 0x3FFFE]));
		break;
	case 3:
		value = READ16LE(((u16 *)&internalRAM[address & 0x7ffe]));
		break;
	case 4:
		if((address < 0x4000400) && ioReadable[address & 0x3fe])
		{
			value =  READ16LE(((u16 *)&ioMem[address & 0x3fe]));
			if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E))
			{
				if (((address & 0x3fe) == 0x100) && timer0On)
					value = 0xFFFF - ((timer0Ticks-cpuTotalTicks) >> timer0ClockReload);
				else
					if (((address & 0x3fe) == 0x104) && timer1On && !(io_registers[REG_TM1CNT] & 4))
						value = 0xFFFF - ((timer1Ticks-cpuTotalTicks) >> timer1ClockReload);
					else
						if (((address & 0x3fe) == 0x108) && timer2On && !(io_registers[REG_TM2CNT] & 4))
							value = 0xFFFF - ((timer2Ticks-cpuTotalTicks) >> timer2ClockReload);
						else
							if (((address & 0x3fe) == 0x10C) && timer3On && !(io_registers[REG_TM3CNT] & 4))
								value = 0xFFFF - ((timer3Ticks-cpuTotalTicks) >> timer3ClockReload);
			}
		}
		else goto unreadable;
		break;
	case 5:
		value = READ16LE(((u16 *)&paletteRAM[address & 0x3fe]));
		break;
	case 6:
		address = (address & 0x1fffe);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
		{
			value = 0;
			break;
		}
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		value = READ16LE(((u16 *)&vram[address]));
		break;
	case 7:
		value = READ16LE(((u16 *)&oam[address & 0x3fe]));
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
			value = rtcRead(address);
		else
			value = READ16LE(((u16 *)&rom[address & 0x1FFFFFE]));
		break;
	case 13:
		if(cpuEEPROMEnabled)
			// no need to swap this
			return  eepromRead(address);
		goto unreadable;
	case 14:
		if(cpuFlashEnabled | cpuSramEnabled)
			// no need to swap this
			return flashRead(address);
		// default
	default:
unreadable:
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_ILLEGAL_READ) {
			log("Illegal halfword read: %08x at %08x\n", address, armMode ?
					armNextPC - 4 : armNextPC - 2);
		}
#endif
		if(cpuDmaHack) {
			value = cpuDmaLast & 0xFFFF;
		} else {
			if(armState) {
				value = CPUReadHalfWordQuick(bus.reg[15].I + (address & 2));
			} else {
				value = CPUReadHalfWordQuick(bus.reg[15].I);
			}
		}
		break;
	}

	if(address & 1) {
		value = (value >> 8) | (value << 24);
	}

	return value;
}

u16 CPUReadHalfWordSigned(u32 address)
{
	u16 value = CPUReadHalfWord(address);
	if((address & 1))
		value = (s8)value;
	return value;
}
#endif //__OPTIMIZE_MEM__


//---------------------------------------- Read Byte --------------------------------------

static u8 rdMem8Default(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+0xF);
	if(cpuDmaHack) {
		return (cpuDmaLast & 0xFF);
	} else {
		if(armState) {
			return CPUReadByteQuick(bus.reg[15].I+(address & 3));
		} else {
			return CPUReadByteQuick(bus.reg[15].I+(address & 1));
		}
	}
}

static u8 rdMem8_0(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+0);
	if (bus.reg[15].I >> 24) {
		if(address < 0x4000) return biosProtected[address & 3];
		else                 return rdMem8Default(address);
	}
#ifdef VIRTUAL_MEM
	return *(u8 *)(address + GBAVM_ADDR);
#else
	return bios[address & 0x3FFF];
#endif
}

static u8 rdMem8_2(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+2);

#ifdef VIRTUAL_MEM
	return *(u8 *)(address);
#else
	return workRAM[address & 0x3FFFF];
#endif
}

static u8 rdMem8_3(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+3);
#ifdef VIRTUAL_MEM
	return *(u8 *)(address);
#else
	return internalRAM[address & 0x7fff];
#endif
}

static u8 rdMem8_4(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+4);
	if((address < 0x4000400) && ioReadable[address & 0x3ff])
		return ioMem[address & 0x3ff];
	else
		return rdMem8Default(address);
}

static u8 rdMem8_5(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+5);
#ifdef VIRTUAL_MEM
	return *(u8 *)(address & (PALETTE_ADDR_BASE + 0x3FF));
#else
	return paletteRAM[address & 0x3ff];
#endif
}

static u8 rdMem8_6(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+6);
	u32 addr = (address & 0x1ffff);
	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
		return 0;
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;
#ifdef VIRTUAL_MEM
	return *(u8 *)(addr + VRAM_ADDR_BASE);
#else
	return vram[addr];
#endif
}

static u8 rdMem8_7(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+7);
#ifdef VIRTUAL_MEM
	return *(u8 *)(address);
#else
	return oam[address & 0x3ff];
#endif
}

static u8 rdMem8_8to12(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+8);
#if 0 //def VIRTUAL_MEM
	return *(u8 *)((address & (ROM_ADDR_BASE + 0x1FFFFFF)) + GBAVM_ADDR);
#else
	return rom[address & 0x1FFFFFF];
#endif
}

static u8 rdMem8_13(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+13);
	if(cpuEEPROMEnabled)
		return eepromRead(address);

	return rdMem8Default(address);
}

static u8 rdMem8_14(const u32 address)
{
	MEMPROFILE(MEMRD8_BASE+14);
	if(cpuSramEnabled | cpuFlashEnabled)
		return flashRead(address);
	if(cpuEEPROMSensorEnabled) {
		switch(address & 0x00008f00) {
		case 0x8200:
			return systemGetSensorX() & 255;
		case 0x8300:
			return (systemGetSensorX() >> 8)|0x80;
		case 0x8400:
			return systemGetSensorY() & 255;
		case 0x8500:
			return systemGetSensorY() >> 8;
		}
	}

	return rdMem8Default(address);
}


readMem8Func_t rdMem8[16 * 16] = {
		rdMem8_0,
		rdMem8Default,
		rdMem8_2,
		rdMem8_3,
		rdMem8_4,
		rdMem8_5,
		rdMem8_6,
		rdMem8_7,
		rdMem8_8to12,
		rdMem8_8to12,
		rdMem8_8to12,
		rdMem8_8to12,
		rdMem8_8to12,
		rdMem8_13,
		rdMem8_14,
		rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,

		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,
		rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default,rdMem8Default
};


#ifndef __OPTIMIZE_MEM__
u8 CPUReadByte(u32 address)
{
	switch(address >> 24) {
	case 0:
		if (bus.reg[15].I >> 24) {
			if(address < 0x4000) {
#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_ILLEGAL_READ) {
					log("Illegal byte read: %08x at %08x\n", address, armMode ?
							armNextPC - 4 : armNextPC - 2);
				}
#endif
				return biosProtected[address & 3];
			} else goto unreadable;
		}
		return bios[address & 0x3FFF];
	case 2:
		return workRAM[address & 0x3FFFF];
	case 3:
		return internalRAM[address & 0x7fff];
	case 4:
		if((address < 0x4000400) && ioReadable[address & 0x3ff])
			return ioMem[address & 0x3ff];
		else goto unreadable;
	case 5:
		return paletteRAM[address & 0x3ff];
	case 6:
		address = (address & 0x1ffff);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		return vram[address];
	case 7:
		return oam[address & 0x3ff];
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		return rom[address & 0x1FFFFFF];
	case 13:
		if(cpuEEPROMEnabled)
			return eepromRead(address);
		goto unreadable;
	case 14:
		if(cpuSramEnabled | cpuFlashEnabled)
			return flashRead(address);
		if(cpuEEPROMSensorEnabled) {
			switch(address & 0x00008f00) {
			case 0x8200:
				return systemGetSensorX() & 255;
			case 0x8300:
				return (systemGetSensorX() >> 8)|0x80;
			case 0x8400:
				return systemGetSensorY() & 255;
			case 0x8500:
				return systemGetSensorY() >> 8;
			}
		}
		// default
		default:
unreadable:
#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_ILLEGAL_READ) {
				log("Illegal byte read: %08x at %08x\n", address, armMode ?
						armNextPC - 4 : armNextPC - 2);
			}
#endif
			if(cpuDmaHack) {
				return cpuDmaLast & 0xFF;
			} else {
				if(armState) {
					return CPUReadByteQuick(bus.reg[15].I+(address & 3));
				} else {
					return CPUReadByteQuick(bus.reg[15].I+(address & 1));
				}
			}
			break;
	}
}
#endif //__OPTIMIZE_MEM__


//----------------------------------- WR Mem 32 --------------------------------

static void wrMem32_default(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+0xF);
#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
				log("Illegal word write: %08x to %08x from %08x\n",
						value,
						address,
						armMode ? armNextPC - 4 : armNextPC - 2);
			}
#endif
}

static void wrMem32_2(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+2);
#ifdef VIRTUAL_MEM
	WRITE32LE(address, value);
#else
	WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
#endif
}

static void wrMem32_3(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+3);
#ifdef VIRTUAL_MEM
	WRITE32LE(address, value);
#else
	WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
#endif
}

static void wrMem32_4(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+4);
	if(address < 0x4000400) {
		CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
		CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
	}
	else wrMem32_default(address, value);
}

static void wrMem32_5(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+5);
#ifdef VIRTUAL_MEM
	WRITE32LE(address & (PALETTE_ADDR_BASE + 0x3FC), value);
#else
	WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
#endif
}

static void wrMem32_6(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+6);
	u32 addr = (address & 0x1fffc);
	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
		return;
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;

#ifdef VIRTUAL_MEM
	WRITE32LE(addr+VRAM_ADDR_BASE, value);
#else
	WRITE32LE(((u32 *)&vram[addr]), value);
#endif
}

static void wrMem32_7(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+7);
#ifdef VIRTUAL_MEM
	WRITE32LE(address, value);
#else
	WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
#endif
}

static void wrMem32_D(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+0xD);
	if(cpuEEPROMEnabled) {
		eepromWrite(address, value);
	}
	else wrMem32_default(address, value);
}

static void wrMem32_E(const u32 address, const u32 value)
{
	MEMPROFILE(MEMWR32_BASE+0xE);
	if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
		(*cpuSaveGameFunc)(address, (u8)value);
	}
	else wrMem32_default(address, value);
}

writeMem32Func_t wrMem32[16 * 16] = {
		wrMem32_default,wrMem32_default,wrMem32_2,      wrMem32_3,      wrMem32_4,      wrMem32_5,      wrMem32_6,      wrMem32_7,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_D,      wrMem32_E,      wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,

		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,
		wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default,wrMem32_default
};

#ifndef __OPTIMIZE_MEM__
void CPUWriteMemory(u32 address, u32 value)
{

#ifdef GBA_LOGGING
	if(address & 3) {
		if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
			log("Unaligned word write: %08x to %08x from %08x\n",
					value,
					address,
					armMode ? armNextPC - 4 : armNextPC - 2);
		}
	}
#endif

	switch(address >> 24) {
	case 0x02:
#ifdef BKPT_SUPPORT
		if(*((u32 *)&freezeWorkRAM[address & 0x3FFFC]))
			cheatsWriteMemory(address & 0x203FFFC,
					value);
		else
#endif
			WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
		break;
	case 0x03:
#ifdef BKPT_SUPPORT
		if(*((u32 *)&freezeInternalRAM[address & 0x7ffc]))
			cheatsWriteMemory(address & 0x3007FFC,
					value);
		else
#endif
			WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
		break;
	case 0x04:
		if(address < 0x4000400) {
			CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
			CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
		} else goto unwritable;
		break;
	case 0x05:
#ifdef BKPT_SUPPORT
		if(*((u32 *)&freezePRAM[address & 0x3fc]))
			cheatsWriteMemory(address & 0x70003FC,
					value);
		else
#endif
			WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
		break;
	case 0x06:
		address = (address & 0x1fffc);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
			return;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;

#ifdef BKPT_SUPPORT
		if(*((u32 *)&freezeVRAM[address]))
			cheatsWriteMemory(address + 0x06000000, value);
		else
#endif

			WRITE32LE(((u32 *)&vram[address]), value);
		break;
	case 0x07:
#ifdef BKPT_SUPPORT
		if(*((u32 *)&freezeOAM[address & 0x3fc]))
			cheatsWriteMemory(address & 0x70003FC,
					value);
		else
#endif
			WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
		break;
	case 0x0D:
		if(cpuEEPROMEnabled) {
			eepromWrite(address, value);
			break;
		}
		goto unwritable;
	case 0x0E:
		if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
			(*cpuSaveGameFunc)(address, (u8)value);
			break;
		}
		// default
		default:
unwritable:
#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
				log("Illegal word write: %08x to %08x from %08x\n",
						value,
						address,
						armMode ? armNextPC - 4 : armNextPC - 2);
			}
#endif
			break;
	}
}
#endif


//----------------------------------- WR Mem 16 --------------------------------

static void wrMem16_default(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+0xF);
#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
				log("Illegal word write: %08x to %08x from %08x\n",
						value,
						address,
						armMode ? armNextPC - 4 : armNextPC - 2);
			}
#endif
}

static void wrMem16_2(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+2);
#ifdef VIRTUAL_MEM
	WRITE16LE(address, value);
#else
	WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
#endif
}

//extern void CPUInterruptEvent();
static void wrMem16_3(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+3);
#ifdef VIRTUAL_MEM
	WRITE16LE(address, value);
//	if (address == 0x03FFFFF8)
//	{
//		SLOG("REG_IFBIOS <= %4X", value);
//		CPUInterruptEvent();
//	}
//	else if (address == 0x03FFFFFC)
//	{
//		SLOG("REG_BIOS_ISR <= %4X", value);
//	}
#else
	WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
#endif
}

static void wrMem16_4(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+4);
	if(address < 0x4000400)
		CPUUpdateRegister(address & 0x3fe, value);
	else wrMem16_default(address, value);
}

static void wrMem16_5(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+5);
#ifdef VIRTUAL_MEM
	WRITE16LE(address & (PALETTE_ADDR_BASE + 0x3FE), value);
#else
	WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
#endif
}

static void wrMem16_6(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+6);
	u32 addr = (address & 0x1fffe);
	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
		return;
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;

#ifdef VIRTUAL_MEM
	WRITE16LE(addr+VRAM_ADDR_BASE, value);
#else
	WRITE16LE(((u16 *)&vram[addr]), value);
#endif
}

static void wrMem16_7(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+7);
#ifdef VIRTUAL_MEM
	WRITE16LE(address, value);
#else
	WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
#endif
}

static void wrMem16_8_9(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+8);
	if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
		if(!rtcWrite(address, value))
			wrMem16_default(address, value);
	} else if(!agbPrintWrite(address, value)) wrMem16_default(address, value);
}

static void wrMem16_D(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+0xD);
	if(cpuEEPROMEnabled) {
		eepromWrite(address, (u8)value);
	}
	else wrMem16_default(address, value);
}

static void wrMem16_E(const u32 address, const u16 value)
{
	MEMPROFILE(MEMWR16_BASE+0xE);
	if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
		(*cpuSaveGameFunc)(address, (u8)value);
	}
	else wrMem16_default(address, value);
}

writeMem16Func_t wrMem16[16 * 16] = {
		wrMem16_default,
		wrMem16_default,
		wrMem16_2,
		wrMem16_3,
		wrMem16_4,
		wrMem16_5,
		wrMem16_6,
		wrMem16_7,
		wrMem16_8_9,
		wrMem16_8_9,
		wrMem16_default,
		wrMem16_default,
		wrMem16_default,
		wrMem16_D,
		wrMem16_E,
		wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,

		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,
		wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default,wrMem16_default
};



#ifndef __OPTIMIZE_MEM__
void CPUWriteHalfWord(u32 address, u16 value)
{
#ifdef GBA_LOGGING
	if(address & 1) {
		if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
			log("Unaligned halfword write: %04x to %08x from %08x\n",
					value,
					address,
					armMode ? armNextPC - 4 : armNextPC - 2);
		}
	}
#endif

	switch(address >> 24) {
	case 2:
#ifdef BKPT_SUPPORT
		if(*((u16 *)&freezeWorkRAM[address & 0x3FFFE]))
			cheatsWriteHalfWord(address & 0x203FFFE,
					value);
		else
#endif
			WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
		break;
	case 3:
#ifdef BKPT_SUPPORT
		if(*((u16 *)&freezeInternalRAM[address & 0x7ffe]))
			cheatsWriteHalfWord(address & 0x3007ffe,
					value);
		else
#endif
			WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
		break;
	case 4:
		if(address < 0x4000400)
			CPUUpdateRegister(address & 0x3fe, value);
		else goto unwritable;
		break;
	case 5:
#ifdef BKPT_SUPPORT
		if(*((u16 *)&freezePRAM[address & 0x03fe]))
			cheatsWriteHalfWord(address & 0x70003fe,
					value);
		else
#endif
			WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
		break;
	case 6:
		address = (address & 0x1fffe);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
			return;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
#ifdef BKPT_SUPPORT
		if(*((u16 *)&freezeVRAM[address]))
			cheatsWriteHalfWord(address + 0x06000000,
					value);
		else
#endif
			WRITE16LE(((u16 *)&vram[address]), value);
		break;
	case 7:
#ifdef BKPT_SUPPORT
		if(*((u16 *)&freezeOAM[address & 0x03fe]))
			cheatsWriteHalfWord(address & 0x70003fe,
					value);
		else
#endif
			WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
		break;
	case 8:
	case 9:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
			if(!rtcWrite(address, value))
				goto unwritable;
		} else if(!agbPrintWrite(address, value)) goto unwritable;
		break;
	case 13:
		if(cpuEEPROMEnabled) {
			eepromWrite(address, (u8)value);
			break;
		}
		goto unwritable;
	case 14:
		if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
			(*cpuSaveGameFunc)(address, (u8)value);
			break;
		}
		goto unwritable;
	default:
unwritable:
#ifdef GBA_LOGGING
if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
	log("Illegal halfword write: %04x to %08x from %08x\n",
			value,
			address,
			armMode ? armNextPC - 4 : armNextPC - 2);
}
#endif
		break;
	}
}
#endif //#ifndef __OPTIMIZE_MEM__


//----------------------------------- WR Mem 8 --------------------------------

static void wrMem8_default(const u32 address, const u8 value)
{
	MEMPROFILE(MEMWR8_BASE+0xF);
#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
				log("Illegal word write: %08x to %08x from %08x\n",
						value,
						address,
						armMode ? armNextPC - 4 : armNextPC - 2);
			}
#endif
}

static void wrMem8_2(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+2);
#ifdef VIRTUAL_MEM
	*(u8 *)(address) = b;
#else
	workRAM[address & 0x3FFFF] = b;
#endif
}

static void wrMem8_3(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+3);
#ifdef VIRTUAL_MEM
	*(u8 *)(address) = b;
#else
	internalRAM[address & 0x7fff] = b;
#endif
}

static void wrMem8_4(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+4);
	if(address < 0x4000400) {
		switch(address & 0x3FF) {
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x68:
		case 0x69:
		case 0x6c:
		case 0x6d:
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x78:
		case 0x79:
		case 0x7c:
		case 0x7d:
		case 0x80:
		case 0x81:
		case 0x84:
		case 0x85:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9a:
		case 0x9b:
		case 0x9c:
		case 0x9d:
		case 0x9e:
		case 0x9f:
			soundEvent_u8(address&0xFF, b);
			break;
		case 0x301: // HALTCNT, undocumented
			if(b == 0x80)
				stopState = true;
			holdState = 1;
			holdType = -1;
			cpuNextEvent = cpuTotalTicks;
			break;
		default: // every other register
			u32 lowerBits = address & 0x3fe;
			if(address & 1) {
				CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0x00FF) | ((u16)b << 8));
			} else {
				CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0xFF00) | b);
			}
			break;
		}
	} else wrMem8_default(address, b);
}

static void wrMem8_5(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+5);
	// no need to switch
#ifdef VIRTUAL_MEM
	*((u16 *)(address & (PALETTE_ADDR_BASE + 0x3FE))) = (b << 8) | b;
#else
	*((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
#endif
}

static void wrMem8_6(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+6);
	u32 addr = (address & 0x1fffe);
	if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((addr & 0x1C000) == 0x18000))
		return;
	if ((addr & 0x18000) == 0x18000)
		addr &= 0x17fff;

	// no need to switch
	// byte writes to OBJ VRAM are ignored
#ifdef VIRTUAL_MEM
	if ((address & 0x1FFFE) < objTilesAddress[((READ_REG(REG_DISPCNT)&DISPCNT_MODE)+1)>>2])
	{
		*((u16 *)(addr+VRAM_ADDR_BASE)) = (b << 8) | b;
	}
#else
	if ((addr) < objTilesAddress[((READ_REG(REG_DISPCNT)&DISPCNT_MODE)+1)>>2])
	{
		*((u16 *)&vram[addr]) = (b << 8) | b;

	}
#endif
}

static void wrMem8_7(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+7);
	// no need to switch
	// byte writes to OAM are ignored
	//    *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
}

static void wrMem8_D(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+0xD);
	if(cpuEEPROMEnabled) {
		eepromWrite(address, b);
	}
	else wrMem8_default(address, b);
}

static void wrMem8_E(const u32 address, const u8 b)
{
	MEMPROFILE(MEMWR8_BASE+0xE);
	if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {
		//if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) {
		(*cpuSaveGameFunc)(address, b);
	}
	else wrMem8_default(address, b);
}

writeMem8Func_t wrMem8[16 * 16] = {
		wrMem8_default,
		wrMem8_default,
		wrMem8_2,
		wrMem8_3,
		wrMem8_4,
		wrMem8_5,
		wrMem8_6,
		wrMem8_7,
		wrMem8_default,
		wrMem8_default,
		wrMem8_default,
		wrMem8_default,
		wrMem8_default,
		wrMem8_D,
		wrMem8_E,
		wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,

		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,
		wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default,wrMem8_default
};

#ifndef __OPTIMIZE_MEM__
void CPUWriteByte(u32 address, u8 b)
{
	switch(address >> 24) {
	case 2:
#ifdef BKPT_SUPPORT
		if(freezeWorkRAM[address & 0x3FFFF])
			cheatsWriteByte(address & 0x203FFFF, b);
		else
#endif
			workRAM[address & 0x3FFFF] = b;
		break;
	case 3:
#ifdef BKPT_SUPPORT
		if(freezeInternalRAM[address & 0x7fff])
			cheatsWriteByte(address & 0x3007fff, b);
		else
#endif
			internalRAM[address & 0x7fff] = b;
		break;
	case 4:
		if(address < 0x4000400) {
			switch(address & 0x3FF) {
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x68:
			case 0x69:
			case 0x6c:
			case 0x6d:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x78:
			case 0x79:
			case 0x7c:
			case 0x7d:
			case 0x80:
			case 0x81:
			case 0x84:
			case 0x85:
			case 0x90:
			case 0x91:
			case 0x92:
			case 0x93:
			case 0x94:
			case 0x95:
			case 0x96:
			case 0x97:
			case 0x98:
			case 0x99:
			case 0x9a:
			case 0x9b:
			case 0x9c:
			case 0x9d:
			case 0x9e:
			case 0x9f:
				soundEvent(address&0xFF, b);
				break;
			case 0x301: // HALTCNT, undocumented
				if(b == 0x80)
					stopState = true;
				holdState = 1;
				holdType = -1;
				cpuNextEvent = cpuTotalTicks;
				break;
			default: // every other register
				u32 lowerBits = address & 0x3fe;
				if(address & 1) {
					CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0x00FF) | (b << 8));
				} else {
					CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0xFF00) | b);
				}
			}
			break;
		} else goto unwritable;
		break;
	case 5:
		// no need to switch
		*((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
		break;
	case 6:
		address = (address & 0x1fffe);
		if (((READ_REG(REG_DISPCNT) & DISPCNT_MODE) >2) && ((address & 0x1C000) == 0x18000))
			return;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;

		// no need to switch
		// byte writes to OBJ VRAM are ignored
		if ((address) < objTilesAddress[((READ_REG(REG_DISPCNT)&DISPCNT_MODE)+1)>>2])
		{
#ifdef BKPT_SUPPORT
			if(freezeVRAM[address])
				cheatsWriteByte(address + 0x06000000, b);
			else
#endif
				*((u16 *)&vram[address]) = (b << 8) | b;
		}
		break;
	case 7:
		// no need to switch
		// byte writes to OAM are ignored
		//    *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
		break;
	case 13:
		if(cpuEEPROMEnabled) {
			eepromWrite(address, b);
			break;
		}
		goto unwritable;
	case 14:
		if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {

			//if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) {

			(*cpuSaveGameFunc)(address, b);
			break;
		}
		// default
	default:
unwritable:
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_ILLEGAL_WRITE) {
			log("Illegal byte write: %02x to %08x from %08x\n",
					b,
					address,
					armMode ? armNextPC - 4 : armNextPC -2 );
		}
#endif
		break;
	}
}
#endif //__OPTIMIZE_MEM__

#endif // __GBAINLINE__
