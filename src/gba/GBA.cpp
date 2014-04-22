#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>
#include <string.h>
#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include "Globals.h"
#include "EEprom.h"
#include "Flash.h"
#include "Sound.h"
#include "Sram.h"
#include "Cheats.h"
#include "../NLS.h"
#include "elf.h"
#include "../Util.h"
#include "../common/Port.h"
#include "../System.h"
#include "agbprint.h"
#include "GBALink.h"
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>

#include "armdis.h"

extern void process_sound_tick_fn(void);

#ifdef PROFILING
#include "prof/prof.h"
#endif

#ifdef __GNUC__
#define _stricmp strcasecmp
#endif

#define  TRACE_LOG_ENTRY(x, y)
#define  TRACE_LOG_EXIT(x, y)

extern int  emulating;
static bool bRenderLineAllEnabled = false;

static int coeff[32] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

static u32  line[6][240];
static bool gfxInWin[2][240];
static int  lineOBJpixleft[128];

int gfxBG2Changed = 0;
int gfxBG3Changed = 0;

int gfxBG2X = 0;
int gfxBG2Y = 0;
int gfxBG3X = 0;
int gfxBG3Y = 0;
int gfxLastVCOUNT = 0;

static u16 io_regshadow[1024 * 16];
static int layerenableShadow;

#define IO_REG           io_registers
#define LAYER_ENABLE_REG layerEnable

#include "types.h"
#include "GBAGfx.h"
#include "gba_gfx.inl"
#include "gba_modes.inl"
#include "gba_mode0.inl"
#include "gba_mode1.inl"
#include "gba_mode2.inl"
#include "gba_mode3.inl"
#include "gba_mode4.inl"
#include "gba_mode5.inl"

#undef IO_REG
#undef LAYER_ENABLE_REG

// --------------------- Shared Memory declaration ---------------------
struct _shmNode_t
{
	u8    *memPtr;
	int    memSize;
	int    fd;
	const char  *name;
	struct _shmNode_t *next;
};
typedef struct _shmNode_t shmNode_t;

static shmNode_t *rom_node         = NULL;
static shmNode_t *internalRAM_node = NULL;
static shmNode_t *workRAM_node     = NULL;

static bool bEmulateNow = false;
#if (DIS_ENABLE)
static char disMsg[256];
#endif

#define EMULATE_ON  //bEmulateNow = true;
#define EMULATE_OFF //bEmulateNow = false;

static shmNode_t *gba_createSharedMem(const char *memName, void *memAddr, int memSize)
{
	int        fd;
	u8        *memPtr;
	shmNode_t *shmNode;

	shmNode = (shmNode_t *)calloc(1, sizeof(shmNode_t));
	if (NULL == shmNode) {
		return NULL;
	}

    /* Create a new memory object */
	fd = shm_open( memName, O_RDWR | O_CREAT, 0777 );
    if( fd == -1 ) {
        return NULL;
    }

    /* Set the memory object's size */
    if( ftruncate( fd, memSize ) == -1 ) {
        return NULL;
    }

	memPtr = (u8 *)mmap(memAddr, memSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#ifdef __X86__
	if (NULL == memPtr) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate Shared memory for %s"), memName);
		CPUCleanUp();
		return NULL;
	}
#else
	if(memPtr != memAddr) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate Shared memory for %s"), memName);
		CPUCleanUp();
		return NULL;
	}
#endif

	shmNode->fd      = fd;
	shmNode->memPtr  = memPtr;
	shmNode->memSize = memSize;
	shmNode->name    = memName;
	shmNode->next    = NULL;

	return shmNode;
}


static void gba_addShardMem(shmNode_t *curNode, const char *memName, void *memAddr, int memSize)
{
	int fdint;
	u32 mirror_addr;
	u8 *tmpPtr;

	fdint = shm_open( memName, O_RDWR, 0777 );
	if( fdint == -1 ) {
		signal( SIGSEGV, SIG_DFL );
		return;
	}

	mirror_addr  = (int)memAddr & (~(memSize-1));

	tmpPtr = (u8 *)mmap64((void *)(mirror_addr), memSize, PROT_READ|PROT_WRITE, MAP_SHARED, fdint, 0);
	if ((u32)tmpPtr != mirror_addr)
	{
		munmap(tmpPtr, memSize);
		SLOG("Failed dynamic mapping mirror virtual address 0x%X for WORKRAM", (int)memAddr);
		signal( SIGSEGV, SIG_DFL );
	}
	else
	{
		shmNode_t *newNode;
		shmNode_t *shmNode = curNode;

		while( shmNode->next ) {
			shmNode = shmNode->next;
		}

		newNode = (shmNode_t *)calloc(1, sizeof(shmNode_t));
		if (newNode)
		{
			newNode->fd      = shmNode->fd;
			newNode->memPtr  = tmpPtr;
			newNode->memSize = shmNode->memSize;
			newNode->name    = shmNode->name;
			shmNode->next    = newNode;
		}
	}

}

static void shmNodeCleanup(shmNode_t *node)
{
	shmNode_t *preNode;

	if (NULL == node) return;

	close(node->fd);
	shm_unlink(node->name);

	while (node)
	{
		munmap(node->memPtr, node->memSize);
		preNode = node;
		node    = node->next;
		free(preNode);
	}
}


// --------------------- Segmentation Fault GBA handler ----------------------------
struct sigaction g_sigact;

void sigseg_gbaHandler(int sig_no, siginfo_t *sigInfo, void *others)
{
	u8 *tmpPtr;
	int fdint;

	SLOG("Invalid Mem Access:0x%X @PC:0x%X", (int)sigInfo->si_addr, (int)sigInfo->si_fltip);

	// Create memory mapped for the invalid location, provided that it is not NULL access
	if (sigInfo->si_addr)
	{
		// create 4k bytes mapping per invalid mem access.
		// TO-DO: find out the offending PC location to identify the invalid memory access
		//        instruction, so we can map the exact size
		u32 base_addr = (int)sigInfo->si_addr & 0x1F000000;
		u32 mirror_addr;

		switch(base_addr)
		{
		case (BIOS_ADDR_BASE):
			if (bEmulateNow == true)
			{
				SLOG("Bios Access....");
			}
			else
			{
				signal( SIGSEGV, SIG_DFL );
			}
			break;

		case (WORKRAM_ADDR_BASE):
#if (GBAVM_ADDR)
		case (WORKRAM_ADDR_BASE+GBAVM_ADDR):
#endif
			gba_addShardMem(workRAM_node, WORKRAM_NAME, sigInfo->si_addr, WORKRAM_SIZE);
			break;

		case (INTRAM_ADDR_BASE):
#if (GBAVM_ADDR)
		case (INTRAM_ADDR_BASE+GBAVM_ADDR):
#endif
			gba_addShardMem(internalRAM_node, INTRAM_NAME, sigInfo->si_addr, INTRAM_SIZE);
			break;

		case ROM_ADDR_BASE:
			gba_addShardMem(rom_node, ROM_NAME, sigInfo->si_addr, ROM_SIZE);
			break;

		default:
			signal( SIGSEGV, SIG_DFL );
			break;

		}
	}
}


/*============================================================
	GBA INLINE
============================================================ */
#ifndef VIRTUAL_MEM
#define ARM_PREFETCH_NEXT		cpuPrefetch[1] = CPUReadMemoryQuick(bus.armNextPC+4);
#define THUMB_PREFETCH_NEXT		cpuPrefetch[1] = CPUReadHalfWordQuick(bus.armNextPC+2);

#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadMemoryQuick(bus.armNextPC);\
    cpuPrefetch[1] = CPUReadMemoryQuick(bus.armNextPC+4);\
  }

#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadHalfWordQuick(bus.armNextPC);\
    cpuPrefetch[1] = CPUReadHalfWordQuick(bus.armNextPC+2);\
  }

#else

#define ARM_PREFETCH_NEXT		cpuPrefetch[1] = READ32LE(bus.armNextPC+4+GBAVM_ADDR);
#define THUMB_PREFETCH_NEXT		cpuPrefetch[1] = READ16LE(bus.armNextPC+2+GBAVM_ADDR);

#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = READ32LE(bus.armNextPC+GBAVM_ADDR);\
    cpuPrefetch[1] = READ32LE(bus.armNextPC+GBAVM_ADDR+4);\
  }

#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = READ16LE(bus.armNextPC+GBAVM_ADDR);\
    cpuPrefetch[1] = READ16LE(bus.armNextPC+GBAVM_ADDR+2);\
  }

#endif

static int clockTicks;
static u8 memoryWait[16]      = { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4,  4,  4, 4, 0 };
static u8 memoryWaitSeq[16]   = { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4,  8,  8, 4, 0 };
static u8 memoryWait32[16]    = { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };
static u8 memoryWaitSeq32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };
static u8 *memoryWaitType[2]  = {memoryWait, memoryWait32};

#ifndef OLD_DATATICKS_UPDATE
void dataticks_update_on_busPrefetch_enable(void);
void dataticks_update_on_busPrefetch_disable(void);

static void dataticks_prefetch_update_bypass(int value) {}
static void dataticks_prefetch_update_outofbound(int value)
{
	bus.busPrefetchCount = 0;
	bus.busPrefetch      = false;
	dataticks_update_on_busPrefetch_disable();
}
static void dataticks_prefetch_update_inbound(int waitState)
{
	waitState = (1 & ~waitState) | (waitState & waitState);
	bus.busPrefetchCount = ((bus.busPrefetchCount+1)<<waitState) - 1;
}
typedef void (*dataticksFunc_t)(const int value);

static dataticksFunc_t dtFunc[16] = {
		dataticks_prefetch_update_outofbound,  // 0
		dataticks_prefetch_update_outofbound,  // 1
		dataticks_prefetch_update_bypass,      // 2
		dataticks_prefetch_update_bypass,      // 3
		dataticks_prefetch_update_bypass,      // 4
		dataticks_prefetch_update_bypass,      // 5
		dataticks_prefetch_update_bypass,      // 6
		dataticks_prefetch_update_bypass,      // 7
		dataticks_prefetch_update_outofbound,  // 8
		dataticks_prefetch_update_outofbound,  // 9
		dataticks_prefetch_update_outofbound,  // A
		dataticks_prefetch_update_outofbound,  // B
		dataticks_prefetch_update_outofbound,  // C
		dataticks_prefetch_update_outofbound,  // D
		dataticks_prefetch_update_outofbound,  // E
		dataticks_prefetch_update_outofbound   // F
};

void dataticks_update_on_busPrefetch_enable(void)
{
	dtFunc[2] = dtFunc[3] = dtFunc[4] = dtFunc[5] = dtFunc[6] = dtFunc[7] = dataticks_prefetch_update_inbound;
}

void dataticks_update_on_busPrefetch_disable(void)
{
	dtFunc[2] = dtFunc[3] = dtFunc[4] = dtFunc[5] = dtFunc[6] = dtFunc[7] = dataticks_prefetch_update_bypass;
}

#define DATATICKS_UPDATE_ON_BUSPREFETCH                \
	    if (bus.busPrefetchCount == 0)                 \
	    {                                              \
	    	bus.busPrefetch = bus.busPrefetchEnable;   \
	    	if (bus.busPrefetch == true)               \
	    		dtFunc[2] = dtFunc[3] = dtFunc[4] = dtFunc[5] = dtFunc[6] = dtFunc[7] = dataticks_prefetch_update_inbound; \
	    	else                                       \
	    		dtFunc[2] = dtFunc[3] = dtFunc[4] = dtFunc[5] = dtFunc[6] = dtFunc[7] = dataticks_prefetch_update_bypass;  \
	    }

#define DATATICKS_ACCESS_BUS_PREFETCH_ARM(addr, value) dtFunc[addr](value)
#define DATATICKS_ACCESS_BUS_PREFETCH(address, value) \
	dtFunc[(address >> 24) & 15](value);

#else
#define DATATICKS_UPDATE_ON_BUSPREFETCH                \
	    if (bus.busPrefetchCount == 0)                 \
	    {                                              \
	    	bus.busPrefetch = bus.busPrefetchEnable;   \
	    }

#define DATATICKS_ACCESS_BUS_PREFETCH_ARM(addr, value) \
	if ((addr>=0x08) || (addr < 0x02)) \
	{ \
		bus.busPrefetchCount=0; \
		bus.busPrefetch=false; \
	} \
	else if (bus.busPrefetch) \
	{ \
		int waitState = value; \
		waitState = (1 & ~waitState) | (waitState & waitState); \
		bus.busPrefetchCount = ((bus.busPrefetchCount+1)<<waitState) - 1; \
	}

#define DATATICKS_ACCESS_BUS_PREFETCH(address, value) \
	int addr = (address >> 24) & 15; \
	if ((addr>=0x08) || (addr < 0x02)) \
	{ \
		bus.busPrefetchCount=0; \
		bus.busPrefetch=false; \
	} \
	else if (bus.busPrefetch) \
	{ \
		int waitState = value; \
		waitState = (1 & ~waitState) | (waitState & waitState); \
		bus.busPrefetchCount = ((bus.busPrefetchCount+1)<<waitState) - 1; \
	}
#endif

/* Waitstates when accessing data */

#define DATATICKS_ACCESS_32BIT(address)      (memoryWait32[(address >> 24) & 15])
#define DATATICKS_ACCESS_32BIT_SEQ(address)  (memoryWaitSeq32[(address >> 24) & 15])
#define DATATICKS_ACCESS_16BIT(address)      (memoryWait[(address >> 24) & 15])
#define DATATICKS_ACCESS_16BIT_PAGE(addrpage)      (memoryWait[addrpage])
#define DATATICKS_ACCESS_16BIT_SEQ(address)  (memoryWaitSeq[(address >> 24) & 15])

#ifndef OLD_CODETICKS_UPDATE
static int codeticks_prefetch_update_outofbound(int addr, int bit32)
{
	bus.busPrefetchCount = 0;

	return memoryWaitType[bit32][addr];
}
static int codeticks_prefetch_update_inbound(int addr, int bit32)
{
	if (bus.busPrefetchCount & 0x1)
	{
		if (bus.busPrefetchCount & 0x2)
		{
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
		return memoryWaitSeq[addr]-1;
	}
	bus.busPrefetchCount = 0;
	return memoryWaitType[bit32][addr];
}

typedef int (*codeticksFunc_t)(const int addr, const int bit32);

static codeticksFunc_t ctFunc[16] = {
		codeticks_prefetch_update_outofbound,  // 0
		codeticks_prefetch_update_outofbound,  // 1
		codeticks_prefetch_update_outofbound,  // 2
		codeticks_prefetch_update_outofbound,  // 3
		codeticks_prefetch_update_outofbound,  // 4
		codeticks_prefetch_update_outofbound,  // 5
		codeticks_prefetch_update_outofbound,  // 6
		codeticks_prefetch_update_outofbound,  // 7
		codeticks_prefetch_update_inbound,     // 8
		codeticks_prefetch_update_inbound,     // 9
		codeticks_prefetch_update_inbound,     // A
		codeticks_prefetch_update_inbound,     // B
		codeticks_prefetch_update_inbound,     // C
		codeticks_prefetch_update_inbound,     // D
		codeticks_prefetch_update_outofbound,  // E
		codeticks_prefetch_update_outofbound   // F
};

#define codeTicksAccess(address, bit32)        \
	ctFunc[(address>>24) & 15]((address>>24) & 15, bit32)

#else

// Waitstates when executing opcode
static INLINE int codeTicksAccess(u32 address, u8 bit32) // THUMB NON SEQ
{
	int addr = (address>>24) & 15;
	//   int ret;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			if (bus.busPrefetchCount&0x2)
			{
				bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr]-1;
		}
	}
	bus.busPrefetchCount = 0;

#if 0
	if(bit32)		/* ARM NON SEQ */
		ret = memoryWait32[addr];
	else			/* THUMB NON SEQ */
		ret = memoryWait[addr];

	return ret;
#else
	return memoryWaitType[bit32][addr];
#endif
}

#endif

#define OLD_CODETICKSEQ_UPDATE
#ifndef OLD_CODETICKSEQ_UPDATE
static int codeticksSeq16_prefetch_update_outofbound(int addr)
{
	bus.busPrefetchCount = 0;

	return memoryWaitSeq[addr];
}

static int codeticksSeq16_prefetch_update_inbound(int addr)
{
	if (bus.busPrefetchCount&0x1)
	{
		bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
		return 0;
	}
	else if (bus.busPrefetchCount>0xFF)
	{
		bus.busPrefetchCount=0;
		return memoryWait[addr];
	}
	return memoryWaitSeq[addr];
}

typedef int (*codeticksSeqFunc_t)(const int addr);

static codeticksSeqFunc_t ctseq16Func[16] = {
		codeticksSeq16_prefetch_update_outofbound,  // 0
		codeticksSeq16_prefetch_update_outofbound,  // 1
		codeticksSeq16_prefetch_update_outofbound,  // 2
		codeticksSeq16_prefetch_update_outofbound,  // 3
		codeticksSeq16_prefetch_update_outofbound,  // 4
		codeticksSeq16_prefetch_update_outofbound,  // 5
		codeticksSeq16_prefetch_update_outofbound,  // 6
		codeticksSeq16_prefetch_update_outofbound,  // 7
		codeticksSeq16_prefetch_update_inbound,     // 8
		codeticksSeq16_prefetch_update_inbound,     // 9
		codeticksSeq16_prefetch_update_inbound,     // A
		codeticksSeq16_prefetch_update_inbound,     // B
		codeticksSeq16_prefetch_update_inbound,     // C
		codeticksSeq16_prefetch_update_inbound,     // D
		codeticksSeq16_prefetch_update_outofbound,  // E
		codeticksSeq16_prefetch_update_outofbound   // F
};

#define codeTicksAccessSeq16(address)        \
		ctseq16Func[(address>>24) & 15]((address>>24) & 15)

static int codeticksSeq32_prefetch_update_outofbound(int addr)
{
	return memoryWaitSeq32[addr];
}

static int codeticksSeq32_prefetch_update_inbound(int addr)
{
	if (bus.busPrefetchCount&0x1)
	{
		if (bus.busPrefetchCount&0x2)
		{
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
		return memoryWaitSeq[addr];
	}
	else if (bus.busPrefetchCount > 0xFF)
	{
		bus.busPrefetchCount=0;
		return memoryWait32[addr];
	}
	return memoryWaitSeq[addr];
}

static codeticksSeqFunc_t ctseq32Func[16] = {
		codeticksSeq32_prefetch_update_outofbound,  // 0
		codeticksSeq32_prefetch_update_outofbound,  // 1
		codeticksSeq32_prefetch_update_outofbound,  // 2
		codeticksSeq32_prefetch_update_outofbound,  // 3
		codeticksSeq32_prefetch_update_outofbound,  // 4
		codeticksSeq32_prefetch_update_outofbound,  // 5
		codeticksSeq32_prefetch_update_outofbound,  // 6
		codeticksSeq32_prefetch_update_outofbound,  // 7
		codeticksSeq32_prefetch_update_inbound,     // 8
		codeticksSeq32_prefetch_update_inbound,     // 9
		codeticksSeq32_prefetch_update_inbound,     // A
		codeticksSeq32_prefetch_update_inbound,     // B
		codeticksSeq32_prefetch_update_inbound,     // C
		codeticksSeq32_prefetch_update_inbound,     // D
		codeticksSeq32_prefetch_update_outofbound,  // E
		codeticksSeq32_prefetch_update_outofbound   // F
};

#define codeTicksAccessSeq32(address)        \
		ctseq32Func[(address>>24) & 15]((address>>24) & 15)

#else

static INLINE int codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
	int addr = (address>>24) & 15;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		else if (bus.busPrefetchCount>0xFF)
		{
			bus.busPrefetchCount=0;
			return memoryWait[addr];
		}
	}
	else
		bus.busPrefetchCount = 0;

	return memoryWaitSeq[addr];
}

static INLINE int codeTicksAccessSeq32(u32 address) // ARM SEQ
{
	int addr = (address>>24) & 15;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			if (bus.busPrefetchCount&0x2)
			{
				bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr];
		}
		else if (bus.busPrefetchCount > 0xFF)
		{
			bus.busPrefetchCount=0;
			return memoryWait32[addr];
		}
	}

	return memoryWaitSeq32[addr];
}
#endif

#ifdef USE_SWITICKS
int SWITicks = 0;
#endif
int IRQTicks = 0;

u32 mastercode = 0;
static u32 cheatJoyData = 0;
int layerEnableDelay = 0;
int cpuDmaTicksToUpdate = 0;
int cpuDmaCount = 0;
bool cpuDmaHack = false;
u32 cpuDmaLast = 0;
int dummyAddress = 0;

bool cpuBreakLoop = false;
int cpuNextEvent = 0;

int gbaSaveType = 0; // used to remember the save type on reset
bool intState = false;
bool stopState = false;
bool holdState = false;
int holdType = 0;
bool cpuSramEnabled = true;
bool cpuFlashEnabled = true;
bool cpuEEPROMEnabled = true;
bool cpuEEPROMSensorEnabled = false;

u32 cpuPrefetch[2];

int cpuTotalTicks = 0;
#ifdef PROFILING
int profilingTicks = 0;
int profilingTicksReload = 0;
static profile_segment *profilSegment = NULL;
#endif

#ifdef BKPT_SUPPORT
u8 freezeWorkRAM[0x40000];
u8 freezeInternalRAM[0x8000];
u8 freezeVRAM[0x18000];
u8 freezePRAM[0x400];
u8 freezeOAM[0x400];
bool debugger_last;
#endif

int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
u8 timerOnOffDelay = 0;
u16 timer0Value = 0;
bool timer0On = false;
int timer0Ticks = 0;
int timer0Reload = 0;
int timer0ClockReload  = 0;
u16 timer1Value = 0;
bool timer1On = false;
int timer1Ticks = 0;
int timer1Reload = 0;
int timer1ClockReload  = 0;
u16 timer2Value = 0;
bool timer2On = false;
int timer2Ticks = 0;
int timer2Reload = 0;
int timer2ClockReload  = 0;
u16 timer3Value = 0;
bool timer3On = false;
int timer3Ticks = 0;
int timer3Reload = 0;
int timer3ClockReload  = 0;
u32 dma0Source = 0;
u32 dma0Dest = 0;
u32 dma1Source = 0;
u32 dma1Dest = 0;
u32 dma2Source = 0;
u32 dma2Dest = 0;
u32 dma3Source = 0;
u32 dma3Dest = 0;
void (*cpuSaveGameFunc)(u32,u8) = flashSaveDecide;

