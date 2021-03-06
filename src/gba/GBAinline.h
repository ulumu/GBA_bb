#ifndef GBAINLINE_H
#define GBAINLINE_H

#include "../System.h"
#include "../common/Port.h"
#include "RTC.h"
#include "Sound.h"
#include "agbprint.h"
#include "GBAcpu.h"
#include "GBALink.h"
#include "GBAMem.h"

extern const u32 objTilesAddress[3];

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool cpuDmaHack;
extern u32 cpuDmaLast;
extern bool timer0On;
extern int timer0Ticks;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Ticks;
extern int timer1ClockReload;
extern bool timer2On;
extern int timer2Ticks;
extern int timer2ClockReload;
extern bool timer3On;
extern int timer3Ticks;
extern int timer3ClockReload;
extern int cpuTotalTicks;

#define CPUReadByteQuick(addr)		map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]
#define CPUReadHalfWordQuick(addr)	READ16LE(((u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))
#define CPUReadMemoryQuick(addr)	READ32LE(((u32*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

extern u32 myROM[];

#if defined(__GNUC__)
# if 0 //defined(__ARM__)
#  define PREFETCH_FIRST(_addr)  \
    __asm__ __volatile__ (              \
        "   pld [%0]        \n\t"       \
        "   pld [%0, #32]   \n\t"       \
        "   pld [%0, #64]   \n\t"       \
        "   pld [%0, #96]   \n\t"       \
        :                               \
        : "r" (_addr));
#  define PREFETCH_NEXT(_addr)   \
    __asm__ __volatile__ (              \
        "   pld [%0, #128]  \n\t"       \
        :                               \
        : "r" (_addr));
# else
#  define PREFETCH_FIRST(_addr)
#  define PREFETCH_NEXT(_addr)
# endif
#endif

//# include <arm_neon.h>

#endif // GBAINLINE_H
