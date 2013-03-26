/*
 * GBAcpu.cpp
 *
 *  Created on: Mar 21, 2013
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>
#include <string.h>

#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"

#ifndef __GBAINLINE__

static void dataBusPrefetchUpdate(int addr, int waitState)
{
	if ((addr>=0x08) || (addr < 0x02))
	{
		busPrefetchCount=0;
		busPrefetch=false;
	}
	else if (busPrefetch)
	{
		if (!waitState)
			waitState = 1;
		busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
	}
}

// Waitstates when accessing data
int dataTicksAccess16(u32 address) // DATA 8/16bits NON SEQ
{
	int addr = (address>>24)&15;
	int value =  memoryWait[addr];

	dataBusPrefetchUpdate(addr, value);

	return value;
}

int dataTicksAccess32(u32 address) // DATA 32bits NON SEQ
{
	int addr = (address>>24)&15;
	int value = memoryWait32[addr];

	dataBusPrefetchUpdate(addr, value);

	return value;
}

int dataTicksAccessSeq16(u32 address)// DATA 8/16bits SEQ
{
	int addr = (address>>24)&15;
	int value = memoryWaitSeq[addr];

	dataBusPrefetchUpdate(addr, value);

	return value;
}

int dataTicksAccessSeq32(u32 address)// DATA 32bits SEQ
{
	int addr = (address>>24)&15;
	int value =  memoryWaitSeq32[addr];

	dataBusPrefetchUpdate(addr, value);

	return value;
}


// Waitstates when executing opcode
int codeTicksAccess16(u32 address) // THUMB NON SEQ
{
	int addr = (address>>24)&15;

	if ((addr>=0x08) && (addr<=0x0D))
	{
		if (busPrefetchCount&0x1)
		{
			if (busPrefetchCount&0x2)
			{
				busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr]-1;
		}
		else
		{
			busPrefetchCount=0;
			return memoryWait[addr];
		}
	}
	else
	{
		busPrefetchCount = 0;
		return memoryWait[addr];
	}
}

int codeTicksAccess32(u32 address) // ARM NON SEQ
{
	int addr = (address>>24)&15;

	if ((addr>=0x08) && (addr<=0x0D))
	{
		if (busPrefetchCount&0x1)
		{
			if (busPrefetchCount&0x2)
			{
				busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr] - 1;
		}
		else
		{
			busPrefetchCount = 0;
			return memoryWait32[addr];
		}
	}
	else
	{
		busPrefetchCount = 0;
		return memoryWait32[addr];
	}
}

int codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
	int addr = (address>>24)&15;

	if ((addr>=0x08) && (addr<=0x0D))
	{
		if (busPrefetchCount&0x1)
		{
			busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		else
			if (busPrefetchCount>0xFF)
			{
				busPrefetchCount=0;
				return memoryWait[addr];
			}
			else
				return memoryWaitSeq[addr];
	}
	else
	{
		busPrefetchCount = 0;
		return memoryWaitSeq[addr];
	}
}

int codeTicksAccessSeq32(u32 address) // ARM SEQ
{
	int addr = (address>>24)&15;

	if ((addr>=0x08) && (addr<=0x0D))
	{
		if (busPrefetchCount&0x1)
		{
			if (busPrefetchCount&0x2)
			{
				busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr];
		}
		else
			if (busPrefetchCount>0xFF)
			{
				busPrefetchCount=0;
				return memoryWait32[addr];
			}
			else
				return memoryWaitSeq32[addr];
	}
	else
	{
		return memoryWaitSeq32[addr];
	}
}


// Emulates the Cheat System (m) code
void cpuMasterCodeCheck()
{
	if((mastercode) && (mastercode == armNextPC))
	{
		u32 joy = 0;
		if(systemReadJoypads())
			joy = systemReadJoypad(-1);
		u32 ext = (joy >> 10);
		cpuTotalTicks += cheatsCheckKeys(P1^0x3FF, ext);
	}
}

#endif //__GBAINLINE__
