#ifndef GBACPU_H
#define GBACPU_H

extern int armExecute();
extern int thumbExecute();

#ifdef __GNUC__
#if !defined(__APPLE__) || !defined(__PLAYBOOK__)
//#ifndef __APPLE__
# define INSN_REGPARM __attribute__((regparm(1)))
#else
# define INSN_REGPARM /*nothing*/
#endif
# define LIKELY(x) __builtin_expect(!!(x),1)
# define UNLIKELY(x) __builtin_expect(!!(x),0)
#else
# define INSN_REGPARM /*nothing*/
# define LIKELY(x) (x)
# define UNLIKELY(x) (x)
#endif

#define UPDATE_REG(address, value)\
  {\
    WRITE16LE(((u16 *)&ioMem[address]),value);\
  }\

#if 0
#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);\
  }
#else
#define ARM_PREFETCH \
  {\
	register int  page = armNextPC >> 24;\
	register u32 *addr = (u32 *)&map[page].address[(armNextPC) & map[page].mask];\
    cpuPrefetch[0] = *addr++;\
    cpuPrefetch[1] = *addr;\
  }
#endif

#if 0
#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);\
  }
#else
#define THUMB_PREFETCH \
  {\
	register int  page = armNextPC >> 24;\
	register u16 *addr = (u16 *)&map[page].address[(armNextPC) & map[page].mask];\
    cpuPrefetch[0] = *addr++;\
    cpuPrefetch[1] = *addr;\
  }
#endif

#define ARM_PREFETCH_NEXT \
  cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);

#define THUMB_PREFETCH_NEXT\
  cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);


extern int SWITicks;
extern u32 mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern u32 busPrefetchCount;
extern int cpuNextEvent;
extern bool holdState;
extern u32 cpuPrefetch[2];
extern int cpuTotalTicks;
extern u8 memoryWait[16];
extern u8 memoryWait32[16];
extern u8 memoryWaitSeq[16];
extern u8 memoryWaitSeq32[16];
extern u8 cpuBitsSet[256];
extern u8 cpuLowestBitSet[256];
extern void CPUSwitchMode(int mode, bool saveState, bool breakLoop);
extern void CPUSwitchMode(int mode, bool saveState);
extern void CPUUpdateCPSR();
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUUpdateFlags();
extern void CPUUndefinedException();
extern void CPUSoftwareInterrupt();
extern void CPUSoftwareInterrupt(int comment);

#ifndef __GBAINLINE__

#define BUSPREFETCH_STATE  2
#define PAGESIZE           16

typedef int (*codeTicksAccess_t)(int page);
typedef int (*dataTicksAccess_t)(int page);

extern dataTicksAccess_t fdataAccess16[PAGESIZE];
extern dataTicksAccess_t fdataAccess32[PAGESIZE];
extern codeTicksAccess_t fcodeAccess16[PAGESIZE];
extern codeTicksAccess_t fcodeAccess32[PAGESIZE];

int dataTicksAccess32(u32 address);
int dataTicksAccessSeq16(u32 address);
int dataTicksAccessSeq32(u32 address);
int codeTicksAccessSeq16(u32 address);
int codeTicksAccessSeq32(u32 address);
void cpuMasterCodeCheck();

inline int dataTicksAccess16(u32 address)
{
	int page = (address>>24) & 0xF;
	return fdataAccess16[page](page);
}

inline int codeTicksAccess16(u32 address) // ARM NON SEQ
{
	int page = (address>>24) & 0xF;
	return fcodeAccess16[page](page);
}

inline int codeTicksAccess32(u32 address) // ARM NON SEQ
{
	int page = (address>>24) & 0xF;
	return fcodeAccess32[page](page);
}


#else
// Waitstates when accessing data
inline int dataTicksAccess16(u32 address) // DATA 8/16bits NON SEQ
{
  int addr = (address>>24)&15;
  int value =  memoryWait[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    if (!waitState)
      waitState = 1;
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}

inline int dataTicksAccess32(u32 address) // DATA 32bits NON SEQ
{
  int addr = (address>>24)&15;
  int value = memoryWait32[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    if (!waitState)
      waitState = 1;
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq16(u32 address)// DATA 8/16bits SEQ
{
  int addr = (address>>24)&15;
  int value = memoryWaitSeq[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    if (!waitState)
      waitState = 1;
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq32(u32 address)// DATA 32bits SEQ
{
  int addr = (address>>24)&15;
  int value =  memoryWaitSeq32[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    if (!waitState)
      waitState = 1;
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}


// Waitstates when executing opcode
inline int codeTicksAccess16(u32 address) // THUMB NON SEQ
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

inline int codeTicksAccess32(u32 address) // ARM NON SEQ
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

inline int codeTicksAccessSeq16(u32 address) // THUMB SEQ
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

inline int codeTicksAccessSeq32(u32 address) // ARM SEQ
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
inline void cpuMasterCodeCheck()
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
#endif // __GBAINLINE__

#endif // GBACPU_H