#ifdef GBA_USE_RGBA8888
void (*renderLine)(u32 *) = mode0RenderLine;
#else
void (*renderLine)(u16 *) = mode0RenderLine;
#endif

bool fxOn = false;
bool windowOn = false;
int gFrameCount = 0;
char buffer[1024];
u32 lastTime = 0;
int count = 0;

int capture = 0;
int capturePrevious = 0;
int captureNumber = 0;

const int TIMER_TICKS[4] = {
	0,
	6,
	8,
	10
};

const u32  objTilesAddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakRamWaitState[4] = { 4, 3, 2, 8 };
const u8 gamepakWaitState[4] =  { 4, 3, 2, 8 };
const u8 gamepakWaitState0[2] = { 2, 1 };
const u8 gamepakWaitState1[2] = { 4, 1 };
const u8 gamepakWaitState2[2] = { 8, 1 };
const bool isInRom [16]=
{ false, false, false, false, false, false, false, false,
  true, true, true, true, true, true, false, false };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const u8 videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};


u8 biosProtected[4];

#ifdef WORDS_BIGENDIAN
bool cpuBiosSwapped = false;
#endif

u32 myROM[] = {
	0xEA000006,
	0xEA000093,
	0xEA000006,
	0x00000000,
	0x00000000,
	0x00000000,
	0xEA000088,
	0x00000000,
	0xE3A00302,
	0xE1A0F000,
	0xE92D5800,
	0xE55EC002,
	0xE28FB03C,
	0xE79BC10C,
	0xE14FB000,
	0xE92D0800,
	0xE20BB080,
	0xE38BB01F,
	0xE129F00B,
	0xE92D4004,
	0xE1A0E00F,
	0xE12FFF1C,
	0xE8BD4004,
	0xE3A0C0D3,
	0xE129F00C,
	0xE8BD0800,
	0xE169F00B,
	0xE8BD5800,
	0xE1B0F00E,
	0x0000009C,
	0x0000009C,
	0x0000009C,
	0x0000009C,
	0x000001F8,
	0x000001F0,
	0x000000AC,
	0x000000A0,
	0x000000FC,
	0x00000168,
	0xE12FFF1E,
	0xE1A03000,
	0xE1A00001,
	0xE1A01003,
	0xE2113102,
	0x42611000,
	0xE033C040,
	0x22600000,
	0xE1B02001,
	0xE15200A0,
	0x91A02082,
	0x3AFFFFFC,
	0xE1500002,
	0xE0A33003,
	0x20400002,
	0xE1320001,
	0x11A020A2,
	0x1AFFFFF9,
	0xE1A01000,
	0xE1A00003,
	0xE1B0C08C,
	0x22600000,
	0x42611000,
	0xE12FFF1E,
	0xE92D0010,
	0xE1A0C000,
	0xE3A01001,
	0xE1500001,
	0x81A000A0,
	0x81A01081,
	0x8AFFFFFB,
	0xE1A0000C,
	0xE1A04001,
	0xE3A03000,
	0xE1A02001,
	0xE15200A0,
	0x91A02082,
	0x3AFFFFFC,
	0xE1500002,
	0xE0A33003,
	0x20400002,
	0xE1320001,
	0x11A020A2,
	0x1AFFFFF9,
	0xE0811003,
	0xE1B010A1,
	0xE1510004,
	0x3AFFFFEE,
	0xE1A00004,
	0xE8BD0010,
	0xE12FFF1E,
	0xE0010090,
	0xE1A01741,
	0xE2611000,
	0xE3A030A9,
	0xE0030391,
	0xE1A03743,
	0xE2833E39,
	0xE0030391,
	0xE1A03743,
	0xE2833C09,
	0xE283301C,
	0xE0030391,
	0xE1A03743,
	0xE2833C0F,
	0xE28330B6,
	0xE0030391,
	0xE1A03743,
	0xE2833C16,
	0xE28330AA,
	0xE0030391,
	0xE1A03743,
	0xE2833A02,
	0xE2833081,
	0xE0030391,
	0xE1A03743,
	0xE2833C36,
	0xE2833051,
	0xE0030391,
	0xE1A03743,
	0xE2833CA2,
	0xE28330F9,
	0xE0000093,
	0xE1A00840,
	0xE12FFF1E,
	0xE3A00001,
	0xE3A01001,
	0xE92D4010,
	0xE3A03000,
	0xE3A04001,
	0xE3500000,
	0x1B000004,
	0xE5CC3301,
	0xEB000002,
	0x0AFFFFFC,
	0xE8BD4010,
	0xE12FFF1E,
	0xE3A0C301,
	0xE5CC3208,
	0xE15C20B8,
	0xE0110002,
	0x10222000,
	0x114C20B8,
	0xE5CC4208,
	0xE12FFF1E,
	0xE92D500F,
	0xE3A00301,
	0xE1A0E00F,
	0xE510F004,
	0xE8BD500F,
	0xE25EF004,
	0xE59FD044,
	0xE92D5000,
	0xE14FC000,
	0xE10FE000,
	0xE92D5000,
	0xE3A0C302,
	0xE5DCE09C,
	0xE35E00A5,
	0x1A000004,
	0x05DCE0B4,
	0x021EE080,
	0xE28FE004,
	0x159FF018,
	0x059FF018,
	0xE59FD018,
	0xE8BD5000,
	0xE169F00C,
	0xE8BD5000,
	0xE25EF004,
	0x03007FF0,
	0x09FE2000,
	0x09FFC000,
	0x03007FE0
};

variable_desc saveGameStruct[] = {
	{ &io_registers[REG_DISPCNT]  , sizeof(u16) },
	{ &io_registers[REG_DISPSTAT] , sizeof(u16) },
	{ &io_registers[REG_VCOUNT]   , sizeof(u16) },
	{ &io_registers[REG_BG0CNT]   , sizeof(u16) },
	{ &io_registers[REG_BG1CNT]   , sizeof(u16) },
	{ &io_registers[REG_BG2CNT]   , sizeof(u16) },
	{ &io_registers[REG_BG3CNT]   , sizeof(u16) },
	{ &io_registers[REG_BG0HOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG0VOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG1HOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG1VOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG2HOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG2VOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG3HOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG3VOFS]  , sizeof(u16) },
	{ &io_registers[REG_BG2PA]    , sizeof(u16) },
	{ &io_registers[REG_BG2PB]    , sizeof(u16) },
	{ &io_registers[REG_BG2PC]    , sizeof(u16) },
	{ &io_registers[REG_BG2PD]    , sizeof(u16) },
	{ &io_registers[REG_BG2X_L]   , sizeof(u16) },
	{ &io_registers[REG_BG2X_H]   , sizeof(u16) },
	{ &io_registers[REG_BG2Y_L]   , sizeof(u16) },
	{ &io_registers[REG_BG2Y_H]   , sizeof(u16) },
	{ &io_registers[REG_BG3PA]    , sizeof(u16) },
	{ &io_registers[REG_BG3PB]    , sizeof(u16) },
	{ &io_registers[REG_BG3PC]    , sizeof(u16) },
	{ &io_registers[REG_BG3PD]    , sizeof(u16) },
	{ &io_registers[REG_BG3X_L]   , sizeof(u16) },
	{ &io_registers[REG_BG3X_H]   , sizeof(u16) },
	{ &io_registers[REG_BG3Y_L]   , sizeof(u16) },
	{ &io_registers[REG_BG3Y_H]   , sizeof(u16) },
	{ &io_registers[REG_WIN0H]    , sizeof(u16) },
	{ &io_registers[REG_WIN1H]    , sizeof(u16) },
	{ &io_registers[REG_WIN0V]    , sizeof(u16) },
	{ &io_registers[REG_WIN1V]    , sizeof(u16) },
	{ &io_registers[REG_WININ]    , sizeof(u16) },
	{ &io_registers[REG_WINOUT]   , sizeof(u16) },
	{ &io_registers[REG_MOSAIC]   , sizeof(u16) },
	{ &io_registers[REG_BLDMOD]   , sizeof(u16) },
	{ &io_registers[REG_COLEV]    , sizeof(u16) },
	{ &io_registers[REG_COLY]     , sizeof(u16) },
	{ &io_registers[REG_DM0SAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM0SAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM0DAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM0DAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM0CNT_L] , sizeof(u16) },
	{ &io_registers[REG_DM0CNT_H] , sizeof(u16) },
	{ &io_registers[REG_DM1SAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM1SAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM1DAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM1DAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM1CNT_L] , sizeof(u16) },
	{ &io_registers[REG_DM1CNT_H] , sizeof(u16) },
	{ &io_registers[REG_DM2SAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM2SAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM2DAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM2DAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM2CNT_L] , sizeof(u16) },
	{ &io_registers[REG_DM2CNT_H] , sizeof(u16) },
	{ &io_registers[REG_DM3SAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM3SAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM3DAD_L] , sizeof(u16) },
	{ &io_registers[REG_DM3DAD_H] , sizeof(u16) },
	{ &io_registers[REG_DM3CNT_L] , sizeof(u16) },
	{ &io_registers[REG_DM3CNT_H] , sizeof(u16) },
	{ &io_registers[REG_TM0D]     , sizeof(u16) },
	{ &io_registers[REG_TM0CNT]   , sizeof(u16) },
	{ &io_registers[REG_TM1D]     , sizeof(u16) },
	{ &io_registers[REG_TM1CNT]   , sizeof(u16) },
	{ &io_registers[REG_TM2D]     , sizeof(u16) },
	{ &io_registers[REG_TM2CNT]   , sizeof(u16) },
	{ &io_registers[REG_TM3D]     , sizeof(u16) },
	{ &io_registers[REG_TM3CNT]   , sizeof(u16) },
	{ &io_registers[REG_P1]       , sizeof(u16) },
	{ &io_registers[REG_IE]       , sizeof(u16) },
	{ &io_registers[REG_IF]       , sizeof(u16) },
	{ &io_registers[REG_IME]      , sizeof(u16) },
	{ &holdState, sizeof(bool) },
	{ &holdType, sizeof(int) },
	{ &lcdTicks, sizeof(int) },
	{ &timer0On , sizeof(bool) },
	{ &timer0Ticks , sizeof(int) },
	{ &timer0Reload , sizeof(int) },
	{ &timer0ClockReload  , sizeof(int) },
	{ &timer1On , sizeof(bool) },
	{ &timer1Ticks , sizeof(int) },
	{ &timer1Reload , sizeof(int) },
	{ &timer1ClockReload  , sizeof(int) },
	{ &timer2On , sizeof(bool) },
	{ &timer2Ticks , sizeof(int) },
	{ &timer2Reload , sizeof(int) },
	{ &timer2ClockReload  , sizeof(int) },
	{ &timer3On , sizeof(bool) },
	{ &timer3Ticks , sizeof(int) },
	{ &timer3Reload , sizeof(int) },
	{ &timer3ClockReload  , sizeof(int) },
	{ &dma0Source , sizeof(u32) },
	{ &dma0Dest , sizeof(u32) },
	{ &dma1Source , sizeof(u32) },
	{ &dma1Dest , sizeof(u32) },
	{ &dma2Source , sizeof(u32) },
	{ &dma2Dest , sizeof(u32) },
	{ &dma3Source , sizeof(u32) },
	{ &dma3Dest , sizeof(u32) },
	{ &fxOn, sizeof(bool) },
	{ &windowOn, sizeof(bool) },
	{ &N_FLAG , sizeof(bool) },
	{ &C_FLAG , sizeof(bool) },
	{ &Z_FLAG , sizeof(bool) },
	{ &V_FLAG , sizeof(bool) },
	{ &armState , sizeof(bool) },
	{ &armIrqEnable , sizeof(bool) },
	{ &bus.armNextPC , sizeof(u32) },
	{ &armMode , sizeof(int) },
	{ &saveType , sizeof(int) },
	{ NULL, 0 }
};

#include "gba_bios.inl"
#include "../sdl/bbDialog.h"

// ------------------------------------------- Thread rendering model ------------------------------------
#ifdef _GBA_THREAD_RENDERING_
static pthread_t lineRender_tid = 0;
static int       gba_chid = -1;
static int       gba_coid = -1;
static pthread_mutex_t  gbaIntrMutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   gbaIntrEvent   = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  renderMutex    = PTHREAD_MUTEX_INITIALIZER;

#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>     // #define for ND_LOCAL_NODE is in here
#include <errno.h>

#define MAX_STRING_LEN    256
#define GBA_MSG_TYPE      _IO_MAX + 1

// pulse definitions
#define   GBA_PULSE_RENDERLINE  ( _PULSE_CODE_MINAVAIL + 5)
#define   GBA_PULSE_DRAWSCREEN  ( _PULSE_CODE_MINAVAIL + 6)
#define   GBA_PULSE_SNDEVENT    ( _PULSE_CODE_MINAVAIL + 7)
#define   GBA_PULSE_INTRWAIT    ( _PULSE_CODE_MINAVAIL + 8)


//layout of msg's should always defined by a struct, and ID'd by a msg type
// number as the first member
typedef struct {
	uint16_t  msg_type;
	char      string_to_cksum[MAX_STRING_LEN + 1];  //ptr to string we're sending to server, to make it
} gba_msg_t;					//easy, server assumes a max of 256 chars!!!!

typedef struct {
	uint16_t 	msg_type;
	unsigned 	buffer_size;
} gba_header_iov_t;

typedef union {
	struct _pulse    pulse;
	// other message types you will receive
	gba_msg_t        msg;
	gba_header_iov_t iovmsg;
} gbaMessage_t;

/*-----------------------------------------------------------------
  Used by the IntrWait functions
-----------------------------------------------------------------*/
static bool CheckInterrupts(u32 waitFlags)
{
	SLOG("Disable IME");
	CPUWriteByte(0x04000000+REG_IME, 0);  //Disable interrupts

	u16 intFlags = *(u16*)(0x04000000-8); //Get current flags
	u16 flags = intFlags & waitFlags;
	if(flags)
	{
		intFlags = (flags) ^ intFlags;
		*(u16*)(0x04000000-8) = intFlags;
		SLOG("Clear REG_IFBIOS");
	}
	CPUWriteByte(0x04000000+REG_IME, 1);  //Enable interrupts
	SLOG("Enbable IME");

	return flags;
}

/*-----------------------------------------------------------------
 SWI(0x04) - IntrWait
  Continues to wait in Halt state until one (or more) of the specified interrupt(s) do occur.
  The function forcefully sets IME=1. When using multiple interrupts at the same time,
  this function is having less overhead than repeatedly calling the Halt function
-----------------------------------------------------------------*/
static void BIOS_IntrWait(bool discard, u32 waitFlags)
{
	if(discard)
	{
		CheckInterrupts(waitFlags);
	}

	u32 val = 0;
	do
	{
		// wait for Interrupt
		pthread_mutex_lock(&gbaIntrMutex);
		pthread_cond_wait(&gbaIntrEvent, &gbaIntrMutex);
		pthread_mutex_unlock(&gbaIntrMutex);

		SLOG("Intr Wait release");

		val = CheckInterrupts(waitFlags);
	}
	while(!val);
}


/*-----------------------------------------------------------------------*/


static void *lineRender_thread(void *option)
{
	int          rcvid;
	gbaMessage_t gbaMsg;
	int          status;


	// create a channel
	gba_chid = ChannelCreate(0);
	if(-1 == gba_chid) {                //was there an error creating the channel?
		perror("ChannelCreate()");  //look up the errno code and print
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		// receive msg from client, store the receive id in rcvid
		rcvid = MsgReceive(gba_chid, &gbaMsg, sizeof(gbaMsg), NULL);

		if(rcvid == -1) {            //was there an error receiving msg?
			perror("MsgReceive");     //look up errno code and print
			break;                    //try receiving another msg
		}
		else if (rcvid == 0)
		{
			// it’s a pulse, look in msg.pulse… for code
			switch (gbaMsg.pulse.code)
			{
				case GBA_PULSE_RENDERLINE:
				{
					pthread_mutex_lock(&renderMutex);

#ifdef GBA_USE_RGBA8888
					(*renderLine)((u32 *)gbaMsg.pulse.value.sival_int);
#else
					(*renderLine)((u16 *)gbaMsg.pulse.value.sival_int);
#endif
					pthread_mutex_unlock(&renderMutex);

					break;
				}

				case GBA_PULSE_DRAWSCREEN:

					systemDrawScreen();

					break;

				case GBA_PULSE_SNDEVENT:
					process_sound_tick_fn();
					break;

				case GBA_PULSE_INTRWAIT:
					if (holdState)
						BIOS_IntrWait(true, 0x1);
					break;

				default:
					printf("Server:Pulse Unrecognized Pulse Code\n");
					break;
				}; // switch (gbaMsg.pulse.code)
		}
		else
		{
			// handle the messages
			if(gbaMsg.msg.msg_type == GBA_MSG_TYPE)
			{
				int retVal = 1;

				// reply to client with checksum, store the return status in status
				status = MsgReply(rcvid, EOK, &retVal, sizeof(retVal) );
				if(-1 == status) {
					perror("MsgReply");
				}
			}
			else
			{
				printf("Error: Not recognized msg type from client %d\n", gbaMsg.msg.msg_type);
				MsgError(rcvid, ENOSYS);
			}
		}
	}

	return NULL;
}

static void gbaMsgInit(void)
{
	while( gba_chid == -1) usleep(1000);

	gba_coid = ConnectAttach(ND_LOCAL_NODE, getpid(), gba_chid, _NTO_SIDE_CHANNEL, 0);

	if(-1 == gba_coid) {   //was there an error attaching to server?
		perror("ConnectAttach"); //look up error code and print
		exit(EXIT_FAILURE);
	}
}

void CPUInterruptEvent(void)
{
	pthread_mutex_lock(&gbaIntrMutex);
	pthread_cond_signal(&gbaIntrEvent);
	pthread_mutex_unlock(&gbaIntrMutex);
}
#endif // _GBA_THREAD_RENDERING_

//------------------------------------------------------------------------------------------------------


static int romSize = ROM_SIZE;

#ifdef PROFILING
void cpuProfil(profile_segment *seg)
{
profilSegment = seg;
}

void cpuEnableProfiling(int hz)
{
if(hz == 0)
	hz = 100;
profilingTicks = profilingTicksReload = 16777216 / hz;
profSetHertz(hz);
}
#endif

#ifdef PROFILING
static u32 cheatProfile = 0;
#endif

// Emulates the Cheat System (m) code
void cpuMasterCodeCheck()
{
	if((mastercode) && (mastercode == bus.armNextPC))
	{
#ifdef PROFILING
		++cheatProfile;
#endif
		cpuTotalTicks += cheatsCheckKeys(READ_REG(REG_P1)^0x3FF, (cheatJoyData >> 10));
	}
}

inline int CPUUpdateTicks()
{
	int cpuLoopTicks = lcdTicks;

	if(soundTicks < cpuLoopTicks)
		cpuLoopTicks = soundTicks;

	if(timer0On && (timer0Ticks < cpuLoopTicks)) {
		cpuLoopTicks = timer0Ticks;
	}
	if(timer1On && !(READ_REG(REG_TM1CNT) & 4) && (timer1Ticks < cpuLoopTicks)) {
		cpuLoopTicks = timer1Ticks;
	}
	if(timer2On && !(READ_REG(REG_TM2CNT) & 4) && (timer2Ticks < cpuLoopTicks)) {
		cpuLoopTicks = timer2Ticks;
	}
	if(timer3On && !(READ_REG(REG_TM3CNT) & 4) && (timer3Ticks < cpuLoopTicks)) {
		cpuLoopTicks = timer3Ticks;
	}
#ifdef PROFILING
	if(profilingTicksReload != 0) {
		if(profilingTicks < cpuLoopTicks) {
			cpuLoopTicks = profilingTicks;
		}
	}
#endif

#ifdef USE_SWITICKS
	if (SWITicks) {
		if (SWITicks < cpuLoopTicks)
			cpuLoopTicks = SWITicks;
	}
#endif

	if (IRQTicks) {
		if (IRQTicks < cpuLoopTicks)
			cpuLoopTicks = IRQTicks;
	}

	return cpuLoopTicks;
}

void CPUUpdateWindow0()
{
	int x00 = READ_U8REG(REG_WIN0H+1); // io_registers[REG_WIN0H]>>8;
	int x01 = READ_U8REG(REG_WIN0H);   // io_registers[REG_WIN0H] & 255;

	if(x00 <= x01) {
		for(int i = 0; i < 240; i++) {
			gfxInWin[0][i] = (i >= x00 && i < x01);
		}
	} else {
		for(int i = 0; i < 240; i++) {
			gfxInWin[0][i] = (i >= x00 || i < x01);
		}
	}

}

void CPUUpdateWindow1()
{
	int x00 = READ_U8REG(REG_WIN1H+1);  // io_registers[REG_WIN1H]>>8;
	int x01 = READ_U8REG(REG_WIN1H);    // io_registers[REG_WIN1H] & 255;

	if(x00 <= x01) {
		for(int i = 0; i < 240; i++) {
			gfxInWin[1][i] = (i >= x00 && i < x01);
		}
	} else {
		for(int i = 0; i < 240; i++) {
			gfxInWin[1][i] = (i >= x00 || i < x01);
		}
	}

}

#define CLEAR_ARRAY(a) \
	{\
		u32 *array = (a);\
		for(int i = 0; i < 240; i++) {\
			*array++ = 0x80000000;\
		}\
	}\

void CPUUpdateRenderBuffers(bool force)
{
	if(!(layerEnable & 0x0100) || force) {
		CLEAR_ARRAY(line[0]);
	}
	if(!(layerEnable & 0x0200) || force) {
		CLEAR_ARRAY(line[1]);
	}
	if(!(layerEnable & 0x0400) || force) {
		CLEAR_ARRAY(line[2]);
	}
	if(!(layerEnable & 0x0800) || force) {
		CLEAR_ARRAY(line[3]);
	}

}

