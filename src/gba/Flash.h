#ifndef FLASH_H
#define FLASH_H

#define FLASH_128K_SZ 0x20000

typedef enum
{
  FLASH_DEVICE_MACRONIX_64KB   = 0x1C,
  FLASH_DEVICE_AMTEL_64KB      = 0x3D,
  FLASH_DEVICE_SST_64K         = 0xD4,
  FLASH_DEVICE_PANASONIC_64KB  = 0x1B,
  FLASH_DEVICE_MACRONIX_128KB  = 0x09
} flash_device_id_type;

typedef enum
{
  FLASH_MANUFACTURER_MACRONIX  = 0xC2,
  FLASH_MANUFACTURER_AMTEL     = 0x1F,
  FLASH_MANUFACTURER_PANASONIC = 0x32,
  FLASH_MANUFACTURER_SST       = 0xBF
} flash_manufacturer_id_type;


extern void flashSaveGame(gzFile _gzFile);
extern void flashReadGame(gzFile _gzFile, int version);
extern void flashReadGameSkip(gzFile _gzFile, int version);
extern u8 flashRead(u32 address);
extern void flashWrite(u32 address, u8 byte);
extern void flashDelayedWrite(u32 address, u8 byte);
extern u8 flashSaveMemory[FLASH_128K_SZ];
extern void flashSaveDecide(u32 address, u8 byte);
extern void flashReset();
extern void flashSetSize(int size);
extern void flashInit();

extern int flashSize;

#endif // FLASH_H
