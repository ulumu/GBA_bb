#include "GBA.h"

#ifdef BKPT_SUPPORT
int  oldreg[18];
char oldbuffer[10];
#endif

bus_t bus;
memoryMap map[256];
bool ioReadable[0x400];
bool N_FLAG = 0;
bool C_FLAG = 0;
bool Z_FLAG = 0;
bool V_FLAG = 0;
bool armState = true;
bool armIrqEnable = true;
//u32 armNextPC = 0x00000000;
int armMode = 0x1f;
u32 stop = 0x08000568;
int saveType = 0;
bool useBios = false;
bool skipBios = true;
int  frameSkip = 0;
bool speedup = false;
bool synchronize = true;
bool cpuDisableSfx = false;
bool cpuIsMultiBoot = false;
bool parseDebug = false;
int layerSettings = 0xff00;
int layerEnable = 0xff00;
bool speedHack = true;
int cpuSaveType = 0;
bool cheatsEnabled = false;
bool mirroringEnable = false;
bool skipSaveGameBattery = false;
bool skipSaveGameCheats = false;
bool flashSizeOverride = true;

// this is an optional hack to change the backdrop/background color:
// -1: disabled
// 0x0000 to 0x7FFF: set custom 15 bit color
int customBackdropColor = -1;

u8 *bios = 0;
u8 *rom = 0;
u8 *romMirror = 0;
u8 *internalRAM = 0;
u8 *workRAM = 0;
u8 *paletteRAM = 0;
u8 *vram = 0;
u8 *pix = 0;
u8 *oam = 0;
u8 *ioMem = 0;
u16 io_registers[1024];