static bool CPUWriteState(gzFile gzFile)
{
	utilWriteInt(gzFile, SAVE_GAME_VERSION);

	utilGzWrite(gzFile, &rom[0xa0], 16);

	utilWriteInt(gzFile, useBios);

	utilGzWrite(gzFile, &bus.reg[0], sizeof(bus.reg));

	utilWriteData(gzFile, saveGameStruct);

	// new to version 0.7.1
	utilWriteInt(gzFile, stopState);
	// new to version 0.8
	utilWriteInt(gzFile, IRQTicks);

	utilGzWrite(gzFile, internalRAM, INTRAM_SIZE);
	utilGzWrite(gzFile, paletteRAM,  PALETTE_SIZE);
	utilGzWrite(gzFile, workRAM,     WORKRAM_SIZE);
	utilGzWrite(gzFile, vram,        VRAM_SIZE);
	utilGzWrite(gzFile, oam,         OAM_SIZE);
	utilGzWrite(gzFile, pix,         4*241*162);
	utilGzWrite(gzFile, ioMem,       IOMEM_SIZE);

	eepromSaveGame(gzFile);
	flashSaveGame(gzFile);
	soundSaveGame(gzFile);

	cheatsSaveGame(gzFile);

	// version 1.5
	rtcSaveGame(gzFile);

	return true;
}

bool CPUWriteState(const char *file)
{
	gzFile gzFile = utilGzOpen(file, "wb");

	if(gzFile == NULL) {
		bbDialog dialog;

		dialog.showNotification("Error writing to Save State file\nCheck permission issue!!");

		return false;
	}

	bool res = CPUWriteState(gzFile);

	utilGzClose(gzFile);

	return res;
}

bool CPUWriteMemState(char *memory, int available)
{
	gzFile gzFile = utilMemGzOpen(memory, available, "w");

	if(gzFile == NULL) {
		return false;
	}

	bool res = CPUWriteState(gzFile);

	long pos = utilGzMemTell(gzFile)+8;

	if(pos >= (available))
		res = false;

	utilGzClose(gzFile);

	return res;
}

static bool CPUReadState(gzFile gzFile)
{
	int version = utilReadInt(gzFile);

	if(version > SAVE_GAME_VERSION || version < SAVE_GAME_VERSION_1) {
		bbDialog dialog;

		dialog.showNotification("Unsupported GBA save game version");

		return false;
	}

	u8 romname[17];

	utilGzRead(gzFile, romname, 16);

	if(memcmp(&rom[0xa0], romname, 16) != 0) {
		romname[16]=0;
		for(int i = 0; i < 16; i++)
			if(romname[i] < 32)
				romname[i] = 32;

		bbDialog dialog;
		dialog.showNotification("Cannot load save game");
		return false;
	}

	bool ub = utilReadInt(gzFile) ? true : false;

	SLOG("save UB flag=%d, local useBios flag=%d", ub, useBios);

	if(ub != useBios) {
		bbDialog dialog;
		if(useBios)
		{
			dialog.showNotification("Save State expects not using BIOS files\nState restore result is undefined!!");
		}
		else
		{
			dialog.showNotification("Save State requires BIOS file\nState restore result is undefined.\nProceed at your own risk!");
		}
	}

	utilGzRead(gzFile, &bus.reg[0], sizeof(bus.reg));

	utilReadData(gzFile, saveGameStruct);

	if(version < SAVE_GAME_VERSION_3)
		stopState = false;
	else
		stopState = utilReadInt(gzFile) ? true : false;

	if(version < SAVE_GAME_VERSION_4)
	{
		IRQTicks = 0;
		intState = false;
	}
	else
	{
		IRQTicks = utilReadInt(gzFile);
		if (IRQTicks>0)
			intState = true;
		else
		{
			intState = false;
			IRQTicks = 0;
		}
	}

	utilGzRead(gzFile, internalRAM, INTRAM_SIZE);
	utilGzRead(gzFile, paletteRAM,  PALETTE_SIZE);
	utilGzRead(gzFile, workRAM,     WORKRAM_SIZE);
	utilGzRead(gzFile, vram,        VRAM_SIZE);
	utilGzRead(gzFile, oam,         OAM_SIZE);
	if(version < SAVE_GAME_VERSION_6)
		utilGzRead(gzFile, pix, 4*240*160);
	else
		utilGzRead(gzFile, pix, 4*241*162);
	utilGzRead(gzFile, ioMem, IOMEM_SIZE);

	if(skipSaveGameBattery) {
		// skip eeprom data
		eepromReadGameSkip(gzFile, version);
		// skip flash data
		flashReadGameSkip(gzFile, version);
	} else {
		eepromReadGame(gzFile, version);
		flashReadGame(gzFile, version);
	}
	soundReadGame(gzFile, version);

	if(version > SAVE_GAME_VERSION_1) {
		if(skipSaveGameCheats) {
			// skip cheats list data
			cheatsReadGameSkip(gzFile, version);
		} else {
			cheatsReadGame(gzFile, version);
		}
	}
	if(version > SAVE_GAME_VERSION_6) {
		rtcReadGame(gzFile);
	}

	if(version <= SAVE_GAME_VERSION_7) {
		u32 temp;
#define SWAP(a,b,c) \
		temp = (a);\
		(a) = (b)<<16|(c);\
		(b) = (temp) >> 16;\
		(c) = (temp) & 0xFFFF;

		SWAP(dma0Source, READ_REG(REG_DM0SAD_H), READ_REG(REG_DM0SAD_L));
		SWAP(dma0Dest,   READ_REG(REG_DM0DAD_H), READ_REG(REG_DM0DAD_L));
		SWAP(dma1Source, READ_REG(REG_DM1SAD_H), READ_REG(REG_DM1SAD_L));
		SWAP(dma1Dest,   READ_REG(REG_DM1DAD_H), READ_REG(REG_DM1DAD_L));
		SWAP(dma2Source, READ_REG(REG_DM2SAD_H), READ_REG(REG_DM2SAD_L));
		SWAP(dma2Dest,   READ_REG(REG_DM2DAD_H), READ_REG(REG_DM2DAD_L));
		SWAP(dma3Source, READ_REG(REG_DM3SAD_H), READ_REG(REG_DM3SAD_L));
		SWAP(dma3Dest,   READ_REG(REG_DM3DAD_H), READ_REG(REG_DM3DAD_L));
	}

	if(version <= SAVE_GAME_VERSION_8) {
		timer0ClockReload = TIMER_TICKS[READ_REG(REG_TM0CNT) & 3];
		timer1ClockReload = TIMER_TICKS[READ_REG(REG_TM1CNT) & 3];
		timer2ClockReload = TIMER_TICKS[READ_REG(REG_TM2CNT) & 3];
		timer3ClockReload = TIMER_TICKS[READ_REG(REG_TM3CNT) & 3];

		timer0Ticks = ((0x10000 - READ_REG(REG_TM0D)) << timer0ClockReload) - timer0Ticks;
		timer1Ticks = ((0x10000 - READ_REG(REG_TM1D)) << timer1ClockReload) - timer1Ticks;
		timer2Ticks = ((0x10000 - READ_REG(REG_TM2D)) << timer2ClockReload) - timer2Ticks;
		timer3Ticks = ((0x10000 - READ_REG(REG_TM3D)) << timer3ClockReload) - timer3Ticks;
	}

	// set pointers!
	layerEnable = layerSettings & READ_REG(REG_DISPCNT);

	CPUUpdateRender();
	CPUUpdateRenderBuffers(true);
	CPUUpdateWindow0();
	CPUUpdateWindow1();
	gbaSaveType = 0;
	switch(saveType) {
	case 0:
		cpuSaveGameFunc = flashSaveDecide;
		break;
	case 1:
		cpuSaveGameFunc = sramWrite;
		gbaSaveType = 1;
		break;
	case 2:
		cpuSaveGameFunc = flashWrite;
		gbaSaveType = 2;
		break;
	case 3:
		break;
	case 5:
		gbaSaveType = 5;
		break;
	default:
		systemMessage(MSG_UNSUPPORTED_SAVE_TYPE,
				N_("Unsupported save type %d"), saveType);
		break;
	}
	if(eepromInUse)
		gbaSaveType = 3;

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
	if(armState) {
		ARM_PREFETCH;
	} else {
		THUMB_PREFETCH;
	}

	CPUUpdateRegister(0x204, CPUReadHalfWordQuick(0x4000204));

	return true;
}

bool CPUReadMemState(char *memory, int available)
{
gzFile gzFile = utilMemGzOpen(memory, available, "r");

bool res = CPUReadState(gzFile);

utilGzClose(gzFile);

return res;
}

bool CPUReadState(const char * file)
{
	gzFile gzFile = utilGzOpen(file, "rb");

	if(gzFile == NULL)
		return false;

	bool res = CPUReadState(gzFile);

	utilGzClose(gzFile);

	return res;
}

bool CPUExportEepromFile(const char *fileName)
{
if(eepromInUse) {
	FILE *file = fopen(fileName, "wb");

	if(!file) {
		systemMessage(MSG_ERROR_CREATING_FILE, N_("Error creating file %s"),
				fileName);
		return false;
	}

	for(int i = 0; i < eepromSize;) {
		for(int j = 0; j < 8; j++) {
			if(fwrite(&eepromData[i+7-j], 1, 1, file) != 1) {
				fclose(file);
				return false;
			}
		}
		i += 8;
	}
	fclose(file);
}
return true;
}

bool CPUWriteBatteryFile(const char *fileName)
{
if(gbaSaveType == 0) {
	if(eepromInUse)
		gbaSaveType = 3;
	else switch(saveType) {
	case 1:
		gbaSaveType = 1;
		break;
	case 2:
		gbaSaveType = 2;
		break;
	}
}

if((gbaSaveType) && (gbaSaveType!=5)) {
	FILE *file = fopen(fileName, "wb");

	if(!file) {
		systemMessage(MSG_ERROR_CREATING_FILE, N_("Error creating file %s"),
				fileName);
		return false;
	}

	// only save if Flash/Sram in use or EEprom in use
	if(gbaSaveType != 3) {
		if(gbaSaveType == 2) {
			if(fwrite(flashSaveMemory, 1, flashSize, file) != (size_t)flashSize) {
				fclose(file);
				return false;
			}
		} else {
			if(fwrite(flashSaveMemory, 1, 0x10000, file) != 0x10000) {
				fclose(file);
				return false;
			}
		}
	} else {
		if(fwrite(eepromData, 1, eepromSize, file) != (size_t)eepromSize) {
			fclose(file);
			return false;
		}
	}
	fclose(file);
}
return true;
}

bool CPUReadGSASnapshot(const char *fileName)
{
int i;
FILE *file = fopen(fileName, "rb");

if(!file) {
	systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s"), fileName);
	return false;
}

// check file size to know what we should read
fseek(file, 0, SEEK_END);

// long size = ftell(file);
fseek(file, 0x0, SEEK_SET);
fread(&i, 1, 4, file);
fseek(file, i, SEEK_CUR); // Skip SharkPortSave
fseek(file, 4, SEEK_CUR); // skip some sort of flag
fread(&i, 1, 4, file); // name length
fseek(file, i, SEEK_CUR); // skip name
fread(&i, 1, 4, file); // desc length
fseek(file, i, SEEK_CUR); // skip desc
fread(&i, 1, 4, file); // notes length
fseek(file, i, SEEK_CUR); // skip notes
int saveSize;
fread(&saveSize, 1, 4, file); // read length
saveSize -= 0x1c; // remove header size
char buffer[17];
char buffer2[17];
fread(buffer, 1, 16, file);
buffer[16] = 0;
for(i = 0; i < 16; i++)
	if(buffer[i] < 32)
		buffer[i] = 32;
memcpy(buffer2, &rom[0xa0], 16);
buffer2[16] = 0;
for(i = 0; i < 16; i++)
	if(buffer2[i] < 32)
		buffer2[i] = 32;
if(memcmp(buffer, buffer2, 16)) {
	systemMessage(MSG_CANNOT_IMPORT_SNAPSHOT_FOR,
			N_("Cannot import snapshot for %s. Current game is %s"),
			buffer,
			buffer2);
	fclose(file);
	return false;
}
fseek(file, 12, SEEK_CUR); // skip some flags
if(saveSize >= 65536) {
	if(fread(flashSaveMemory, 1, saveSize, file) != (size_t)saveSize) {
		fclose(file);
		return false;
	}
} else {
	systemMessage(MSG_UNSUPPORTED_SNAPSHOT_FILE,
			N_("Unsupported snapshot file %s"),
			fileName);
	fclose(file);
	return false;
}
fclose(file);
CPUReset();
return true;
}

bool CPUReadGSASPSnapshot(const char *fileName)
{
const char gsvfooter[] = "xV4\x12";
const size_t namepos=0x0c, namesz=12;
const size_t footerpos=0x42c, footersz=4;

char footer[footersz+1], romname[namesz+1], savename[namesz+1];;
FILE *file = fopen(fileName, "rb");

if(!file) {
	systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s"), fileName);
	return false;
}

// read save name
fseek(file, namepos, SEEK_SET);
fread(savename, 1, namesz, file);
savename[namesz] = 0;

memcpy(romname, &rom[0xa0], namesz);
romname[namesz] = 0;

if(memcmp(romname, savename, namesz)) {
	systemMessage(MSG_CANNOT_IMPORT_SNAPSHOT_FOR,
			N_("Cannot import snapshot for %s. Current game is %s"),
			savename,
			romname);
	fclose(file);
	return false;
}

// read footer tag
fseek(file, footerpos, SEEK_SET);
fread(footer, 1, footersz, file);
footer[footersz] = 0;

if(memcmp(footer, gsvfooter, footersz)) {
	systemMessage(0,
			N_("Unsupported snapshot file %s. Footer '%s' at %u should be '%s'"),
			fileName,
			footer,
			footerpos,
			gsvfooter);
	fclose(file);
	return false;
}

// Read up to 128k save
fread(flashSaveMemory, 1, FLASH_128K_SZ, file);

fclose(file);
CPUReset();
return true;
}


bool CPUWriteGSASnapshot(const char *fileName,
	const char *title,
	const char *desc,
	const char *notes)
{
FILE *file = fopen(fileName, "wb");

if(!file) {
	systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s"), fileName);
	return false;
}

u8 buffer[17];

utilPutDword(buffer, 0x0d); // SharkPortSave length
fwrite(buffer, 1, 4, file);
fwrite("SharkPortSave", 1, 0x0d, file);
utilPutDword(buffer, 0x000f0000);
fwrite(buffer, 1, 4, file); // save type 0x000f0000 = GBA save
utilPutDword(buffer, (u32)strlen(title));
fwrite(buffer, 1, 4, file); // title length
fwrite(title, 1, strlen(title), file);
utilPutDword(buffer, (u32)strlen(desc));
fwrite(buffer, 1, 4, file); // desc length
fwrite(desc, 1, strlen(desc), file);
utilPutDword(buffer, (u32)strlen(notes));
fwrite(buffer, 1, 4, file); // notes length
fwrite(notes, 1, strlen(notes), file);
int saveSize = 0x10000;
if(gbaSaveType == 2)
	saveSize = flashSize;
int totalSize = saveSize + 0x1c;

utilPutDword(buffer, totalSize); // length of remainder of save - CRC
fwrite(buffer, 1, 4, file);

char *temp = new char[0x2001c];
memset(temp, 0, 28);
memcpy(temp, &rom[0xa0], 16); // copy internal name
temp[0x10] = rom[0xbe]; // reserved area (old checksum)
temp[0x11] = rom[0xbf]; // reserved area (old checksum)
temp[0x12] = rom[0xbd]; // complement check
temp[0x13] = rom[0xb0]; // maker
temp[0x14] = 1; // 1 save ?
memcpy(&temp[0x1c], flashSaveMemory, saveSize); // copy save
fwrite(temp, 1, totalSize, file); // write save + header
u32 crc = 0;

for(int i = 0; i < totalSize; i++) {
	crc += ((u32)temp[i] << (crc % 0x18));
}

utilPutDword(buffer, crc);
fwrite(buffer, 1, 4, file); // CRC?

fclose(file);
delete [] temp;
return true;
}

bool CPUImportEepromFile(const char *fileName)
{
FILE *file = fopen(fileName, "rb");

if(!file)
	return false;

// check file size to know what we should read
fseek(file, 0, SEEK_END);

long size = ftell(file);
fseek(file, 0, SEEK_SET);
if(size == 512 || size == 0x2000) {
	if(fread(eepromData, 1, size, file) != (size_t)size) {
		fclose(file);
		return false;
	}
	for(int i = 0; i < size;) {
		u8 tmp = eepromData[i];
		eepromData[i] = eepromData[7-i];
		eepromData[7-i] = tmp;
		i++;
		tmp = eepromData[i];
		eepromData[i] = eepromData[7-i];
		eepromData[7-i] = tmp;
		i++;
		tmp = eepromData[i];
		eepromData[i] = eepromData[7-i];
		eepromData[7-i] = tmp;
		i++;
		tmp = eepromData[i];
		eepromData[i] = eepromData[7-i];
		eepromData[7-i] = tmp;
		i++;
		i += 4;
	}
} else {
	fclose(file);
	return false;
}
fclose(file);
return true;
}

bool CPUReadBatteryFile(const char *fileName)
{
FILE *file = fopen(fileName, "rb");

if(!file)
	return false;

// check file size to know what we should read
fseek(file, 0, SEEK_END);

long size = ftell(file);
fseek(file, 0, SEEK_SET);
systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

if(size == 512 || size == 0x2000) {
	if(fread(eepromData, 1, size, file) != (size_t)size) {
		fclose(file);
		return false;
	}
} else {
	if(size == 0x20000) {
		if(fread(flashSaveMemory, 1, 0x20000, file) != 0x20000) {
			fclose(file);
			return false;
		}
		flashSetSize(0x20000);
	} else {
		if(fread(flashSaveMemory, 1, 0x10000, file) != 0x10000) {
			fclose(file);
			return false;
		}
		flashSetSize(0x10000);
	}
}
fclose(file);
return true;
}

bool CPUWritePNGFile(const char *fileName)
{
return utilWritePNGFile(fileName, 240, 160, pix);
}

bool CPUWriteBMPFile(const char *fileName)
{
return utilWriteBMPFile(fileName, 240, 160, pix);
}

bool CPUIsZipFile(const char * file)
{
if(strlen(file) > 4) {
	const char * p = strrchr(file,'.');

	if(p != NULL) {
		if(_stricmp(p, ".zip") == 0)
			return true;
	}
}

return false;
}

bool CPUIsGBAImage(const char * file)
{
cpuIsMultiBoot = false;
if(strlen(file) > 4) {
	const char * p = strrchr(file,'.');

	if(p != NULL) {
		if(_stricmp(p, ".gba") == 0)
			return true;
		if(_stricmp(p, ".agb") == 0)
			return true;
		if(_stricmp(p, ".bin") == 0)
			return true;
		if(_stricmp(p, ".elf") == 0)
			return true;
		if(_stricmp(p, ".mb") == 0) {
			cpuIsMultiBoot = true;
			return true;
		}
	}
}

return false;
}

bool CPUIsGBABios(const char * file)
{
if(strlen(file) > 4) {
	const char * p = strrchr(file,'.');

	if(p != NULL) {
		if(_stricmp(p, ".gba") == 0)
			return true;
		if(_stricmp(p, ".agb") == 0)
			return true;
		if(_stricmp(p, ".bin") == 0)
			return true;
		if(_stricmp(p, ".bios") == 0)
			return true;
		if(_stricmp(p, ".rom") == 0)
			return true;
	}
}

return false;
}

bool CPUIsELF(const char *file)
{
if(file == NULL)
	return false;

if(strlen(file) > 4) {
	const char * p = strrchr(file,'.');

	if(p != NULL) {
		if(_stricmp(p, ".elf") == 0)
			return true;
	}
}
return false;
}


void CPUCleanUp()
{
#ifdef PROFILING
	if(profilingTicksReload) {
		profCleanup();
	}
#endif

	if(rom != NULL) {
		shmNodeCleanup(rom_node);
		rom      = NULL;
		rom_node = NULL;
	}

	if(vram != NULL) {
		munmap(vram, VRAM_SIZE);
		vram = NULL;
	}

	if(paletteRAM != NULL) {
		munmap(paletteRAM, PALETTE_SIZE);
		paletteRAM = NULL;
	}

	if(internalRAM != NULL) {
		shmNodeCleanup(internalRAM_node);
		internalRAM      = NULL;
		internalRAM_node = NULL;
	}

	if(workRAM != NULL) {
		shmNodeCleanup(workRAM_node);
		workRAM      = NULL;
		workRAM_node = NULL;
	}

	if(bios != NULL) {
		munmap(bios, BIOS_SIZE);
		bios = NULL;
	}

	if(pix != NULL) {
		free(pix);
		pix = NULL;
	}

	if(oam != NULL) {
		munmap(oam, OAM_SIZE);
		oam = NULL;
	}

	if(ioMem != NULL) {
		munmap(ioMem, IOMEM_SIZE);
		ioMem = NULL;
	}

#ifndef NO_DEBUGGER
	elfCleanUp();
#endif //NO_DEBUGGER

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

#ifndef __QNXNTO__
	emulating = 0;
#endif
}

