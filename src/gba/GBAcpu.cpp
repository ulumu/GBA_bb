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

 // DATA 8/16bits NON SEQ
static int _dataTickAccess16(int page);
static int _dataTickAccess16busFetch(int page);

dataTicksAccess_t fdataAccess16[PAGESIZE] = {
		_dataTickAccess16,          // page 0
		_dataTickAccess16,          // page 1
		_dataTickAccess16busFetch,  // page 2
		_dataTickAccess16busFetch,  // page 3
		_dataTickAccess16busFetch,  // page 4
		_dataTickAccess16busFetch,  // page 5
		_dataTickAccess16busFetch,  // page 6
		_dataTickAccess16busFetch,  // page 7
		_dataTickAccess16,          // page 8
		_dataTickAccess16,          // page 9
		_dataTickAccess16,          // page A
		_dataTickAccess16,          // page B
		_dataTickAccess16,          // page C
		_dataTickAccess16,          // page D
		_dataTickAccess16,          // page E
		_dataTickAccess16           // page F
};

// DATA 32bits NON SEQ
static int _dataTickAccess32(int page);
static int _dataTickAccess32busFetch(int page);

dataTicksAccess_t fdataAccess32[PAGESIZE] = {
		_dataTickAccess32,          // page 0
		_dataTickAccess32,          // page 1
		_dataTickAccess32busFetch,  // page 2
		_dataTickAccess32busFetch,  // page 3
		_dataTickAccess32busFetch,  // page 4
		_dataTickAccess32busFetch,  // page 5
		_dataTickAccess32busFetch,  // page 6
		_dataTickAccess32busFetch,  // page 7
		_dataTickAccess32,          // page 8
		_dataTickAccess32,          // page 9
		_dataTickAccess32,          // page A
		_dataTickAccess32,          // page B
		_dataTickAccess32,          // page C
		_dataTickAccess32,          // page D
		_dataTickAccess32,          // page E
		_dataTickAccess32           // page F
};

// THUMB NON SEQ wait state lookup
static int _codeTickAccess16(int page);
static int _codeTickAccess16busFetch(int page);

codeTicksAccess_t fcodeAccess16[PAGESIZE] = {
		_codeTickAccess16,          // page 0
		_codeTickAccess16,          // page 1
		_codeTickAccess16,          // page 2
		_codeTickAccess16,          // page 3
		_codeTickAccess16,          // page 4
		_codeTickAccess16,          // page 5
		_codeTickAccess16,          // page 6
		_codeTickAccess16,          // page 7
		_codeTickAccess16busFetch,  // page 8
		_codeTickAccess16busFetch,  // page 9
		_codeTickAccess16busFetch,  // page A
		_codeTickAccess16busFetch,  // page B
		_codeTickAccess16busFetch,  // page C
		_codeTickAccess16busFetch,  // page D
		_codeTickAccess16,          // page E
		_codeTickAccess16           // page F
};

// ARM NON SEQ wait state lookup
static int _codeTickAccess32(int page);
static int _codeTickAccess32busFetch(int page);

codeTicksAccess_t fcodeAccess32[PAGESIZE] = {
		_codeTickAccess32,          // page 0
		_codeTickAccess32,          // page 1
		_codeTickAccess32,          // page 2
		_codeTickAccess32,          // page 3
		_codeTickAccess32,          // page 4
		_codeTickAccess32,          // page 5
		_codeTickAccess32,          // page 6
		_codeTickAccess32,          // page 7
		_codeTickAccess32busFetch,  // page 8
		_codeTickAccess32busFetch,  // page 9
		_codeTickAccess32busFetch,  // page A
		_codeTickAccess32busFetch,  // page B
		_codeTickAccess32busFetch,  // page C
		_codeTickAccess32busFetch,  // page D
		_codeTickAccess32,          // page E
		_codeTickAccess32           // page F
};


inline void dataBusPrefetchUpdate(int addr, int waitState)
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

static int _dataTickAccess16(int page)
{
	busPrefetchCount=0;
	busPrefetch=false;

	return memoryWait[page];
}

static int _dataTickAccess16busFetch(int page)
{
	int waitState = memoryWait[page];
	if (busPrefetch)
	{
		if (!waitState)
			waitState = 1;
		busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
	}

	return memoryWait[page];
}


static int _dataTickAccess32(int page)
{
	busPrefetchCount=0;
	busPrefetch=false;

	return memoryWait32[page];
}

static int _dataTickAccess32busFetch(int page)
{
	int waitState = memoryWait32[page];
	if (busPrefetch)
	{
		if (!waitState)
			waitState = 1;
		busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
	}

	return memoryWait32[page];
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

// -----------------------------------------------------------------------------------------------
// Waitstates when executing opcode

static int _codeTickAccess16(int page)
{
	busPrefetchCount = 0;
	return memoryWait[page];
}

static int _codeTickAccess16busFetch(int page)
{
	if (busPrefetchCount&0x1)
	{
		if (busPrefetchCount&0x2)
		{
			busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
		return memoryWaitSeq[page]-1;
	}
	else
	{
		busPrefetchCount=0;
		return memoryWait[page];
	}
}


static int _codeTickAccess32(int page)
{
	busPrefetchCount = 0;
	return memoryWait32[page];
}

static int _codeTickAccess32busFetch(int page)
{
	if (busPrefetchCount&0x1)
	{
		if (busPrefetchCount&0x2)
		{
			busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
		return memoryWaitSeq[page] - 1;
	}
	else
	{
		busPrefetchCount = 0;
		return memoryWait32[page];
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