int CPULoadRom(const char *szFile)
{
	romSize = ROM_SIZE;
	if(rom != NULL) {
		CPUCleanUp();
	}

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	rom_node = gba_createSharedMem(ROM_NAME, ROM_ADDR, ROM_SIZE);
	if (NULL == rom_node) { SLOG("Fail allocating ROM memory"); return 0; }
	rom = rom_node->memPtr;

	workRAM_node = gba_createSharedMem(WORKRAM_NAME, WORKRAM_ADDR, WORKRAM_SIZE);
	if (NULL == workRAM_node) { SLOG("Fail allocating WRAM memory"); return 0; }
	workRAM = workRAM_node->memPtr;

	u8 *whereToLoad = cpuIsMultiBoot ? workRAM : rom;

#ifndef NO_DEBUGGER
	if(CPUIsELF(szFile)) {
		FILE *f = fopen(szFile, "rb");
		if(!f) {
			systemMessage(MSG_ERROR_OPENING_IMAGE, N_("Error opening image %s"),
					szFile);
			munmap(rom, ROM_SIZE);
			munmap(workRAM, WORKRAM_SIZE);
			rom     = NULL;
			workRAM = NULL;
			return 0;
		}
		bool res = elfRead(szFile, romSize, f);
		if(!res || romSize == 0) {
			munmap(rom, ROM_SIZE);
			munmap(workRAM, WORKRAM_SIZE);
			rom     = NULL;
			workRAM = NULL;
			elfCleanUp();
			return 0;
		}
	} else
#endif //NO_DEBUGGER
		if(szFile!=NULL)
		{
			if(!utilLoad(szFile,
					utilIsGBAImage,
					whereToLoad,
					romSize)) {
				munmap(rom, ROM_SIZE);
				munmap(workRAM, WORKRAM_SIZE);
				rom     = NULL;
				workRAM = NULL;
				return 0;
			}
		}

	// Write remaining ROM location with pattern
	u16 *temp = (u16 *)(rom+((romSize+1)&~1));
	int i;
	for(i = (romSize+1)&~1; i < ROM_SIZE; i+=2) {
		WRITE16LE(temp, (i >> 1) & 0xFFFF);
		temp++;
	}

	bios = (u8 *)mmap(BIOS_ADDR, BIOS_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0);
	if(bios != BIOS_ADDR) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"BIOS");
		CPUCleanUp();
		return 0;
	}

	internalRAM_node = gba_createSharedMem(INTRAM_NAME, INTRAM_ADDR, INTRAM_SIZE);
	if (NULL == internalRAM_node) { SLOG("Fail allocating IRAM memory"); return 0; }
	internalRAM = internalRAM_node->memPtr;


	paletteRAM = (u8 *)mmap(PALETTE_ADDR, PALETTE_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0);
	if(paletteRAM != PALETTE_ADDR) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"PRAM");
		CPUCleanUp();
		return 0;
	}
	vram = (u8 *)mmap(VRAM_ADDR, VRAM_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0);
	if(vram != VRAM_ADDR) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"VRAM");
		CPUCleanUp();
		return 0;
	}
	oam = (u8 *)mmap(OAM_ADDR, OAM_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0);
	if(oam != OAM_ADDR) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"OAM");
		CPUCleanUp();
		return 0;
	}
	pix = (u8 *)calloc(1, 4 * 241 * 162);
	if(pix == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
				"PIX");
		CPUCleanUp();
		return 0;
	}
	ioMem = (u8 *)mmap(IOMEM_ADDR, IOMEM_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0);
	if(ioMem == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"), "IO");
		CPUCleanUp();
		return 0;
	}

	flashInit();
	eepromInit();

	CPUUpdateRenderBuffers(true);

	return romSize;
}

void doMirroring (bool b)
{
	u32 mirroredRomSize = (((romSize)>>20) & 0x3F)<<20;
	u32 mirroredRomAddress = romSize;
	if ((mirroredRomSize <=0x800000) && (b))
	{
		mirroredRomAddress = mirroredRomSize;
		if (mirroredRomSize==0)
			mirroredRomSize=0x100000;
		while (mirroredRomAddress<0x01000000)
		{
			memcpy ((u16 *)(rom+mirroredRomAddress), (u16 *)(rom), mirroredRomSize);
			mirroredRomAddress+=mirroredRomSize;
		}
	}
}

#define __DBGMODE(x) \
	x;	\
	SLOG(#x)

#define __NORMALMODE(x) x

void CPUUpdateRender()
{
	bRenderLineAllEnabled = false;

	switch(READ_REG(REG_DISPCNT) & DISPCNT_MODE) {
	case 0:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
		{
			__NORMALMODE(renderLine = mode0RenderLine);
		}
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
		{
			__NORMALMODE(renderLine = mode0RenderLineNoWindow);
		}
		else
		{
			__NORMALMODE(renderLine = mode0RenderLineAll);
			bRenderLineAllEnabled = true;
		}
		break;
	case 1:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
			renderLine = mode1RenderLine;
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
			renderLine = mode1RenderLineNoWindow;
		else {
			renderLine = mode1RenderLineAll;
			bRenderLineAllEnabled = true;
		}
		break;
	case 2:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
			renderLine = mode2RenderLine;
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
			renderLine = mode2RenderLineNoWindow;
		else {
			renderLine = mode2RenderLineAll;
			bRenderLineAllEnabled = true;
		}
		break;
	case 3:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
			renderLine = mode3RenderLine;
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
			renderLine = mode3RenderLineNoWindow;
		else {
			renderLine = mode3RenderLineAll;
			bRenderLineAllEnabled = true;
		}
		break;
	case 4:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
			renderLine = mode4RenderLine;
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
			renderLine = mode4RenderLineNoWindow;
		else {
			renderLine = mode4RenderLineAll;
			bRenderLineAllEnabled = true;
		}
		break;
	case 5:
		if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
				cpuDisableSfx)
			renderLine = mode5RenderLine;
		else if(fxOn && !windowOn && !(layerEnable & 0x8000))
			renderLine = mode5RenderLineNoWindow;
		else {
			renderLine = mode5RenderLineAll;
			bRenderLineAllEnabled = true;
		}
		break;
	default:
		break;
	}
}

void CPUUpdateCPSR()
{
	u32 CPSR = bus.reg[16].I & 0x40;
	if(N_FLAG)
		CPSR |= 0x80000000;
	if(Z_FLAG)
		CPSR |= 0x40000000;
	if(C_FLAG)
		CPSR |= 0x20000000;
	if(V_FLAG)
		CPSR |= 0x10000000;
	if(!armState)
		CPSR |= 0x00000020;
	if(!armIrqEnable)
		CPSR |= 0x80;
	CPSR |= (armMode & 0x1F);
	bus.reg[16].I = CPSR;
}

void CPUUpdateFlags(bool breakLoop)
{
	u32 CPSR = bus.reg[16].I;

	N_FLAG = (CPSR & 0x80000000) ? true: false;
	Z_FLAG = (CPSR & 0x40000000) ? true: false;
	C_FLAG = (CPSR & 0x20000000) ? true: false;
	V_FLAG = (CPSR & 0x10000000) ? true: false;
	armState = (CPSR & 0x20) ? false : true;
	armIrqEnable = (CPSR & 0x80) ? false : true;
	if(breakLoop) {
		if (armIrqEnable && (READ_REG(REG_IF) & READ_REG(REG_IE)) && (READ_REG(REG_IME) & 1))
			cpuNextEvent = cpuTotalTicks;
	}
}

void CPUUpdateFlags()
{
	CPUUpdateFlags(true);
}

#ifdef WORDS_BIGENDIAN
static void CPUSwap(volatile u32 *a, volatile u32 *b)
{
volatile u32 c = *b;
*b = *a;
*a = c;
}
#else
#define CPUSwap(a, b) \
*(a) ^= *(b); \
*(b) ^= *(a); \
*(a) ^= *(b);
#endif

void CPUSwitchMode(int mode, bool saveState, bool breakLoop)
{
	//  if(armMode == mode)
	//    return;

	CPUUpdateCPSR();

	switch(armMode) {
	case 0x10:
	case 0x1F:
		bus.reg[R13_USR].I = bus.reg[13].I;
		bus.reg[R14_USR].I = bus.reg[14].I;
		bus.reg[17].I = bus.reg[16].I;
		break;
	case 0x11:
		CPUSwap(&bus.reg[R8_FIQ].I, &bus.reg[8].I);
		CPUSwap(&bus.reg[R9_FIQ].I, &bus.reg[9].I);
		CPUSwap(&bus.reg[R10_FIQ].I, &bus.reg[10].I);
		CPUSwap(&bus.reg[R11_FIQ].I, &bus.reg[11].I);
		CPUSwap(&bus.reg[R12_FIQ].I, &bus.reg[12].I);
		bus.reg[R13_FIQ].I = bus.reg[13].I;
		bus.reg[R14_FIQ].I = bus.reg[14].I;
		bus.reg[SPSR_FIQ].I = bus.reg[17].I;
		break;
	case 0x12:
		bus.reg[R13_IRQ].I  = bus.reg[13].I;
		bus.reg[R14_IRQ].I  = bus.reg[14].I;
		bus.reg[SPSR_IRQ].I =  bus.reg[17].I;
		break;
	case 0x13:
		bus.reg[R13_SVC].I  = bus.reg[13].I;
		bus.reg[R14_SVC].I  = bus.reg[14].I;
		bus.reg[SPSR_SVC].I =  bus.reg[17].I;
		break;
	case 0x17:
		bus.reg[R13_ABT].I  = bus.reg[13].I;
		bus.reg[R14_ABT].I  = bus.reg[14].I;
		bus.reg[SPSR_ABT].I =  bus.reg[17].I;
		break;
	case 0x1b:
		bus.reg[R13_UND].I  = bus.reg[13].I;
		bus.reg[R14_UND].I  = bus.reg[14].I;
		bus.reg[SPSR_UND].I =  bus.reg[17].I;
		break;
	}

	u32 CPSR = bus.reg[16].I;
	u32 SPSR = bus.reg[17].I;

	switch(mode) {
	case 0x10:
	case 0x1F:
		bus.reg[13].I = bus.reg[R13_USR].I;
		bus.reg[14].I = bus.reg[R14_USR].I;
		bus.reg[16].I = SPSR;
		break;
	case 0x11:
		CPUSwap(&bus.reg[8].I, &bus.reg[R8_FIQ].I);
		CPUSwap(&bus.reg[9].I, &bus.reg[R9_FIQ].I);
		CPUSwap(&bus.reg[10].I, &bus.reg[R10_FIQ].I);
		CPUSwap(&bus.reg[11].I, &bus.reg[R11_FIQ].I);
		CPUSwap(&bus.reg[12].I, &bus.reg[R12_FIQ].I);
		bus.reg[13].I = bus.reg[R13_FIQ].I;
		bus.reg[14].I = bus.reg[R14_FIQ].I;
		if(saveState)
			bus.reg[17].I = CPSR;
		else
			bus.reg[17].I = bus.reg[SPSR_FIQ].I;
		break;
	case 0x12:
		bus.reg[13].I = bus.reg[R13_IRQ].I;
		bus.reg[14].I = bus.reg[R14_IRQ].I;
		bus.reg[16].I = SPSR;
		if(saveState)
			bus.reg[17].I = CPSR;
		else
			bus.reg[17].I = bus.reg[SPSR_IRQ].I;
		break;
	case 0x13:
		bus.reg[13].I = bus.reg[R13_SVC].I;
		bus.reg[14].I = bus.reg[R14_SVC].I;
		bus.reg[16].I = SPSR;
		if(saveState)
			bus.reg[17].I = CPSR;
		else
			bus.reg[17].I = bus.reg[SPSR_SVC].I;
		break;
	case 0x17:
		bus.reg[13].I = bus.reg[R13_ABT].I;
		bus.reg[14].I = bus.reg[R14_ABT].I;
		bus.reg[16].I = SPSR;
		if(saveState)
			bus.reg[17].I = CPSR;
		else
			bus.reg[17].I = bus.reg[SPSR_ABT].I;
		break;
	case 0x1b:
		bus.reg[13].I = bus.reg[R13_UND].I;
		bus.reg[14].I = bus.reg[R14_UND].I;
		bus.reg[16].I = SPSR;
		if(saveState)
			bus.reg[17].I = CPSR;
		else
			bus.reg[17].I = bus.reg[SPSR_UND].I;
		break;
	default:
		systemMessage(MSG_UNSUPPORTED_ARM_MODE, N_("Unsupported ARM mode %02x"), mode);
		break;
	}
	armMode = mode;
	CPUUpdateFlags(breakLoop);
	CPUUpdateCPSR();
}

void CPUSwitchMode(int mode, bool saveState)
{
	CPUSwitchMode(mode, saveState, true);
}

void CPUUndefinedException()
{
	u32 PC = bus.reg[15].I;
	bool savedArmState = armState;
	CPUSwitchMode(0x1b, true, false);
	bus.reg[14].I = PC - (savedArmState ? 4 : 2);
	bus.reg[15].I = 0x04 + BIOS_VIRTUAL_BASE;
	armState = true;
	armIrqEnable = false;
	bus.armNextPC = 0x04 + BIOS_VIRTUAL_BASE;
	ARM_PREFETCH;
	bus.reg[15].I += 4;
}

void CPUSoftwareInterrupt()
{
	u32 PC = bus.reg[15].I;
	bool savedArmState = armState;
	CPUSwitchMode(0x13, true, false);
	bus.reg[14].I = PC - (savedArmState ? 4 : 2);
	bus.reg[15].I = 0x08 + BIOS_VIRTUAL_BASE;
	armState = true;
	armIrqEnable = false;
	bus.armNextPC = 0x08 + BIOS_VIRTUAL_BASE;
	ARM_PREFETCH;
	bus.reg[15].I += 4;
}

void CPUSoftwareInterrupt(int comment)
{
	static bool disableMessage = false;
	if(armState) comment >>= 16;
#ifdef BKPT_SUPPORT
	if(comment == 0xff) {
		dbgOutput(NULL, bus.reg[0].I);
		return;
	}
#endif
#ifdef PROFILING
	if(comment == 0xfe) {
		profStartup(bus.reg[0].I, bus.reg[1].I);
		return;
	}
	if(comment == 0xfd) {
		profControl(bus.reg[0].I);
		return;
	}
	if(comment == 0xfc) {
		profCleanup();
		return;
	}
	if(comment == 0xfb) {
		profCount();
		return;
	}
#endif

#if 0
	if(comment == 0xfa) {
		agbPrintFlush();
		return;
	}
#endif

#ifdef SDL
	if(comment == 0xf9) {
		emulating = 0;
		cpuNextEvent = cpuTotalTicks;
		cpuBreakLoop = true;
		return;
	}
#endif
	if(useBios) {
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_SWI) {
			log("SWI: %08x at %08x (0x%08x,0x%08x,0x%08x,READ_REG(REG_VCOUNT] = %2d)\n", comment,
					armState ? bus.armNextPC - 4: bus.armNextPC -2,
							bus.reg[0].I,
							bus.reg[1].I,
							bus.reg[2].I,
							READ_REG(REG_VCOUNT]);
		}
#endif
		CPUSoftwareInterrupt();
		return;
	}
	// This would be correct, but it causes problems if uncommented
	//  else {
	//    biosProtected = 0xe3a02004;
	//  }

	switch(comment) {
	case 0x00: // SoftReset
		BIOS_SoftReset();
		ARM_PREFETCH;
		break;
	case 0x01: // RegisterRAMReset
		BIOS_REGISTER_RAM_RESET();
		break;
	case 0x02: // Halt
		BIOS_SLOG("Halt: (READ_REG(REG_VCOUNT) = %2d)", READ_REG(REG_VCOUNT));
		holdState = true;
		holdType = -1;
		cpuNextEvent = cpuTotalTicks;
		break;
	case 0x03: // Stop
		BIOS_SLOG("Stop: (READ_REG(REG_VCOUNT) = %2d)", READ_REG(REG_VCOUNT));
		holdState = true;
		holdType = -1;
		stopState = true;
		cpuNextEvent = cpuTotalTicks;
		break;
	case 0x04: // IntrWait
		BIOS_SLOG("IntrWait: 0x%08x,0x%08x (READ_REG(REG_VCOUNT) = %2d)",
							bus.reg[0].I,
							bus.reg[1].I,
							READ_REG(REG_VCOUNT));
		CPUSoftwareInterrupt();
		break;
	case 0x05: // VBlankIntrWait
		BIOS_SLOG("VBlankIntrWait+HALT: (READ_REG(REG_VCOUNT) = %2d)", READ_REG(REG_VCOUNT));
		CPUSoftwareInterrupt();
		break;
	case 0x06: // Div
		BIOS_Div();
		break;
	case 0x07: // DivARM
		BIOS_DivArm();
		break;
	case 0x08: // Sqrt
		BIOS_SLOG("BIOS_SQRT");
		BIOS_SQRT();
		break;
	case 0x09: // ArcTan
		BIOS_SLOG("BIOS_ArcTan");
		BIOS_ArcTan();
		break;
	case 0x0A: // ArcTan2
		BIOS_SLOG("BIOS_ArcTan2");
		BIOS_ArcTan2();
		break;
	case 0x0B:
#ifdef USE_SWITICKS
		{
			int len = (bus.reg[2].I & 0x1FFFFF) >>1;
			if (!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + len) & 0xe000000) == 0))
			{
				if ((bus.reg[2].I >> 24) & 1)
				{
					if ((bus.reg[2].I >> 26) & 1)
						SWITicks = (7 + memoryWait32[(bus.reg[1].I>>24) & 0xF]) * (len>>1);
					else
						SWITicks = (8 + memoryWait[(bus.reg[1].I>>24) & 0xF]) * (len);
				}
				else
				{
					if ((bus.reg[2].I >> 26) & 1)
						SWITicks = (10 + memoryWait32[(bus.reg[0].I>>24) & 0xF] +
								memoryWait32[(bus.reg[1].I>>24) & 0xF]) * (len>>1);
					else
						SWITicks = (11 + memoryWait[(bus.reg[0].I>>24) & 0xF] +
								memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
				}
			}
		}
#endif
		if(!(((bus.reg[0].I & 0xe000000) == 0) || ((bus.reg[0].I + (((bus.reg[2].I << 11)>>9) & 0x1fffff)) & 0xe000000) == 0))
		{
			BIOS_SLOG("BIOS_CpuSet");
			BIOS_CpuSet();
		}
		break;
	case 0x0C:
#ifdef USE_SWITICKS
		{
			int len = (bus.reg[2].I & 0x1FFFFF) >>5;
			if (!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + len) & 0xe000000) == 0))
			{
				if ((bus.reg[2].I >> 24) & 1)
					SWITicks = (6 + memoryWait32[(bus.reg[1].I>>24) & 0xF] +
							7 * (memoryWaitSeq32[(bus.reg[1].I>>24) & 0xF] + 1)) * len;
				else
					SWITicks = (9 + memoryWait32[(bus.reg[0].I>>24) & 0xF] +
							memoryWait32[(bus.reg[1].I>>24) & 0xF] +
							7 * (memoryWaitSeq32[(bus.reg[0].I>>24) & 0xF] +
									memoryWaitSeq32[(bus.reg[1].I>>24) & 0xF] + 2)) * len;
			}
		}
#endif
		if(!(((bus.reg[0].I & 0xe000000) == 0) || ((bus.reg[0].I + (((bus.reg[2].I << 11)>>9) & 0x1fffff)) & 0xe000000) == 0))
		{
			BIOS_SLOG("BIOS_CpuFastSet");
			BIOS_CpuFastSet();
		}
		break;
	case 0x0D:
		BIOS_GET_BIOS_CHECKSUM();
		break;
	case 0x0E:
		BIOS_SLOG("BIOS_BgAffineSet");
		BIOS_BgAffineSet();
		break;
	case 0x0F:
		BIOS_SLOG("BIOS_ObjAffineSet");
		BIOS_ObjAffineSet();
		break;
	case 0x10:
#ifdef USE_SWITICKS
		{
			int len = CPUReadHalfWord(bus.reg[2].I);
			if (!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + len) & 0xe000000) == 0))
				SWITicks = (32 + memoryWait[(bus.reg[0].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_BitUnPack");
		BIOS_BitUnPack();
		break;
	case 0x11:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 8;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (9 + memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_LZ77UnCompWram");
		BIOS_LZ77UnCompWram();
		break;
	case 0x12:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 8;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (19 + memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_LZ77UnCompVram");
		BIOS_LZ77UnCompVram();
		break;
	case 0x13:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 8;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (29 + (memoryWait[(bus.reg[0].I>>24) & 0xF]<<1)) * len;
		}
#endif
		BIOS_SLOG("BIOS_HuffUnComp");
		BIOS_HuffUnComp();
		break;
	case 0x14:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 8;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (11 + memoryWait[(bus.reg[0].I>>24) & 0xF] +
						memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_RLUnCompWram");
		BIOS_RLUnCompWram();
		break;
	case 0x15:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 9;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (34 + (memoryWait[(bus.reg[0].I>>24) & 0xF] << 1) +
						memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_RLUnCompVram");
		BIOS_RLUnCompVram();
		break;
	case 0x16:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 8;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (13 + memoryWait[(bus.reg[0].I>>24) & 0xF] +
						memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_Diff8bitUnFilterWram");
		BIOS_Diff8bitUnFilterWram();
		break;
	case 0x17:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 9;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (39 + (memoryWait[(bus.reg[0].I>>24) & 0xF]<<1) +
						memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_Diff8bitUnFilterVram");
		BIOS_Diff8bitUnFilterVram();
		break;
	case 0x18:
#ifdef USE_SWITICKS
		{
			u32 len = CPUReadMemory(bus.reg[0].I) >> 9;
			if(!(((bus.reg[0].I & 0xe000000) == 0) ||
					((bus.reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
				SWITicks = (13 + memoryWait[(bus.reg[0].I>>24) & 0xF] +
						memoryWait[(bus.reg[1].I>>24) & 0xF]) * len;
		}
#endif
		BIOS_SLOG("BIOS_Diff16bitUnFilter");
		BIOS_Diff16bitUnFilter();
		break;
	case 0x19:
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_SWI) {
			log("SoundBiasSet: 0x%08x (READ_REG(REG_VCOUNT) = %2d)\n",
					bus.reg[0].I,
					READ_REG(REG_VCOUNT));
		}
#endif
		if(bus.reg[0].I)
			soundPause();
		else
			soundResume();
		break;
	case 0x1F:
		BIOS_SLOG("BIOS_MIDI_KEY_2_FREQ");
		BIOS_MIDI_KEY_2_FREQ();
		break;
	case 0x2A:
		BIOS_SLOG("BIOS_SND_DRIVER_JMP_TABLE_COPY");
		BIOS_SND_DRIVER_JMP_TABLE_COPY();
		// let it go, because we don't really emulate this function
	default:
#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_SWI) {
			log("SWI: %08x at %08x (0x%08x,0x%08x,0x%08x,READ_REG(REG_VCOUNT) = %2d)\n", comment,
					armState ? bus.armNextPC - 4: bus.armNextPC -2,
							bus.reg[0].I,
							bus.reg[1].I,
							bus.reg[2].I,
							READ_REG(REG_VCOUNT));
		}
#endif

		if(!disableMessage) {
			systemMessage(MSG_UNSUPPORTED_BIOS_FUNCTION,
					N_("Unsupported BIOS function %02x called from %08x. A BIOS file is needed in order to get correct behaviour."),
					comment,
					armMode ? bus.armNextPC - 4: bus.armNextPC - 2);
			disableMessage = true;
		}
		break;
	}
}

void CPUCompareVCOUNT()
{
	u16 dispstat = READ_REG(REG_DISPSTAT);
	if(READ_REG(REG_VCOUNT) == (dispstat >> 8)) {
		dispstat |= DSTAT_IN_VCT;
		UPDATE_REG(REG_DISPSTAT, dispstat);

		if(dispstat & DSTAT_VCT_IRQ) {
			READ_REG(REG_IF) |= 4;
		}
	} else {
		dispstat &= 0xFFFB;
		UPDATE_REG(REG_DISPSTAT, dispstat);
	}
	if (layerEnableDelay>0)
	{
		layerEnableDelay--;
		if (layerEnableDelay==1)
			layerEnable = layerSettings & READ_REG(REG_DISPCNT);
	}

}

#ifdef __X86__
#define __OLDDMA__
#endif

#ifdef __OLDDMA__
void doDMA(u32 &s, u32 &d, u32 si, u32 di, u32 c, int transfer32)
{
	int sm = s >> 24;
	int dm = d >> 24;
	int sw = 0;
	int dw = 0;
	int sc = c;

	cpuDmaCount = c;
	// This is done to get the correct waitstates.
	if (sm > 15)
		sm = 15;
	if (dm > 15)
		dm = 15;

	//if ((sm>=0x05) && (sm<=0x07) || (dm>=0x05) && (dm <=0x07))
	//    blank = (((io_registers[REG_DISPSTAT] | ((io_registers[REG_DISPSTAT] >> 1)&1))==1) ?  true : false);

	if(transfer32)
	{
		s &= 0xFFFFFFFC;
		if(s < 0x02000000 && (bus.reg[15].I >> 24))
		{
			while (c != 0)
			{
				CPUWriteMemory(d, 0);
				d += di;
				c--;
			}
		}
		else
		{
			while (c != 0)
			{
				CPUWriteMemory(d, CPUReadMemory(s));
				d += di;
				s += si;
				c--;
			}
		}
	}
	else
	{
		s &= 0xFFFFFFFE;
		si = (int)si >> 1;
		di = (int)di >> 1;
		if(s < 0x02000000 && (bus.reg[15].I >> 24))
		{
			while (c != 0)
			{
				CPUWriteHalfWord(d, 0);
				d += di;
				c--;
			}
		}
		else
		{
			while (c != 0)
			{
				CPUWriteHalfWord(d, CPUReadHalfWord(s));
				d += di;
				s += si;
				c--;
			}
		}
	}

	cpuDmaCount = 0;

	if(transfer32)
	{
		sw = 1+memoryWaitSeq32[sm & 15];
		dw = 1+memoryWaitSeq32[dm & 15];
		cpuDmaTicksToUpdate += (sw+dw)*(sc-1) + 6 + memoryWait32[sm & 15] + memoryWaitSeq32[dm & 15];
	}
	else
	{
		sw = 1+memoryWaitSeq[sm & 15];
		dw = 1+memoryWaitSeq[dm & 15];
		cpuDmaTicksToUpdate += (sw+dw)*(sc-1) + 6 + memoryWait[sm & 15] + memoryWaitSeq[dm & 15];
	}
}

void CPUCheckDMA(int reason, int dmamask)
{
	u32 arrayval[] = {4, (u32)-4, 0, 4};
	// DMA 0
	if((READ_REG(REG_DM0CNT_H) & 0x8000) && (dmamask & 1))
	{
		if(((READ_REG(REG_DM0CNT_H) >> 12) & 3) == reason)
		{
			u32 sourceIncrement, destIncrement;
			u32 condition1 = ((READ_REG(REG_DM0CNT_H) >> 7) & 3);
			u32 condition2 = ((READ_REG(REG_DM0CNT_H) >> 5) & 3);
			sourceIncrement = arrayval[condition1];
			destIncrement = arrayval[condition2];
			doDMA(dma0Source, dma0Dest, sourceIncrement, destIncrement,
					READ_REG(REG_DM0CNT_L) ? READ_REG(REG_DM0CNT_L) : 0x4000,
							READ_REG(REG_DM0CNT_H) & 0x0400);

			if(READ_REG(REG_DM0CNT_H) & 0x4000)
			{
				READ_REG(REG_IF) |= 0x0100;
				UPDATE_REG(0x202, READ_REG(REG_IF));
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM0CNT_H) >> 5) & 3) == 3) {
				dma0Dest = READ_REG(REG_DM0DAD_L) | (READ_REG(REG_DM0DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM0CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM0CNT_H) &= 0x7FFF;
				UPDATE_REG(0xBA, READ_REG(REG_DM0CNT_H));
			}
		}
	}

	// DMA 1
	if((READ_REG(REG_DM1CNT_H) & 0x8000) && (dmamask & 2)) {
		if(((READ_REG(REG_DM1CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement, destIncrement;
			u32 condition1 = ((READ_REG(REG_DM1CNT_H) >> 7) & 3);
			u32 condition2 = ((READ_REG(REG_DM1CNT_H) >> 5) & 3);
			sourceIncrement = arrayval[condition1];
			destIncrement = arrayval[condition2];
			u32 di_value, c_value, transfer_value;
			if(reason == 3)
			{
				di_value = 0;
				c_value = 4;
				transfer_value = 0x0400;
			}
			else
			{
				di_value = destIncrement;
				c_value = READ_REG(REG_DM1CNT_L) ? READ_REG(REG_DM1CNT_L) : 0x4000;
				transfer_value = READ_REG(REG_DM1CNT_H) & 0x0400;
			}
			doDMA(dma1Source, dma1Dest, sourceIncrement, di_value, c_value, transfer_value);

			if(READ_REG(REG_DM1CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0200;
				UPDATE_REG(0x202, READ_REG(REG_IF));
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM1CNT_H) >> 5) & 3) == 3) {
				dma1Dest = READ_REG(REG_DM1DAD_L) | (READ_REG(REG_DM1DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM1CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM1CNT_H) &= 0x7FFF;
				UPDATE_REG(0xC6, READ_REG(REG_DM1CNT_H));
			}
		}
	}

	// DMA 2
	if((READ_REG(REG_DM2CNT_H) & 0x8000) && (dmamask & 4)) {
		if(((READ_REG(REG_DM2CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement, destIncrement;
			u32 condition1 = ((READ_REG(REG_DM2CNT_H) >> 7) & 3);
			u32 condition2 = ((READ_REG(REG_DM2CNT_H) >> 5) & 3);
			sourceIncrement = arrayval[condition1];
			destIncrement = arrayval[condition2];
			u32 di_value, c_value, transfer_value;
			if(reason == 3)
			{
				di_value = 0;
				c_value = 4;
				transfer_value = 0x0400;
			}
			else
			{
				di_value = destIncrement;
				c_value = READ_REG(REG_DM2CNT_L) ? READ_REG(REG_DM2CNT_L) : 0x4000;
				transfer_value = READ_REG(REG_DM2CNT_H) & 0x0400;
			}
			doDMA(dma2Source, dma2Dest, sourceIncrement, di_value, c_value, transfer_value);

			if(READ_REG(REG_DM2CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0400;
				UPDATE_REG(0x202, READ_REG(REG_IF));
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM2CNT_H) >> 5) & 3) == 3) {
				dma2Dest = READ_REG(REG_DM2DAD_L) | (READ_REG(REG_DM2DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM2CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM2CNT_H) &= 0x7FFF;
				UPDATE_REG(0xD2, READ_REG(REG_DM2CNT_H));
			}
		}
	}

	// DMA 3
	if((READ_REG(REG_DM3CNT_H) & 0x8000) && (dmamask & 8))
	{
		if(((READ_REG(REG_DM3CNT_H) >> 12) & 3) == reason)
		{
			u32 sourceIncrement, destIncrement;
			u32 condition1 = ((READ_REG(REG_DM3CNT_H) >> 7) & 3);
			u32 condition2 = ((READ_REG(REG_DM3CNT_H) >> 5) & 3);
			sourceIncrement = arrayval[condition1];
			destIncrement = arrayval[condition2];
			doDMA(dma3Source, dma3Dest, sourceIncrement, destIncrement,
					READ_REG(REG_DM3CNT_L) ? READ_REG(REG_DM3CNT_L) : 0x10000,
							READ_REG(REG_DM3CNT_H) & 0x0400);
			if(READ_REG(REG_DM3CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0800;
				UPDATE_REG(0x202, READ_REG(REG_IF));
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM3CNT_H) >> 5) & 3) == 3) {
				dma3Dest = READ_REG(REG_DM3DAD_L) | (READ_REG(REG_DM3DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM3CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM3CNT_H) &= 0x7FFF;
				UPDATE_REG(0xDE, READ_REG(REG_DM3CNT_H));
			}
		}
	}
}

#else
void doDMA32(u32 &s, u32 &d, u32 si, u32 di, u32 c)
{
	int sm = s >> 24;
	int dm = d >> 24;
	int sw = 0;
	int dw = 0;
	int sc = c - 1;

	cpuDmaCount = c;

	// This is done to get the correct waitstates.
	sm = __min(sm, 15);
	dm = __min(dm, 15);

	s &= 0xFFFFFFFC;
	if(s < 0x02000000 && (bus.reg[15].I >> 24)) {
		switch(dm)
		{
		case 2:
		case 3:
		case 5:
		case 7:
			while(c != 0) {
				CPUWriteMem32Direct(d, 0);
				d += di;
				c--;
			}
			break;

		default:
			CPUWriteMem32Setup(d);
			while (c != 0) {
				CPUWriteMemFast(d, 0);
				d += di;
				c--;
			}
			break;
		}
	} else {
		switch (sm)
		{
			case 2:
			case 3:
			case 5:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			{
				switch (dm)
				{
				case 2:
				case 3:
				case 5:
				case 7:
					while(c != 0) {
						cpuDmaLast = CPUReadMem32Direct(s);
						CPUWriteMem32Direct(d, cpuDmaLast);
						d += di;
						s += si;
						c--;
					}
					break;
				default:
					CPUWriteMem32Setup(d);
					while(c != 0) {
						cpuDmaLast = CPUReadMem32Direct(s);
						CPUWriteMemFast(d, cpuDmaLast);
						d += di;
						s += si;
						c--;
					}
					break;
				}

				break;
			}
			default:
			{
				switch (dm)
				{
					case 2:
					case 3:
					case 5:
					case 7:
					{
						CPUReadMem32Setup(s);
						while(c != 0) {
							cpuDmaLast = CPUReadMemFast(s);
							CPUWriteMem32Direct(d, cpuDmaLast);
							d += di;
							s += si;
							c--;
						}
						break;
					}
					default:
					{
						CPUWriteMem32Setup(d);
						CPUReadMem32Setup(s);
						while(c != 0) {
							cpuDmaLast = CPUReadMemFast(s);
							CPUWriteMemFast(d, cpuDmaLast);
							d += di;
							s += si;
							c--;
						}
						break;
					}
				}

				break;
			}
		}
	}

	cpuDmaCount = 0;

	int totalTicks = 0;

	sw =1+memoryWaitSeq32[sm];
	dw =1+memoryWaitSeq32[dm];
	totalTicks = (sw+dw)*(sc) + 6 + memoryWait32[sm] + memoryWaitSeq32[dm];

	cpuDmaTicksToUpdate += totalTicks;
}


void doDMA16(u32 &s, u32 &d, u32 si, u32 di, u32 c)
{
	int sm = s >> 24;
	int dm = d >> 24;
	int sw = 0;
	int dw = 0;
	int sc = c - 1;

	cpuDmaCount = c;

	// This is done to get the correct waitstates.
	sm = __min(sm, 15);
	dm = __min(dm, 15);

	s &= 0xFFFFFFFE;
	si = (int)si >> 1;
	di = (int)di >> 1;
	if(s < 0x02000000 && (bus.reg[15].I >> 24)) {
		switch(dm)
		{
		case 2:
		case 3:
		case 5:
		case 7:
			while(c != 0) {
				CPUWriteMem16Direct(d, 0);
				d += di;
				c--;
			}
			break;

		default:
			CPUWriteMem16Setup(d);
			while (c != 0) {
				CPUWriteMemFast(d, 0);
				d += di;
				c--;
			}
			break;
		}
	} else {
		switch (sm)
		{
			case 2:
			case 3:
			case 5:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			{
				switch (dm)
				{
				case 2:
				case 3:
				case 5:
				case 7:
					while(c != 0) {
						cpuDmaLast = CPUReadMem16Direct(s);
						CPUWriteMem16Direct(d, cpuDmaLast);
						cpuDmaLast |= (cpuDmaLast<<16);
						d += di;
						s += si;
						c--;
					}
					break;
				default:
					CPUWriteMem16Setup(d);
					while(c != 0) {
						cpuDmaLast = CPUReadMem16Direct(s);
						CPUWriteMemFast(d, cpuDmaLast);
						cpuDmaLast |= (cpuDmaLast<<16);
						d += di;
						s += si;
						c--;
					}
					break;
				}

				break;
			}
			default:
			{
				switch (dm)
				{
					case 2:
					case 3:
					case 5:
					case 7:
					{
						CPUReadMem16Setup(s);
						while(c != 0) {
							cpuDmaLast = CPUReadMemFast(s);
							CPUWriteMem16Direct(d, cpuDmaLast);
							cpuDmaLast |= (cpuDmaLast<<16);
							d += di;
							s += si;
							c--;
						}
						break;
					}
					default:
					{
						CPUWriteMem16Setup(d);
						CPUReadMem16Setup(s);
						while(c != 0) {
							cpuDmaLast = CPUReadMemFast(s);
							CPUWriteMemFast(d, cpuDmaLast);
							cpuDmaLast |= (cpuDmaLast<<16);
							d += di;
							s += si;
							c--;
						}
						break;
					}
				}

				break;
			}
		}
	}

	cpuDmaCount = 0;

	int totalTicks = 0;

	sw = 1+memoryWaitSeq[sm];
	dw = 1+memoryWaitSeq[dm];
	totalTicks = (sw+dw)*(sc) + 6 + memoryWait[sm] + memoryWaitSeq[dm];

	cpuDmaTicksToUpdate += totalTicks;
}

static u32 stepCount[4] = {4, (u32)-4, 0, 4};
typedef void (*dmaFunc_t)(u32 &s, u32 &d, u32 si, u32 di, u32 c);

dmaFunc_t dmaFunc[2] = { doDMA16, doDMA32 };

void CPUCheckDMA(int reason, int dmamask)
{
	// DMA 0
	if((READ_REG(REG_DM0CNT_H) & 0x8000) && (dmamask & 1)) {
		if(((READ_REG(REG_DM0CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement = stepCount[(READ_REG(REG_DM0CNT_H) >> 7) & 3];
			u32 destIncrement   = stepCount[(READ_REG(REG_DM0CNT_H) >> 5) & 3];
			int dwordDma        = (READ_REG(REG_DM0CNT_H) >> 10) & 0x1;

			dmaFunc[dwordDma](dma0Source, dma0Dest, sourceIncrement, destIncrement,
					READ_REG(REG_DM0CNT_L) ? READ_REG(REG_DM0CNT_L) : 0x4000);

			cpuDmaHack = true;

			if(READ_REG(REG_DM0CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0100;
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM0CNT_H) >> 5) & 3) == 3) {
				dma0Dest = READ_REG(REG_DM0DAD_L) | (READ_REG(REG_DM0DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM0CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM0CNT_H) &= 0x7FFF;
			}
		}
	}

	// DMA 1
	if((READ_REG(REG_DM1CNT_H) & 0x8000) && (dmamask & 2)) {
		if(((READ_REG(REG_DM1CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement = stepCount[(READ_REG(REG_DM1CNT_H) >> 7) & 3];
			u32 destIncrement   = stepCount[(READ_REG(REG_DM1CNT_H) >> 5) & 3];
			int dwordDma        = (READ_REG(REG_DM1CNT_H) >> 10) & 0x1;

			if(reason == 3) {
				doDMA32(dma1Source, dma1Dest, sourceIncrement, 0, 4);
			} else {
				dmaFunc[dwordDma](dma1Source, dma1Dest, sourceIncrement, destIncrement,
						READ_REG(REG_DM1CNT_L) ? READ_REG(REG_DM1CNT_L) : 0x4000);
			}
			cpuDmaHack = true;

			if(READ_REG(REG_DM1CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0200;
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM1CNT_H) >> 5) & 3) == 3) {
				dma1Dest = READ_REG(REG_DM1DAD_L) | (READ_REG(REG_DM1DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM1CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM1CNT_H) &= 0x7FFF;
			}
		}
	}

	// DMA 2
	if((READ_REG(REG_DM2CNT_H) & 0x8000) && (dmamask & 4)) {
		if(((READ_REG(REG_DM2CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement = stepCount[(READ_REG(REG_DM2CNT_H) >> 7) & 3];
			u32 destIncrement   = stepCount[(READ_REG(REG_DM2CNT_H) >> 5) & 3];
			int dwordDma        = (READ_REG(REG_DM2CNT_H) >> 10) & 0x1;

			if(reason == 3) {
				doDMA32(dma2Source, dma2Dest, sourceIncrement, 0, 4);
			} else {
				dmaFunc[dwordDma](dma2Source, dma2Dest, sourceIncrement, destIncrement,
						READ_REG(REG_DM2CNT_L) ? READ_REG(REG_DM2CNT_L) : 0x4000);
			}
			cpuDmaHack = true;

			if(READ_REG(REG_DM2CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0400;
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM2CNT_H) >> 5) & 3) == 3) {
				dma2Dest = READ_REG(REG_DM2DAD_L) | (READ_REG(REG_DM2DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM2CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM2CNT_H) &= 0x7FFF;
			}
		}
	}

	// DMA 3
	if((READ_REG(REG_DM3CNT_H) & 0x8000) && (dmamask & 8)) {
		if(((READ_REG(REG_DM3CNT_H) >> 12) & 3) == reason) {
			u32 sourceIncrement = stepCount[(READ_REG(REG_DM3CNT_H) >> 7) & 3];
			u32 destIncrement   = stepCount[(READ_REG(REG_DM3CNT_H) >> 5) & 3];
			int dwordDma        = (READ_REG(REG_DM3CNT_H) >> 10) & 0x1;

			dmaFunc[dwordDma](dma3Source, dma3Dest, sourceIncrement, destIncrement,
					READ_REG(REG_DM3CNT_L) ? READ_REG(REG_DM3CNT_L) : 0x10000);

			if(READ_REG(REG_DM3CNT_H) & 0x4000) {
				READ_REG(REG_IF) |= 0x0800;
				cpuNextEvent = cpuTotalTicks;
			}

			if(((READ_REG(REG_DM3CNT_H) >> 5) & 3) == 3) {
				dma3Dest = READ_REG(REG_DM3DAD_L) | (READ_REG(REG_DM3DAD_H) << 16);
			}

			if(!(READ_REG(REG_DM3CNT_H) & 0x0200) || (reason == 0)) {
				READ_REG(REG_DM3CNT_H) &= 0x7FFF;
			}
		}
	}
}
#endif

//--------------------------------------- Register Update optimization -------------------------------

static inline void cpuUpdReg00(u32 address, u16 value)
{
	u16 dispcnt = READ_REG(REG_DISPCNT);
	if((value & 7) > 5) {
		// display modes above 0-5 are prohibited
		dispcnt = (value & DISPCNT_MODE);
	}
	bool change    = (0 != ((dispcnt ^ value) & DISPCNT_FB));
	bool changeBG  = (0 != ((dispcnt ^ value) & (DISPCNT_BG0|DISPCNT_BG1|DISPCNT_BG2|DISPCNT_BG3) ) );
	u16 changeBGon = ((~dispcnt) & value) & (DISPCNT_BG0|DISPCNT_BG1|DISPCNT_BG2|DISPCNT_BG3); // these layers are being activated

	dispcnt = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
	UPDATE_REG(REG_DISPCNT, dispcnt);

	layerEnable = layerSettings & value;
	if(changeBGon) {
		layerEnableDelay = 4;
		layerEnable &= (~changeBGon);
	}

	windowOn = (layerEnable & 0x6000) ? true : false;
	if(change && !((value & 0x80))) {
		if(!(READ_REG(REG_DISPSTAT) & DSTAT_IN_VBL)) {
			lcdTicks = 1008;
			//      READ_REG(REG_VCOUNT) = 0;
			//      UPDATE_REG(0x06, READ_REG(REG_VCOUNT));
			READ_REG(REG_DISPSTAT) &= 0xFFFC;
			CPUCompareVCOUNT();
		}
	}
	CPUUpdateRender();
	// we only care about changes in BG0-BG3
	if(changeBG) {
		CPUUpdateRenderBuffers(false);
	}
}

static inline void cpuUpdReg04(u32 address, u16 value)
{
	READ_REG(REG_DISPSTAT) = (value & 0xFF38) | (READ_REG(REG_DISPSTAT) & (DSTAT_IN_VBL|DSTAT_IN_HBL|DSTAT_IN_VCT));
}

#ifdef __GBAREG_OPTIMIZE__

static void cpuUpdRegNone(u32 address, u16 value)
{
}

static void cpuUpdRegGeneral(u32 address, u16 value)
{
	static u16 regMask[1024] = {
	//      	   0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F
	/*0000*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xDFCF,0x0000,0xDFCF,0x0000,0xFFCF,0x0000,0xFFCF,0x0000,
	/*0010*/	0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,0x01FF,0x0000,
	/*0020*/	0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,
	/*0030*/	0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,
	/*0040*/	0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0xFFFF,0x0000,0x3F3F,0x0000,0x3F3F,0x0000,0xFFFF,0x0000,0x0000,0x0000,
	/*0050*/	0x3FFF,0x0000,0x1F1F,0x0000,0x001F,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*0060*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*0070*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*0080*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*0090*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*00A0*/	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	/*00B0*/	0xFFFF,0x0000,0x07FF,0x0000,0xFFFF,0x0000,0x07FF,0x0000,0x3FFF,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,
	/*00C0*/	0xFFFF,0x0000,0x07FF,0x0000,0x3FFF,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,0xFFFF,0x0000,0x07FF,0x0000,
	/*00D0*/	0x3FFF,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,0xFFFF,0x0000,0x0FFF,0x0000,0x0000,0x0000,0x0000,0x0000,
	};


	UPDATE_REG(address, (value & regMask[address]));
}


static void cpuUpdReg28_2A(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	gfxBG2Changed |= 1;
}

static void cpuUpdReg2C_2E(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	gfxBG2Changed |= 2;
}

static void cpuUpdReg38_3A(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	gfxBG3Changed |= 1;
}

static void cpuUpdReg3C_3E(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	gfxBG3Changed |= 2;
}

static void cpuUpdReg40(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	CPUUpdateWindow0();
}

static void cpuUpdReg42(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	CPUUpdateWindow1();
}

static void cpuUpdRegSnd8(u32 address, u16 value)
{
	soundEvent_u8(address&0xFF, (u8)(value & 0xFF));
	soundEvent_u8((address&0xFF)+1, (u8)(value>>8));
}

static void cpuUpdReg50(u32 address, u16 value)
{
	cpuUpdRegGeneral(address, value);
	fxOn = ((io_registers[REG_BLDMOD]>>6)&3) != 0;
	CPUUpdateRender();
}

static void cpuUpdRegB8(u32 address, u16 value)
{
	io_registers[REG_DM0CNT_L] = value & 0x3FFF;
	UPDATE_REG(0xB8, 0);
}

static void cpuUpdRegBA(u32 address, u16 value)
{
	bool start = ((io_registers[REG_DM0CNT_H] ^ value) & 0x8000) ? true : false;
	value &= 0xF7E0;

	io_registers[REG_DM0CNT_H] = value;
	UPDATE_REG(0xBA, io_registers[REG_DM0CNT_H]);

	if(start && (value & 0x8000)) {
		dma0Source = io_registers[REG_DM0SAD_L] | (io_registers[REG_DM0SAD_H] << 16);
		dma0Dest = io_registers[REG_DM0DAD_L] | (io_registers[REG_DM0DAD_H] << 16);
		CPUCheckDMA(0, 1);
	}
}

static void cpuUpdRegC4(u32 address, u16 value)
{
	io_registers[REG_DM1CNT_L] = value & 0x3FFF;
	UPDATE_REG(0xC4, 0);
}

static void cpuUpdRegC6(u32 address, u16 value)
{
	bool start = ((io_registers[REG_DM1CNT_H] ^ value) & 0x8000) ? true : false;
	value &= 0xF7E0;

	io_registers[REG_DM1CNT_H] = value;
	UPDATE_REG(0xC6, io_registers[REG_DM1CNT_H]);

	if(start && (value & 0x8000)) {
		dma1Source = io_registers[REG_DM1SAD_L] | (io_registers[REG_DM1SAD_H] << 16);
		dma1Dest = io_registers[REG_DM1DAD_L] | (io_registers[REG_DM1DAD_H] << 16);
		CPUCheckDMA(0, 2);
	}
}

static void cpuUpdRegD0(u32 address, u16 value)
{
	io_registers[REG_DM2CNT_L] = value & 0x3FFF;
	UPDATE_REG(0xD0, 0);
}


static void cpuUpdRegD2(u32 address, u16 value)
{
	bool start = ((io_registers[REG_DM2CNT_H] ^ value) & 0x8000) ? true : false;

	value &= 0xF7E0;

	io_registers[REG_DM2CNT_H] = value;
	UPDATE_REG(0xD2, io_registers[REG_DM2CNT_H]);

	if(start && (value & 0x8000)) {
		dma2Source = io_registers[REG_DM2SAD_L] | (io_registers[REG_DM2SAD_H] << 16);
		dma2Dest = io_registers[REG_DM2DAD_L] | (io_registers[REG_DM2DAD_H] << 16);

		CPUCheckDMA(0, 4);
	}
}

static void cpuUpdRegDC(u32 address, u16 value)
{
	io_registers[REG_DM3CNT_L] = value;
	UPDATE_REG(0xDC, 0);
}

static void cpuUpdRegDE(u32 address, u16 value)
{
	bool start = ((io_registers[REG_DM3CNT_H] ^ value) & 0x8000) ? true : false;

	value &= 0xFFE0;

	io_registers[REG_DM3CNT_H] = value;
	UPDATE_REG(0xDE, io_registers[REG_DM3CNT_H]);

	if(start && (value & 0x8000)) {
		dma3Source = io_registers[REG_DM3SAD_L] | (io_registers[REG_DM3SAD_H] << 16);
		dma3Dest = io_registers[REG_DM3DAD_L] | (io_registers[REG_DM3DAD_H] << 16);
		CPUCheckDMA(0,8);
	}
}

static void cpuUpdReg100(u32 address, u16 value)
{
	timer0Reload = value;
}

static void cpuUpdReg102(u32 address, u16 value)
{
	timer0Value = value;
	timerOnOffDelay|=1;
	cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdReg104(u32 address, u16 value)
{
	timer1Reload = value;
}

static void cpuUpdReg106(u32 address, u16 value)
{
	timer1Value = value;
	timerOnOffDelay|=2;
	cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdReg108(u32 address, u16 value)
{
	timer2Reload = value;
}

static void cpuUpdReg10A(u32 address, u16 value)
{
	timer2Value = value;
	timerOnOffDelay|=4;
	cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdReg10C(u32 address, u16 value)
{
	timer3Reload = value;
}

static void cpuUpdReg10E(u32 address, u16 value)
{
	timer3Value = value;
	timerOnOffDelay|=8;
	cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdRegCOMM_SIOCNT(u32 address, u16 value)
{
	SLOG("-----------> COMM_SIOCNT, value=0x%.8X", value);
	StartLink(value);
}

static void cpuUpdRegCOMM_SIODATA8(u32 address, u16 value)
{
	SLOG("-----------> COMM_SIODATA8, value=0x%.8X", value);
	if (gba_link_enabled)
		LinkSSend(value);
	UPDATE_REG(COMM_SIODATA8, value);
}

static void cpuUpdReg130(u32 address, u16 value)
{
	READ_REG(REG_P1) |= (value & 0x3FF);
	UPDATE_REG(0x130, READ_REG(REG_P1));
}

static void cpuUpdReg132(u32 address, u16 value)
{
	UPDATE_REG(0x132, value & 0xC3FF);
}

static void cpuUpdRegCOMM_RCNT(u32 address, u16 value)
{
	SLOG("-----------> COMM_RCNT, value=0x%.8X", value);
	StartGPLink(value);
}

static void cpuUpdRegCOMM_JOYCNT(u32 address, u16 value)
{
	u16 cur = READ16LE(&ioMem[COMM_JOYCNT]);

	if (value & JOYCNT_RESET)			cur &= ~JOYCNT_RESET;
	if (value & JOYCNT_RECV_COMPLETE)	cur &= ~JOYCNT_RECV_COMPLETE;
	if (value & JOYCNT_SEND_COMPLETE)	cur &= ~JOYCNT_SEND_COMPLETE;
	if (value & JOYCNT_INT_ENABLE)	cur |= JOYCNT_INT_ENABLE;

	UPDATE_REG(COMM_JOYCNT, cur);
}

static void cpuUpdRegCOMM_JOY_RECV_L(u32 address, u16 value)
{
	UPDATE_REG(COMM_JOY_RECV_L, value);
}

static void cpuUpdRegCOMM_JOY_RECV_H(u32 address, u16 value)
{
	UPDATE_REG(COMM_JOY_RECV_H, value);
}

static void cpuUpdRegCOMM_JOY_TRANS_L(u32 address, u16 value)
{
	UPDATE_REG(COMM_JOY_TRANS_L, value);
	UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_SEND);
}

static void cpuUpdRegCOMM_JOY_TRANS_H(u32 address, u16 value)
{
	UPDATE_REG(COMM_JOY_TRANS_H, value);
}

static void cpuUpdRegCOMM_JOYSTAT(u32 address, u16 value)
{
	UPDATE_REG(COMM_JOYSTAT, (READ16LE(&ioMem[COMM_JOYSTAT]) & 0xf) | (value & 0xf0));
}

static void cpuUpdReg200(u32 address, u16 value)
{
	UPDATE_REG(REG_IE, value & 0x3FFF);
	if ((READ_REG(REG_IME) & 1) && (io_registers[REG_IF] & READ_REG(REG_IE)) && armIrqEnable)
		cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdReg202(u32 address, u16 value)
{
	io_registers[REG_IF] ^= (value & io_registers[REG_IF]);
	UPDATE_REG(0x202, io_registers[REG_IF]);
}

static void cpuUpdReg204(u32 address, u16 value)
{
	memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakRamWaitState[value & 3];

	if(!speedHack) {
		memoryWait[0x08] = memoryWait[0x09] = gamepakWaitState[(value >> 2) & 3];
		memoryWaitSeq[0x08] = memoryWaitSeq[0x09] =
				gamepakWaitState0[(value >> 4) & 1];

		memoryWait[0x0a] = memoryWait[0x0b] = gamepakWaitState[(value >> 5) & 3];
		memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] =
				gamepakWaitState1[(value >> 7) & 1];

		memoryWait[0x0c] = memoryWait[0x0d] = gamepakWaitState[(value >> 8) & 3];
		memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] =
				gamepakWaitState2[(value >> 10) & 1];
	} else {
		memoryWait[0x08] = memoryWait[0x09] = 3;
		memoryWaitSeq[0x08] = memoryWaitSeq[0x09] = 1;

		memoryWait[0x0a] = memoryWait[0x0b] = 3;
		memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] = 1;

		memoryWait[0x0c] = memoryWait[0x0d] = 3;
		memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] = 1;
	}

	for(int i = 8; i < 15; i++) {
		memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
		memoryWaitSeq32[i] = memoryWaitSeq[i]*2 + 1;
	}

	if((value & 0x4000) == 0x4000) {
		bus.busPrefetchEnable = true;
		bus.busPrefetch = false;
		bus.busPrefetchCount = 0;
	} else {
		bus.busPrefetchEnable = false;
		bus.busPrefetch = false;
		bus.busPrefetchCount = 0;
	}
	UPDATE_REG(0x204, value & 0x7FFF);
}

static void cpuUpdReg208(u32 address, u16 value)
{
	UPDATE_REG(REG_IME, value & 1);
	if ((READ_REG(REG_IME) & 1) && (io_registers[REG_IF] & READ_REG(REG_IE)) && armIrqEnable)
		cpuNextEvent = cpuTotalTicks;
}

static void cpuUpdReg300(u32 address, u16 value)
{
	if(value != 0)
		value &= 0xFFFE;
	UPDATE_REG(0x300, value);
}


static void cpuUpdateRegister(u32 address, u16 value)
{
	UPDATE_REG(address&0x3FE, value);
}



gbaRegUpdate_t gbaRegUpdateFunc[1024];

#else

void CPUUpdateRegister(u32 address, u16 value)
{
	switch(address)
	{
	case 0x00:
		cpuUpdReg00(address, value);
		break;
	case 0x04:
		cpuUpdReg04(address, value);
		break;
	case 0x06:
		// not writable
		break;
	case 0x08:
		UPDATE_REG(REG_BG0CNT, (value & 0xDFCF));
		break;
	case 0x0A:
		UPDATE_REG(REG_BG1CNT, (value & 0xDFCF));
		break;
	case 0x0C:
		UPDATE_REG(REG_BG2CNT, (value & 0xFFCF));
		break;
	case 0x0E:
		UPDATE_REG(REG_BG3CNT, (value & 0xFFCF));
		break;
	case 0x10:
		UPDATE_REG(REG_BG0HOFS, value & 511);
		break;
	case 0x12:
		UPDATE_REG(REG_BG0VOFS, value & 511);
		break;
	case 0x14:
		UPDATE_REG(REG_BG1HOFS, value & 511);
		break;
	case 0x16:
		UPDATE_REG(REG_BG1VOFS, value & 511);
		break;
	case 0x18:
		UPDATE_REG(REG_BG2HOFS, value & 511);
		break;
	case 0x1A:
		UPDATE_REG(REG_BG2VOFS, value & 511);
		break;
	case 0x1C:
		UPDATE_REG(REG_BG3HOFS, value & 511);
		break;
	case 0x1E:
		UPDATE_REG(REG_BG3VOFS, value & 511);
		break;
	case 0x20:
		UPDATE_REG(REG_BG2PA, value);
		break;
	case 0x22:
		UPDATE_REG(REG_BG2PB, value);
		break;
	case 0x24:
		UPDATE_REG(REG_BG2PC, value);
		break;
	case 0x26:
		UPDATE_REG(REG_BG2PD, value);
		break;
	case 0x28:
		UPDATE_REG(REG_BG2X_L, value);
		gfxBG2Changed |= 1;
		break;
	case 0x2A:
		UPDATE_REG(REG_BG2X_H, (value & 0xFFF));
		gfxBG2Changed |= 1;
		break;
	case 0x2C:
		UPDATE_REG(REG_BG2Y_L, value);
		gfxBG2Changed |= 2;
		break;
	case 0x2E:
		UPDATE_REG(REG_BG2Y_H, value & 0xFFF);
		gfxBG2Changed |= 2;
		break;
	case 0x30:
		UPDATE_REG(REG_BG3PA, value);
		break;
	case 0x32:
		UPDATE_REG(REG_BG3PB, value);
		break;
	case 0x34:
		UPDATE_REG(REG_BG3PC, value);
		break;
	case 0x36:
		UPDATE_REG(REG_BG3PD, value);
		break;
	case 0x38:
		UPDATE_REG(REG_BG3X_L, value);
		gfxBG3Changed |= 1;
		break;
	case 0x3A:
		UPDATE_REG(REG_BG3X_H, value & 0xFFF);
		gfxBG3Changed |= 1;
		break;
	case 0x3C:
		UPDATE_REG(REG_BG3Y_L, value);
		gfxBG3Changed |= 2;
		break;
	case 0x3E:
		UPDATE_REG(REG_BG3Y_H, value & 0xFFF);
		gfxBG3Changed |= 2;
		break;
	case 0x40:
		UPDATE_REG(REG_WIN0H, value);
		CPUUpdateWindow0();
		break;
	case 0x42:
		UPDATE_REG(REG_WIN1H, value);
		CPUUpdateWindow1();
		break;
	case 0x44:
		UPDATE_REG(REG_WIN0V, value);
		break;
	case 0x46:
		UPDATE_REG(REG_WIN1V, value);
		break;
	case 0x48:
		UPDATE_REG(REG_WININ, value & 0x3F3F);
		break;
	case 0x4A:
		UPDATE_REG(REG_WINOUT, value & 0x3F3F);
		break;
	case 0x4C:
		UPDATE_REG(REG_MOSAIC, value);
		break;
	case 0x50:
		UPDATE_REG(REG_BLDMOD, value & 0x3FFF);
		fxOn = ((value >> 6)&3) != 0;
		CPUUpdateRender();
		break;
	case 0x52:
		UPDATE_REG(REG_COLEV, value & 0x1F1F);
		break;
	case 0x54:
		UPDATE_REG(REG_COLY, value & 0x1F);
		break;
	case 0x60:
	case 0x62:
	case 0x64:
	case 0x68:
	case 0x6c:
	case 0x70:
	case 0x72:
	case 0x74:
	case 0x78:
	case 0x7c:
	case 0x80:
	case 0x84:
		soundEvent_u8(address&0xFF, (u8)(value & 0xFF));
		soundEvent_u8((address&0xFF)+1, (u8)(value>>8));
		break;
	case 0x82:
	case 0x88:
	case 0xa0:
	case 0xa2:
	case 0xa4:
	case 0xa6:
	case 0x90:
	case 0x92:
	case 0x94:
	case 0x96:
	case 0x98:
	case 0x9a:
	case 0x9c:
	case 0x9e:
		soundEvent_u16(address&0xFF, value);
		break;
	case 0xB0:
		UPDATE_REG(REG_DM0SAD_L, value);
		break;
	case 0xB2:
		UPDATE_REG(REG_DM0SAD_H, value & 0x07FF);
		break;
	case 0xB4:
		UPDATE_REG(REG_DM0DAD_L, value);
		break;
	case 0xB6:
		UPDATE_REG(REG_DM0DAD_H, value & 0x07FF);
		break;
	case 0xB8:
		UPDATE_REG(REG_DM0CNT_L, value & 0x3FFF);
		break;
	case 0xBA:
	{
		bool start = ((READ_REG(REG_DM0CNT_H) ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		UPDATE_REG(REG_DM0CNT_H, value);

		if(start && (value & 0x8000)) {
			dma0Source = READ_REG(REG_DM0SAD_L) | (READ_REG(REG_DM0SAD_H) << 16);
			dma0Dest = READ_REG(REG_DM0DAD_L) | (READ_REG(REG_DM0DAD_H) << 16);
			CPUCheckDMA(0, 1);
		}
	}
	break;
	case 0xBC:
		UPDATE_REG(REG_DM1SAD_L, value);
		break;
	case 0xBE:
		UPDATE_REG(REG_DM1SAD_H, value & 0x0FFF);
		break;
	case 0xC0:
		UPDATE_REG(REG_DM1DAD_L, value);
		break;
	case 0xC2:
		UPDATE_REG(REG_DM1DAD_H, value & 0x07FF);
		break;
	case 0xC4:
		UPDATE_REG(REG_DM1CNT_L, value & 0x3FFF);
		break;
	case 0xC6:
	{
		bool start = ((READ_REG(REG_DM1CNT_H) ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		UPDATE_REG(REG_DM1CNT_H, value);

		if(start && (value & 0x8000)) {
			dma1Source = READ_REG(REG_DM1SAD_L) | (READ_REG(REG_DM1SAD_H) << 16);
			dma1Dest = READ_REG(REG_DM1DAD_L) | (READ_REG(REG_DM1DAD_H) << 16);
			CPUCheckDMA(0, 2);
		}
	}
	break;
	case 0xC8:
		UPDATE_REG(REG_DM2SAD_L, value);
		break;
	case 0xCA:
		UPDATE_REG(REG_DM2SAD_H, value & 0x0FFF);
		break;
	case 0xCC:
		UPDATE_REG(REG_DM2DAD_L, value);
		break;
	case 0xCE:
		UPDATE_REG(REG_DM2DAD_H, value & 0x07FF);
		break;
	case 0xD0:
		UPDATE_REG(REG_DM2CNT_L, value & 0x3FFF);
		break;
	case 0xD2:
	{
		bool start = ((READ_REG(REG_DM2CNT_H) ^ value) & 0x8000) ? true : false;

		value &= 0xF7E0;

		UPDATE_REG(REG_DM2CNT_H, value);

		if(start && (value & 0x8000)) {
			dma2Source = READ_REG(REG_DM2SAD_L) | (READ_REG(REG_DM2SAD_H) << 16);
			dma2Dest = READ_REG(REG_DM2DAD_L) | (READ_REG(REG_DM2DAD_H) << 16);

			CPUCheckDMA(0, 4);
		}
	}
	break;
	case 0xD4:
		UPDATE_REG(REG_DM3SAD_L, value);
		break;
	case 0xD6:
		UPDATE_REG(REG_DM3SAD_H, value & 0x0FFF);
		break;
	case 0xD8:
		UPDATE_REG(REG_DM3DAD_L, value);
		break;
	case 0xDA:
		UPDATE_REG(REG_DM3DAD_H, value & 0x0FFF);
		break;
	case 0xDC:
		UPDATE_REG(REG_DM3CNT_L, value);
		break;
	case 0xDE:
	{
		bool start = ((READ_REG(REG_DM3CNT_H) ^ value) & 0x8000) ? true : false;

		value &= 0xFFE0;

		UPDATE_REG(REG_DM3CNT_H, value);

		if(start && (value & 0x8000)) {
			dma3Source = READ_REG(REG_DM3SAD_L) | (READ_REG(REG_DM3SAD_H) << 16);
			dma3Dest = READ_REG(REG_DM3DAD_L) | (READ_REG(REG_DM3DAD_H) << 16);
			CPUCheckDMA(0,8);
		}
	}
	break;
	case 0x100:
		timer0Reload = value;
		break;
	case 0x102:
		timer0Value = value;
		timerOnOffDelay|=1;
		cpuNextEvent = cpuTotalTicks;
		break;
	case 0x104:
		timer1Reload = value;
		break;
	case 0x106:
		timer1Value = value;
		timerOnOffDelay|=2;
		cpuNextEvent = cpuTotalTicks;
		break;
	case 0x108:
		timer2Reload = value;
		break;
	case 0x10A:
		timer2Value = value;
		timerOnOffDelay|=4;
		cpuNextEvent = cpuTotalTicks;
		break;
	case 0x10C:
		timer3Reload = value;
		break;
	case 0x10E:
		timer3Value = value;
		timerOnOffDelay|=8;
		cpuNextEvent = cpuTotalTicks;
		break;


	case COMM_SIOCNT:
		StartLink(value);
		break;

	case COMM_SIODATA8:
		if (gba_link_enabled)
			LinkSSend(value);
		UPDATE_REG(COMM_SIODATA8, value);
		break;

	case 0x130:
		READ_REG(REG_P1) |= (value & 0x3FF);
		break;

	case 0x132:
		UPDATE_REG(REG_P1CNT, value & 0xC3FF);
		break;

	case COMM_RCNT:
		StartGPLink(value);
		break;

	case COMM_JOYCNT:
	{
		u16 cur = READ16LE(&ioMem[COMM_JOYCNT]);

		if (value & JOYCNT_RESET)           cur &= ~JOYCNT_RESET;
		if (value & JOYCNT_RECV_COMPLETE)   cur &= ~JOYCNT_RECV_COMPLETE;
		if (value & JOYCNT_SEND_COMPLETE)   cur &= ~JOYCNT_SEND_COMPLETE;
		if (value & JOYCNT_INT_ENABLE)      cur |= JOYCNT_INT_ENABLE;

		UPDATE_REG(COMM_JOYCNT, cur);
	}
	break;

	case COMM_JOY_RECV_L:
		UPDATE_REG(COMM_JOY_RECV_L, value);
		break;
	case COMM_JOY_RECV_H:
		UPDATE_REG(COMM_JOY_RECV_H, value);
		break;

	case COMM_JOY_TRANS_L:
		UPDATE_REG(COMM_JOY_TRANS_L, value);
		UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_SEND);
		break;
	case COMM_JOY_TRANS_H:
		UPDATE_REG(COMM_JOY_TRANS_H, value);
		break;

	case COMM_JOYSTAT:
		UPDATE_REG(COMM_JOYSTAT, (READ16LE(&ioMem[COMM_JOYSTAT]) & 0xf) | (value & 0xf0));
		break;

	case 0x200:
		UPDATE_REG(REG_IE, value & 0x3FFF);
		if ((READ_REG(REG_IME) & 1) && (READ_REG(REG_IF) & READ_REG(REG_IE)) && armIrqEnable)
			cpuNextEvent = cpuTotalTicks;
		break;
	case 0x202:
		READ_REG(REG_IF) ^= (value & READ_REG(REG_IF));
		break;
	case 0x204:
	{
		memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakRamWaitState[value & 3];

		if(!speedHack) {
			memoryWait[0x08] = memoryWait[0x09] = gamepakWaitState[(value >> 2) & 3];
			memoryWaitSeq[0x08] = memoryWaitSeq[0x09] =
					gamepakWaitState0[(value >> 4) & 1];

			memoryWait[0x0a] = memoryWait[0x0b] = gamepakWaitState[(value >> 5) & 3];
			memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] =
					gamepakWaitState1[(value >> 7) & 1];

			memoryWait[0x0c] = memoryWait[0x0d] = gamepakWaitState[(value >> 8) & 3];
			memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] =
					gamepakWaitState2[(value >> 10) & 1];
		} else {
			memoryWait[0x08] = memoryWait[0x09] = 3;
			memoryWaitSeq[0x08] = memoryWaitSeq[0x09] = 1;

			memoryWait[0x0a] = memoryWait[0x0b] = 3;
			memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] = 1;

			memoryWait[0x0c] = memoryWait[0x0d] = 3;
			memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] = 1;
		}

		for(int i = 8; i < 15; i++) {
			memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
			memoryWaitSeq32[i] = memoryWaitSeq[i]*2 + 1;
		}

		if((value & 0x4000) == 0x4000) {
			bus.busPrefetchEnable = true;
			bus.busPrefetch       = false;
			bus.busPrefetchCount  = 0;
#ifndef OLD_DATATICKS_UPDATE
			dataticks_update_on_busPrefetch_disable();
#endif
		} else {
			bus.busPrefetchEnable = false;
			bus.busPrefetch       = false;
			bus.busPrefetchCount  = 0;
#ifndef OLD_DATATICKS_UPDATE
			dataticks_update_on_busPrefetch_disable();
#endif
		}
		UPDATE_REG(0x204, value & 0x7FFF);
	}
	break;
	case 0x208:
		UPDATE_REG(REG_IME, value & 1);
		if ((READ_REG(REG_IME) & 1) && (READ_REG(REG_IF) & READ_REG(REG_IE)) && armIrqEnable)
			cpuNextEvent = cpuTotalTicks;
		break;
	case 0x300:
		if(value != 0)
			value &= 0xFFFE;
		UPDATE_REG(0x300, value);
		break;
	default:
		UPDATE_REG(address&0x3FE, value);
		break;
	}
}
#endif

void applyTimer ()
{
	if (timerOnOffDelay & 1)
	{
		timer0ClockReload = TIMER_TICKS[timer0Value & 3];
		if(!timer0On && (timer0Value & 0x80)) {
			// reload the counter
			timer0Ticks = (0x10000 - timer0Reload) << timer0ClockReload;
			UPDATE_REG(REG_TM0D, timer0Reload);
		}
		timer0On = timer0Value & 0x80 ? true : false;
		UPDATE_REG(REG_TM0CNT, timer0Value & 0xC7);
		//    CPUUpdateTicks();
	}
	if (timerOnOffDelay & 2)
	{
		timer1ClockReload = TIMER_TICKS[timer1Value & 3];
		if(!timer1On && (timer1Value & 0x80)) {
			// reload the counter
			timer1Ticks = (0x10000 - timer1Reload) << timer1ClockReload;
			UPDATE_REG(0x104, timer1Reload);
		}
		timer1On = timer1Value & 0x80 ? true : false;
		UPDATE_REG(REG_TM1CNT, timer1Value & 0xC7);
	}
	if (timerOnOffDelay & 4)
	{
		timer2ClockReload = TIMER_TICKS[timer2Value & 3];
		if(!timer2On && (timer2Value & 0x80)) {
			// reload the counter
			timer2Ticks = (0x10000 - timer2Reload) << timer2ClockReload;
			UPDATE_REG(REG_TM2D, timer2Reload);
		}
		timer2On = timer2Value & 0x80 ? true : false;
		UPDATE_REG(REG_TM2CNT, timer2Value & 0xC7);
	}
	if (timerOnOffDelay & 8)
	{
		timer3ClockReload = TIMER_TICKS[timer3Value & 3];
		if(!timer3On && (timer3Value & 0x80)) {
			// reload the counter
			timer3Ticks = (0x10000 - timer3Reload) << timer3ClockReload;
			UPDATE_REG(REG_TM3D, timer3Reload);
		}
		timer3On = timer3Value & 0x80 ? true : false;
		UPDATE_REG(REG_TM3CNT, timer3Value & 0xC7);
	}
	cpuNextEvent = CPUUpdateTicks();
	timerOnOffDelay = 0;
}

static u8 cpuBitsSet[256];
static u8 cpuLowestBitSet[256];

#include "gba_arm_cpuexec.inl"

#include "gba_thumb_cpuexec.inl"

void CPUInit(const char *biosFileName, bool useBiosFile)
{
#ifdef WORDS_BIGENDIAN
	if(!cpuBiosSwapped) {
		for(unsigned int i = 0; i < sizeof(myROM)/4; i++) {
			WRITE32LE(&myROM[i], myROM[i]);
		}
		cpuBiosSwapped = true;
	}
#endif

	sigset_t set;

    sigemptyset( &set );
    g_sigact.sa_flags     = 0;
    g_sigact.sa_mask      = set;
    g_sigact.sa_sigaction = &sigseg_gbaHandler;

	sigaction( SIGSEGV, &g_sigact, NULL);


	gbaSaveType = 0;
	eepromInUse = 0;
	saveType = 0;

	useBios = false;
	if(useBiosFile)
	{
		int size = 0x4000;
		if(utilLoad(biosFileName,
				CPUIsGBABios,
				bios,
				size))
		{
			if(size == 0x4000)
				useBios = true;
			else
				systemMessage(MSG_INVALID_BIOS_FILE_SIZE, N_("Invalid BIOS file size"));
		}
	}  // useBiosFile

	if(!useBios) {
		memcpy(bios, myROM, sizeof(myROM));
	}


	int i = 0;

	biosProtected[0] = 0x00;
	biosProtected[1] = 0xf0;
	biosProtected[2] = 0x29;
	biosProtected[3] = 0xe1;

	for(i = 0; i < 256; i++) {
		int count = 0;
		int j;
		for(j = 0; j < 8; j++)
			if(i & (1 << j))
				count++;
		cpuBitsSet[i] = count;

		for(j = 0; j < 8; j++)
			if(i & (1 << j))
				break;
		cpuLowestBitSet[i] = j;
	}

	for(i = 0; i < 0x400; i++)
		ioReadable[i] = true;
	for(i = 0x10; i < 0x48; i++)
		ioReadable[i] = false;
	for(i = 0x4c; i < 0x50; i++)
		ioReadable[i] = false;
	for(i = 0x54; i < 0x60; i++)
		ioReadable[i] = false;
	for(i = 0x8c; i < 0x90; i++)
		ioReadable[i] = false;
	for(i = 0xa0; i < 0xb8; i++)
		ioReadable[i] = false;
	for(i = 0xbc; i < 0xc4; i++)
		ioReadable[i] = false;
	for(i = 0xc8; i < 0xd0; i++)
		ioReadable[i] = false;
	for(i = 0xd4; i < 0xdc; i++)
		ioReadable[i] = false;
	for(i = 0xe0; i < 0x100; i++)
		ioReadable[i] = false;
	for(i = 0x110; i < 0x120; i++)
		ioReadable[i] = false;
	for(i = 0x12c; i < 0x130; i++)
		ioReadable[i] = false;
	for(i = 0x138; i < 0x140; i++)
		ioReadable[i] = false;
	for(i = 0x144; i < 0x150; i++)
		ioReadable[i] = false;
	for(i = 0x15c; i < 0x200; i++)
		ioReadable[i] = false;
	for(i = 0x20c; i < 0x300; i++)
		ioReadable[i] = false;
	for(i = 0x304; i < 0x400; i++)
		ioReadable[i] = false;

	if(romSize < 0x1fe2000) {
		*((u16 *)&rom[0x1fe209c]) = 0xdffa; // SWI 0xFA
		*((u16 *)&rom[0x1fe209e]) = 0x4770; // BX LR
	} else {
		agbPrintEnable(false);
	}

#ifdef __GBAREG_OPTIMIZE__
	for (int i=0; i<1024; i++)       gbaRegUpdateFunc[i] = cpuUpdateRegister;

	gbaRegUpdateFunc[0x00]  = cpuUpdReg00;
	gbaRegUpdateFunc[0x04]  = cpuUpdReg04;
	gbaRegUpdateFunc[0x06]  = cpuUpdRegNone;

	for (int i=0x08; i<=0x26; i++)   gbaRegUpdateFunc[i] = cpuUpdRegGeneral;

	gbaRegUpdateFunc[0x28]  = cpuUpdReg28_2A;
	gbaRegUpdateFunc[0x2A]  = cpuUpdReg28_2A;
	gbaRegUpdateFunc[0x2C]  = cpuUpdReg2C_2E;
	gbaRegUpdateFunc[0x2E]  = cpuUpdReg2C_2E;

	gbaRegUpdateFunc[0x30]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x32]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x34]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x36]  = cpuUpdRegGeneral;

	gbaRegUpdateFunc[0x38]  = cpuUpdReg38_3A;
	gbaRegUpdateFunc[0x3A]  = cpuUpdReg38_3A;
	gbaRegUpdateFunc[0x3C]  = cpuUpdReg3C_3E;
	gbaRegUpdateFunc[0x3E]  = cpuUpdReg3C_3E;

	gbaRegUpdateFunc[0x40]  = cpuUpdReg40;
	gbaRegUpdateFunc[0x42]  = cpuUpdReg42;

	gbaRegUpdateFunc[0x44]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x46]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x48]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x4A]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x4C]  = cpuUpdRegGeneral;

	gbaRegUpdateFunc[0x50]  = cpuUpdReg50;

	gbaRegUpdateFunc[0x52]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0x54]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xB0]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xB2]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xB4]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xB6]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xB8]  = cpuUpdRegB8;
	gbaRegUpdateFunc[0xBA]  = cpuUpdRegBA;

	gbaRegUpdateFunc[0xBC]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xBE]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xC0]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xC2]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xC4]  = cpuUpdRegC4;
	gbaRegUpdateFunc[0xC6]  = cpuUpdRegC6;

	gbaRegUpdateFunc[0xC8]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xCA]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xCC]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xCE]  = cpuUpdRegGeneral;

	gbaRegUpdateFunc[0xD0]  = cpuUpdRegD0;
	gbaRegUpdateFunc[0xD2]  = cpuUpdRegD2;

	gbaRegUpdateFunc[0xD4]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xD6]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xD8]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xDA]  = cpuUpdRegGeneral;
	gbaRegUpdateFunc[0xDC]  = cpuUpdRegDC;
	gbaRegUpdateFunc[0xDE]  = cpuUpdRegDE;

	gbaRegUpdateFunc[0x60]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x62]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x64]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x68]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x6c]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x70]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x72]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x74]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x78]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x7c]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x80]  = cpuUpdRegSnd8;
	gbaRegUpdateFunc[0x84]  = cpuUpdRegSnd8;

	gbaRegUpdateFunc[0x82]  = soundEvent_u16;
	gbaRegUpdateFunc[0x88]  = soundEvent_u16;
	gbaRegUpdateFunc[0xa0]  = soundEvent_u16;
	gbaRegUpdateFunc[0xa2]  = soundEvent_u16;
	gbaRegUpdateFunc[0xa4]  = soundEvent_u16;
	gbaRegUpdateFunc[0xa6]  = soundEvent_u16;
	gbaRegUpdateFunc[0x90]  = soundEvent_u16;
	gbaRegUpdateFunc[0x92]  = soundEvent_u16;
	gbaRegUpdateFunc[0x94]  = soundEvent_u16;
	gbaRegUpdateFunc[0x96]  = soundEvent_u16;
	gbaRegUpdateFunc[0x98]  = soundEvent_u16;
	gbaRegUpdateFunc[0x9a]  = soundEvent_u16;
	gbaRegUpdateFunc[0x9c]  = soundEvent_u16;
	gbaRegUpdateFunc[0x9e]  = soundEvent_u16;

	gbaRegUpdateFunc[0x100] = cpuUpdReg100;
	gbaRegUpdateFunc[0x102] = cpuUpdReg102;
	gbaRegUpdateFunc[0x104] = cpuUpdReg104;
	gbaRegUpdateFunc[0x106] = cpuUpdReg106;
	gbaRegUpdateFunc[0x108] = cpuUpdReg108;
	gbaRegUpdateFunc[0x10A] = cpuUpdReg10A;
	gbaRegUpdateFunc[0x10C] = cpuUpdReg10C;
	gbaRegUpdateFunc[0x10E] = cpuUpdReg10E;

	gbaRegUpdateFunc[COMM_SIOCNT]   = cpuUpdRegCOMM_SIOCNT;
	gbaRegUpdateFunc[COMM_SIODATA8] = cpuUpdRegCOMM_SIODATA8;
	gbaRegUpdateFunc[COMM_RCNT]     = cpuUpdRegCOMM_RCNT;

	gbaRegUpdateFunc[0x130] = cpuUpdReg130;
	gbaRegUpdateFunc[0x132] = cpuUpdReg132;

	gbaRegUpdateFunc[COMM_JOYCNT]      = cpuUpdRegCOMM_JOYCNT;
	gbaRegUpdateFunc[COMM_JOY_RECV_L]  = cpuUpdRegCOMM_JOY_RECV_L;
	gbaRegUpdateFunc[COMM_JOY_RECV_H]  = cpuUpdRegCOMM_JOY_RECV_H;
	gbaRegUpdateFunc[COMM_JOY_TRANS_L] = cpuUpdRegCOMM_JOY_TRANS_L;
	gbaRegUpdateFunc[COMM_JOY_TRANS_H] = cpuUpdRegCOMM_JOY_TRANS_H;
	gbaRegUpdateFunc[COMM_JOYSTAT]     = cpuUpdRegCOMM_JOYSTAT;

	gbaRegUpdateFunc[0x200] = cpuUpdReg200;
	gbaRegUpdateFunc[0x202] = cpuUpdReg202;
	gbaRegUpdateFunc[0x204] = cpuUpdReg204;
	gbaRegUpdateFunc[0x208] = cpuUpdReg208;
	gbaRegUpdateFunc[0x300] = cpuUpdReg300;
#endif

#ifdef _GBA_THREAD_RENDERING_
	if (!lineRender_tid)
	{
		pthread_create(&lineRender_tid, NULL, lineRender_thread, NULL);
		gbaMsgInit();
	}
#endif // _GBA_THREAD_RENDERING_

}

void CPUReset()
{

	if(gbaSaveType == 0) {
		if(eepromInUse)
			gbaSaveType = 3;
		else
			switch(saveType) {
			case 1:
				gbaSaveType = 1;
				break;
			case 2:
				gbaSaveType = 2;
				break;
			}
	}
	SLOG("RTC-RESET");
	rtcReset();

	// clean registers
	SLOG("REG-RESET");
	memset(&bus.reg[0], 0, sizeof(bus.reg));
	// clean OAM
	SLOG("OAM-RESET");
	memset(oam, 0, OAM_SIZE);
	// clean palette
	SLOG("PALETTE-RESET");
	memset(paletteRAM, 0, PALETTE_SIZE);
	// clean picture
	SLOG("PIX-RESET");
	memset(pix, 0, 4*160*240);
	// clean vram
	SLOG("VRAM-RESET");
	memset(vram, 0, VRAM_SIZE);
	// clean io memory
	SLOG("IOMEM-RESET");
	memset(ioMem, 0, IOMEM_SIZE);

	io_registers[REG_DISPCNT]  = DISPCNT_FB;
	io_registers[REG_DISPSTAT] = 0x0000;
	io_registers[REG_VCOUNT]   = (useBios && !skipBios) ? 0 :0x007E;
	io_registers[REG_BG0CNT]   = 0x0000;
	io_registers[REG_BG1CNT]   = 0x0000;
	io_registers[REG_BG2CNT]   = 0x0000;
	io_registers[REG_BG3CNT]   = 0x0000;
	io_registers[REG_BG0HOFS]  = 0x0000;
	io_registers[REG_BG0VOFS]  = 0x0000;
	io_registers[REG_BG1HOFS]  = 0x0000;
	io_registers[REG_BG1VOFS]  = 0x0000;
	io_registers[REG_BG2HOFS]  = 0x0000;
	io_registers[REG_BG2VOFS]  = 0x0000;
	io_registers[REG_BG3HOFS]  = 0x0000;
	io_registers[REG_BG3VOFS]  = 0x0000;
	io_registers[REG_BG2PA]    = 0x0100;
	io_registers[REG_BG2PB]    = 0x0000;
	io_registers[REG_BG2PC]    = 0x0000;
	io_registers[REG_BG2PD]    = 0x0100;
	io_registers[REG_BG2X_L]   = 0x0000;
	io_registers[REG_BG2X_H]   = 0x0000;
	io_registers[REG_BG2Y_L]   = 0x0000;
	io_registers[REG_BG2Y_H]   = 0x0000;
	io_registers[REG_BG3PA]    = 0x0100;
	io_registers[REG_BG3PB]    = 0x0000;
	io_registers[REG_BG3PC]    = 0x0000;
	io_registers[REG_BG3PD]    = 0x0100;
	io_registers[REG_BG3X_L]   = 0x0000;
	io_registers[REG_BG3X_H]   = 0x0000;
	io_registers[REG_BG3Y_L]   = 0x0000;
	io_registers[REG_BG3Y_H]   = 0x0000;
	io_registers[REG_WIN0H]    = 0x0000;
	io_registers[REG_WIN1H]    = 0x0000;
	io_registers[REG_WIN0V]    = 0x0000;
	io_registers[REG_WIN1V]    = 0x0000;
	io_registers[REG_WININ]    = 0x0000;
	io_registers[REG_WINOUT]   = 0x0000;
	io_registers[REG_MOSAIC]   = 0x0000;
	io_registers[REG_BLDMOD]   = 0x0000;
	io_registers[REG_COLEV]    = 0x0000;
	io_registers[REG_COLY]     = 0x0000;
	io_registers[REG_DM0SAD_L] = 0x0000;
	io_registers[REG_DM0SAD_H] = 0x0000;
	io_registers[REG_DM0DAD_L] = 0x0000;
	io_registers[REG_DM0DAD_H] = 0x0000;
	io_registers[REG_DM0CNT_L] = 0x0000;
	io_registers[REG_DM0CNT_H] = 0x0000;
	io_registers[REG_DM1SAD_L] = 0x0000;
	io_registers[REG_DM1SAD_H] = 0x0000;
	io_registers[REG_DM1DAD_L] = 0x0000;
	io_registers[REG_DM1DAD_H] = 0x0000;
	io_registers[REG_DM1CNT_L] = 0x0000;
	io_registers[REG_DM1CNT_H] = 0x0000;
	io_registers[REG_DM2SAD_L] = 0x0000;
	io_registers[REG_DM2SAD_H] = 0x0000;
	io_registers[REG_DM2DAD_L] = 0x0000;
	io_registers[REG_DM2DAD_H] = 0x0000;
	io_registers[REG_DM2CNT_L] = 0x0000;
	io_registers[REG_DM2CNT_H] = 0x0000;
	io_registers[REG_DM3SAD_L] = 0x0000;
	io_registers[REG_DM3SAD_H] = 0x0000;
	io_registers[REG_DM3DAD_L] = 0x0000;
	io_registers[REG_DM3DAD_H] = 0x0000;
	io_registers[REG_DM3CNT_L] = 0x0000;
	io_registers[REG_DM3CNT_H] = 0x0000;
	io_registers[REG_TM0D]     = 0x0000;
	io_registers[REG_TM0CNT]   = 0x0000;
	io_registers[REG_TM1D]     = 0x0000;
	io_registers[REG_TM1CNT]   = 0x0000;
	io_registers[REG_TM2D]     = 0x0000;
	io_registers[REG_TM2CNT]   = 0x0000;
	io_registers[REG_TM3D]     = 0x0000;
	io_registers[REG_TM3CNT]   = 0x0000;
	io_registers[REG_P1]       = 0x03FF;
	io_registers[REG_IE]       = 0x0000;
	io_registers[REG_IF]       = 0x0000;
	io_registers[REG_IME]      = 0x0000;

	armMode = 0x1F;

	if(cpuIsMultiBoot) {
		bus.reg[13].I = 0x03007F00;
		bus.reg[15].I = 0x02000000;
		bus.reg[16].I = 0x00000000;
		bus.reg[R13_IRQ].I = 0x03007FA0;
		bus.reg[R13_SVC].I = 0x03007FE0;
		armIrqEnable = true;
	} else {
		if(useBios && !skipBios) {
			bus.reg[15].I = 0x00000000;
			armMode = 0x13;
			armIrqEnable = false;
		} else {
			bus.reg[13].I = 0x03007F00;
			bus.reg[15].I = 0x08000000;
			bus.reg[16].I = 0x00000000;
			bus.reg[R13_IRQ].I = 0x03007FA0;
			bus.reg[R13_SVC].I = 0x03007FE0;
			armIrqEnable = true;
		}
	}
	armState = true;
	C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
	UPDATE_REG(REG_DISPCNT, DISPCNT_FB);
	UPDATE_REG(REG_VCOUNT,  (useBios && !skipBios) ? 0 :0x007E);
	UPDATE_REG(REG_BG2PA,   0x0100);
	UPDATE_REG(REG_BG2PD,   0x0100);
	UPDATE_REG(REG_BG3PA,   0x0100);
	UPDATE_REG(REG_BG3PD,   0x0100);
	UPDATE_REG(REG_P1,      0x03FF);
	UPDATE_REG(0x88,        0x200);

	// disable FIQ
	bus.reg[16].I |= 0x40;

	CPUUpdateCPSR();

	bus.armNextPC = bus.reg[15].I;
	bus.reg[15].I += 4;

	// reset internal state
	holdState = false;
	holdType = 0;

	biosProtected[0] = 0x00;
	biosProtected[1] = 0xf0;
	biosProtected[2] = 0x29;
	biosProtected[3] = 0xe1;

	lcdTicks = (useBios && !skipBios) ? 1008 : 208;
	timer0On = false;
	timer0Ticks = 0;
	timer0Reload = 0;
	timer0ClockReload  = 0;
	timer1On = false;
	timer1Ticks = 0;
	timer1Reload = 0;
	timer1ClockReload  = 0;
	timer2On = false;
	timer2Ticks = 0;
	timer2Reload = 0;
	timer2ClockReload  = 0;
	timer3On = false;
	timer3Ticks = 0;
	timer3Reload = 0;
	timer3ClockReload  = 0;
	dma0Source = 0;
	dma0Dest = 0;
	dma1Source = 0;
	dma1Dest = 0;
	dma2Source = 0;
	dma2Dest = 0;
	dma3Source = 0;
	dma3Dest = 0;
	cpuSaveGameFunc = flashSaveDecide;
	renderLine = mode0RenderLine;
	fxOn = false;
	windowOn = false;
	gFrameCount = 0;
	saveType = 0;
	layerEnable = READ_REG(REG_DISPCNT) & layerSettings;

	SLOG("RENDERBUF-RESET");
	CPUUpdateRenderBuffers(true);

	for(int i = 0; i < 256; i++) {
		map[i].address = (u8 *)&dummyAddress;
		map[i].mask = 0;
	}

	map[0].address = bios;
	map[0].mask    = BIOS_SIZE-1;
	map[2].address = workRAM;
	map[2].mask    = WORKRAM_SIZE-1;
	map[3].address = internalRAM;
	map[3].mask    = INTRAM_SIZE-1;
	map[4].address = ioMem;
	map[4].mask = 0x3FF;
	map[5].address = paletteRAM;
	map[5].mask = 0x3FF;
	map[6].address = vram;
	map[6].mask = 0x1FFFF;
	map[7].address = oam;
	map[7].mask = 0x3FF;
	map[8].address = rom;
	map[8].mask = 0x1FFFFFF;
	map[9].address = rom;
	map[9].mask = 0x1FFFFFF;
	map[10].address = rom;
	map[10].mask = 0x1FFFFFF;
	map[12].address = rom;
	map[12].mask = 0x1FFFFFF;
	map[14].address = flashSaveMemory;
	map[14].mask = 0xFFFF;

	SLOG("EEPROM-RESET");
	eepromReset();
	SLOG("FLASH-RESET");
	flashReset();

	SLOG("SOUND-RESET");
	soundReset();

	SLOG("WIN0-RESET");
	CPUUpdateWindow0();
	SLOG("WIN1-RESET");
	CPUUpdateWindow1();

	// make sure registers are correctly initialized if not using BIOS
	if(!useBios) {
		if(cpuIsMultiBoot)
			BIOS_RegisterRamReset(0xfe);
		else
			BIOS_RegisterRamReset(0xff);
	} else {
		if(cpuIsMultiBoot)
			BIOS_RegisterRamReset(0xfe);
	}

	switch(cpuSaveType) {
	case 0: // automatic
		cpuSramEnabled = true;
		cpuFlashEnabled = true;
		cpuEEPROMEnabled = true;
		cpuEEPROMSensorEnabled = false;
		saveType = gbaSaveType = 0;
		break;
	case 1: // EEPROM
		cpuSramEnabled = false;
		cpuFlashEnabled = false;
		cpuEEPROMEnabled = true;
		cpuEEPROMSensorEnabled = false;
		saveType = gbaSaveType = 3;
		// EEPROM usage is automatically detected
		break;
	case 2: // SRAM
		cpuSramEnabled = true;
		cpuFlashEnabled = false;
		cpuEEPROMEnabled = false;
		cpuEEPROMSensorEnabled = false;
		cpuSaveGameFunc = sramDelayedWrite; // to insure we detect the write
		saveType = gbaSaveType = 1;
		break;
	case 3: // FLASH
		cpuSramEnabled = false;
		cpuFlashEnabled = true;
		cpuEEPROMEnabled = false;
		cpuEEPROMSensorEnabled = false;
		cpuSaveGameFunc = flashDelayedWrite; // to insure we detect the write
		saveType = gbaSaveType = 2;
		break;
	case 4: // EEPROM+Sensor
		cpuSramEnabled = false;
		cpuFlashEnabled = false;
		cpuEEPROMEnabled = true;
		cpuEEPROMSensorEnabled = true;
		// EEPROM usage is automatically detected
		saveType = gbaSaveType = 3;
		break;
	case 5: // NONE
		cpuSramEnabled = false;
		cpuFlashEnabled = false;
		cpuEEPROMEnabled = false;
		cpuEEPROMSensorEnabled = false;
		// no save at all
		saveType = gbaSaveType = 5;
		break;
	}

	ARM_PREFETCH;

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	cpuDmaHack = false;

	lastTime = systemGetClock();

#ifdef USE_SWITICKS
	SWITicks = 0;
#endif
#if 0
	{
		int count = sizeof(myROM);

		for (int i=0; i<count; i+=4)
		{
			disArm(i, disMsg, DIS_VIEW_ADDRESS | DIS_VIEW_CODE);
			SLOG("|BIOS| %s", disMsg);
		}
	}
#endif
}

void CPUInterrupt()
{
	u32 PC = bus.reg[15].I;
	bool savedState = armState;
	CPUSwitchMode(0x12, true, false);
	bus.reg[14].I = PC;
	if(!savedState)
		bus.reg[14].I += 2;
	bus.reg[15].I = 0x18 + BIOS_VIRTUAL_BASE;
	armState = true;
	armIrqEnable = false;

	bus.armNextPC = bus.reg[15].I;
	bus.reg[15].I += 4;
	ARM_PREFETCH;

	//  if(!holdState)
	biosProtected[0] = 0x02;
	biosProtected[1] = 0xc0;
	biosProtected[2] = 0x5e;
	biosProtected[3] = 0xe5;
}

void CPULoop(int ticks)
{
	int clockTicks;
	int timerOverflow = 0;
	// variable used by the CPU core
	cpuTotalTicks = 0;

	// shuffle2: what's the purpose?
	if(gba_link_enabled)
		cpuNextEvent = 1;

	cpuBreakLoop = false;
	cpuNextEvent = CPUUpdateTicks();
	if(cpuNextEvent > ticks)
		cpuNextEvent = ticks;


	for(;;) {
		if(!holdState)
		{
			if(armState) {
				if (!armExecute())
					return;
			} else {
				if (!thumbExecute())
					return;
			}
			clockTicks = 0;
		} else
		{
			clockTicks = CPUUpdateTicks();
		}

		cpuTotalTicks += clockTicks;

		if(cpuTotalTicks >= cpuNextEvent)
		{
			int remainingTicks = cpuTotalTicks - cpuNextEvent;

#ifdef USE_SWITICKS
			if (SWITicks)
			{
				SWITicks-=clockTicks;
				if (SWITicks<0)
					SWITicks = 0;
			}
#endif
			clockTicks = cpuNextEvent;
			cpuTotalTicks = 0;
			cpuDmaHack = false;

updateLoop:

			if (IRQTicks)
			{
				IRQTicks -= clockTicks;
				if (IRQTicks<0)
					IRQTicks = 0;
			}

			lcdTicks -= clockTicks;


			if(lcdTicks <= 0) {
				if(READ_REG(REG_DISPSTAT) & DSTAT_IN_VBL) { // V-BLANK
					// if in V-Blank mode, keep computing...
					if(READ_REG(REG_DISPSTAT) & DSTAT_IN_HBL) {
						lcdTicks += 1008;
						READ_REG(REG_VCOUNT) += 1;
						READ_REG(REG_DISPSTAT) &= 0xFFFD;
						CPUCompareVCOUNT();
					} else {
						lcdTicks += 224;
						READ_REG(REG_DISPSTAT) |= DSTAT_IN_HBL;
						if(READ_REG(REG_DISPSTAT) & DSTAT_HBL_IRQ) {
							READ_REG(REG_IF) |= 2;
						}
					}

					if(READ_REG(REG_VCOUNT) >= 228) { //Reaching last line
						READ_REG(REG_DISPSTAT) &= 0xFFFC;
						READ_REG(REG_VCOUNT) = 0;
						CPUCompareVCOUNT();
					}
				} else {
					int framesToSkip = 1;

					if(speedup)
						framesToSkip = 9;
#if 0
					else if (systemRenderFps < 55)
						framesToSkip = 2;
					else if (systemRenderFps < 50)
						framesToSkip = 3;
#endif
					if(READ_REG(REG_DISPSTAT) & DSTAT_IN_HBL) {
						// if in H-Blank, leave it and move to drawing mode
						READ_REG(REG_VCOUNT) += 1;

						lcdTicks += 1008;
						READ_REG(REG_DISPSTAT) &= 0xFFFD;
						if(READ_REG(REG_VCOUNT) == 160) {
							count++;
							systemFrame();

							if((count % 10) == 0) {
								system10Frames(60);
							}
#if 0
							if(count == 60) {
								u32 time = systemGetClock();
								if(time != lastTime) {
									u32 t = 100000/(time - lastTime);
									systemShowSpeed(t);
								} else
									systemShowSpeed(0);
								lastTime = time;
								count = 0;
							}
#endif
							u32 joy = 0;

							// update joystick information
							if(systemReadJoypads())
								// read default joystick
								joy = systemReadJoypad(-1);

							cheatJoyData = joy;

							READ_REG(REG_P1) = 0x03FF ^ (joy & 0x3FF);
							if(cpuEEPROMSensorEnabled)
								systemUpdateMotionSensor();
							u16 P1CNT = READ_REG(REG_P1CNT);

							// this seems wrong, but there are cases where the game
							// can enter the stop state without requesting an IRQ from
							// the joypad.
							if((P1CNT & 0x4000) || stopState) {
								u16 p1 = (0x3FF ^ READ_REG(REG_P1)) & 0x3FF;
								if(P1CNT & 0x8000) {
									if(p1 == (P1CNT & 0x3FF)) {
										READ_REG(REG_IF) |= 0x1000;
									}
								} else {
									if(p1 & P1CNT) {
										READ_REG(REG_IF) |= 0x1000;
									}
								}
							}

							// if no (m) code is enabled, apply the cheats at each LCDline
#if 0
							u32 ext = (joy >> 10);
							if((cheatsEnabled) && (mastercode==0))
								remainingTicks += cheatsCheckKeys(READ_REG(REG_P1)^0x3FF, ext);
							speedup = (ext & 1) ? true : false;
							capture = (ext & 2) ? true : false;

							if(capture && !capturePrevious) {
								captureNumber++;
								systemScreenCapture(captureNumber);
							}
							capturePrevious = capture;
#endif
							READ_REG(REG_DISPSTAT) |= DSTAT_IN_VBL;
							READ_REG(REG_DISPSTAT) &= 0xFFFD;
							if(READ_REG(REG_DISPSTAT) & DSTAT_VBL_IRQ) {
								READ_REG(REG_IF) |= 1;
							}
							CPUCheckDMA(1, 0x0f);

							if (speedup)
							{
								if(gFrameCount >= framesToSkip) {
									gFrameCount = 0;
									systemDrawScreen();
								} else
									gFrameCount++;
							}
							else
							{
								systemDrawScreen();

								if(gFrameCount >= framesToSkip) {
									gFrameCount = 0;
								} else
									gFrameCount++;
							}

							if(systemPauseOnFrame())
								ticks = 0;
						} //if(READ_REG(REG_VCOUNT) == 160)

						CPUCompareVCOUNT();

					} else {
						if(gFrameCount >= framesToSkip)
						{
							bool bDraw_objwin  = (layerEnable & 0x9000) == 0x9000;
							bool bDraw_sprites = layerEnable & 0x1000;
							memset(line[4], -1, 240 * sizeof(u32));	// erase all sprites

							if(bDraw_sprites)
								gfxDrawSprites();

							if(bRenderLineAllEnabled)
							{
								memset(line[5], -1, 240 * sizeof(u32));	// erase all OBJ Win
								if(bDraw_objwin)
									gfxDrawOBJWin();
							}

#ifdef GBA_USE_RGBA8888
							if (systemColorDepth == 32)
							{
								u32 *dest = (u32 *)pix + 240 * (READ_REG(REG_VCOUNT));
								(*renderLine)(dest);
							}
#else
							// only support 16bit output surface
							if (systemColorDepth == 16)
							{
								u16 *dest = (u16 *)pix + 240 * (READ_REG(REG_VCOUNT));
								(*renderLine)(dest);
							}
#endif
						}

						// entering H-Blank
						READ_REG(REG_DISPSTAT) |= DSTAT_IN_HBL;
						lcdTicks += 224;
						CPUCheckDMA(2, 0x0f);

						if(READ_REG(REG_DISPSTAT) & DSTAT_HBL_IRQ) {
							READ_REG(REG_IF) |= 2;
						}
					}
				}
			}

			// we shouldn't be doing sound in stop state, but we loose synchronization
			// if sound is disabled, so in stop state, soundTick will just produce
			// mute sound
			soundTicks -= clockTicks;
			if(soundTicks <= 0) {
				process_sound_tick_fn();
				soundTicks += SOUND_CLOCK_TICKS;
			}

			if(!stopState) {
				if(timer0On) {
					timer0Ticks -= clockTicks;
					if(timer0Ticks <= 0) {
						timer0Ticks += (0x10000 - timer0Reload) << timer0ClockReload;
						timerOverflow |= 1;
						soundTimerOverflow(0);
						if(READ_REG(REG_TM0CNT) & 0x40) {
							READ_REG(REG_IF) |= 0x08;
						}
					}
					UPDATE_REG(0x100, (0xFFFF - (timer0Ticks >> timer0ClockReload)));
				}

				if(timer1On) {
					u16 tm1cnt = READ_REG(REG_TM1CNT);
					if(tm1cnt & 4) {
						if(timerOverflow & 1) {
							u16 tm1d = READ_REG(REG_TM1D) + 1;
							if(tm1d == 0) {
								tm1d += timer1Reload;
								timerOverflow |= 2;
								soundTimerOverflow(1);
								if(tm1cnt & 0x40) {
									READ_REG(REG_IF) |= 0x10;
								}
							}
							UPDATE_REG(REG_TM1D, tm1d);
						}
					} else {
						timer1Ticks -= clockTicks;
						if(timer1Ticks <= 0) {
							timer1Ticks += (0x10000 - timer1Reload) << timer1ClockReload;
							timerOverflow |= 2;
							soundTimerOverflow(1);
							if(tm1cnt & 0x40) {
								READ_REG(REG_IF) |= 0x10;
							}
						}
						UPDATE_REG(REG_TM1D, 0xFFFF - (timer1Ticks >> timer1ClockReload));
					}
				}

				if(timer2On) {
					u16 tm2cnt = READ_REG(REG_TM2CNT);
					if(tm2cnt & 4) {
						if(timerOverflow & 2) {
							u16 tm2d = READ_REG(REG_TM2D) + 1;
							if(tm2d == 0) {
								tm2d += timer2Reload;
								timerOverflow |= 4;
								if(tm2cnt & 0x40) {
									READ_REG(REG_IF) |= 0x20;
								}
							}
							UPDATE_REG(REG_TM2D, tm2d);
						}
					} else {
						timer2Ticks -= clockTicks;
						if(timer2Ticks <= 0) {
							timer2Ticks += (0x10000 - timer2Reload) << timer2ClockReload;
							timerOverflow |= 4;
							if(tm2cnt & 0x40) {
								READ_REG(REG_IF) |= 0x20;
							}
						}
						UPDATE_REG(REG_TM2D, 0xFFFF - (timer2Ticks >> timer2ClockReload));
					}
				}

				if(timer3On) {
					u16 tm3cnt = READ_REG(REG_TM3CNT);
					if(tm3cnt & 4) {
						if(timerOverflow & 4) {
							u16 tm3d = READ_REG(REG_TM3D) + 1;
							if(tm3d == 0) {
								tm3d += timer3Reload;
								if(tm3cnt & 0x40) {
									READ_REG(REG_IF) |= 0x40;
								}
							}
							UPDATE_REG(REG_TM3D, tm3d);
						}
					} else {
						timer3Ticks -= clockTicks;
						if(timer3Ticks <= 0) {
							timer3Ticks += (0x10000 - timer3Reload) << timer3ClockReload;
							if(tm3cnt & 0x40) {
								READ_REG(REG_IF) |= 0x40;
							}
						}
						UPDATE_REG(REG_TM3D, 0xFFFF - (timer3Ticks >> timer3ClockReload));
					}
				}
			}

			timerOverflow = 0;



#ifdef PROFILING
			profilingTicks -= clockTicks;
			if(profilingTicks <= 0) {
				profilingTicks += profilingTicksReload;
				if(profilSegment) {
					profile_segment *seg = profilSegment;
					do {
						u16 *b = (u16 *)seg->sbuf;
						int pc = ((bus.reg[15].I - seg->s_lowpc) * seg->s_scale)/0x10000;
						if(pc >= 0 && pc < seg->ssiz) {
							b[pc]++;
							break;
						}

						seg = seg->next;
					} while(seg);
				}
			}
#endif

			ticks -= clockTicks;

			if (gba_joybus_enabled)
				JoyBusUpdate(clockTicks);

//			if (gba_link_enabled)
//				LinkUpdate(clockTicks);

			cpuNextEvent = CPUUpdateTicks();

			if(cpuDmaTicksToUpdate > 0) {
				if(cpuDmaTicksToUpdate > cpuNextEvent)
					clockTicks = cpuNextEvent;
				else
					clockTicks = cpuDmaTicksToUpdate;
				cpuDmaTicksToUpdate -= clockTicks;
				if(cpuDmaTicksToUpdate < 0)
					cpuDmaTicksToUpdate = 0;
				cpuDmaHack = true;
				goto updateLoop;
			}

			// shuffle2: what's the purpose?
			if(gba_link_enabled)
				cpuNextEvent = 1;

			if(READ_REG(REG_IF) && (READ_REG(REG_IME) & 1) && armIrqEnable) {
				int res = READ_REG(REG_IF) & READ_REG(REG_IE);
				if(stopState)
					res &= 0x3080;
				if(res) {
					if (intState)
					{
						if (!IRQTicks)
						{
							intState  = false;
							holdState = false;
							stopState = false;
							holdType  = 0;
							CPUInterrupt();
						}
					}
					else
					{
						if (!holdState)
						{
							intState = true;
							IRQTicks=7;
							if (cpuNextEvent> IRQTicks)
								cpuNextEvent = IRQTicks;
						}
						else
						{
							holdState = false;
							stopState = false;
							holdType  = 0;
							CPUInterrupt();
						}
					}

#ifdef USE_SWITICKS
					// Stops the SWI Ticks emulation if an IRQ is executed
					//(to avoid problems with nested IRQ/SWI)
					if (SWITicks)
						SWITicks = 0;
#endif
				}
			}

			if(remainingTicks > 0) {
				if(remainingTicks > cpuNextEvent)
					clockTicks = cpuNextEvent;
				else
					clockTicks = remainingTicks;
				remainingTicks -= clockTicks;
				if(remainingTicks < 0)
					remainingTicks = 0;
				goto updateLoop;
			}

			if (timerOnOffDelay)
				applyTimer();

			if(cpuNextEvent > ticks)
				cpuNextEvent = ticks;

			if(ticks <= 0 || cpuBreakLoop)
				break;

		} // if (cpuTotalTicks >= cpuEventsTicks)
	}
}



struct EmulatedSystem GBASystem = {
		// emuMain
		CPULoop,
		// emuReset
		CPUReset,
		// emuCleanUp
		CPUCleanUp,
		// emuReadBattery
		CPUReadBatteryFile,
		// emuWriteBattery
		CPUWriteBatteryFile,
		// emuReadState
		CPUReadState,
		// emuWriteState
		CPUWriteState,
		// emuReadMemState
		CPUReadMemState,
		// emuWriteMemState
		CPUWriteMemState,
		// emuWritePNG
		CPUWritePNGFile,
		// emuWriteBMP
		CPUWriteBMPFile,
		// emuUpdateCPSR
		CPUUpdateCPSR,
		// emuHasDebugger
		true,
		// emuCount
#ifdef FINAL_VERSION
		250000
#else
		5000
#endif
};


