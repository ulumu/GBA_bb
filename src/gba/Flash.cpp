#include <stdio.h>
#include <memory.h>
#include "GBA.h"
#include "Globals.h"
#include "Flash.h"
#include "Sram.h"
#include "../Util.h"

#define FLASH_READ_ARRAY         0
#define FLASH_CMD_1              1
#define FLASH_CMD_2              2
#define FLASH_AUTOSELECT         3
#define FLASH_CMD_3              4
#define FLASH_CMD_4              5
#define FLASH_CMD_5              6
#define FLASH_ERASE_COMPLETE     7
#define FLASH_PROGRAM            8
#define FLASH_SETBANK            9

u8 flashSaveMemory[FLASH_128K_SZ];
int flashState          = FLASH_READ_ARRAY;
int flashReadState      = FLASH_READ_ARRAY;
int flashSize           = 0x10000;
int flashDeviceID       = FLASH_DEVICE_MACRONIX_64KB;  //0x1b;
int flashManufacturerID = FLASH_MANUFACTURER_MACRONIX; //0x32;
int flashBank = 0;

static variable_desc flashSaveData[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSaveMemory[0], 0x10000 },
  { NULL, 0 }
};

static variable_desc flashSaveData2[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSize, sizeof(int) },
  { &flashSaveMemory[0], 0x20000 },
  { NULL, 0 }
};

static variable_desc flashSaveData3[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSize, sizeof(int) },
  { &flashBank, sizeof(int) },
  { &flashSaveMemory[0], 0x20000 },
  { NULL, 0 }
};

void flashInit()
{
  memset(flashSaveMemory, 0xff, sizeof(flashSaveMemory));

  /*
   * FlashSize setup can be coming from multiple area, and we should only
   * allow one time setup per game load.
   */
  flashSizeOverride = true;
}

void flashReset()
{
  flashState = FLASH_READ_ARRAY;
  flashReadState = FLASH_READ_ARRAY;
  flashBank = 0;
}

void flashSaveGame(gzFile gzFile)
{
  utilWriteData(gzFile, flashSaveData3);
}

void flashReadGame(gzFile gzFile, int version)
{
  if(version < SAVE_GAME_VERSION_5)
    utilReadData(gzFile, flashSaveData);
  else if(version < SAVE_GAME_VERSION_7) {
    utilReadData(gzFile, flashSaveData2);
    flashBank = 0;
    flashSetSize(flashSize);
  } else {
    utilReadData(gzFile, flashSaveData3);
  }
}

void flashReadGameSkip(gzFile gzFile, int version)
{
  // skip the flash data in a save game
  if(version < SAVE_GAME_VERSION_5)
    utilReadDataSkip(gzFile, flashSaveData);
  else if(version < SAVE_GAME_VERSION_7) {
    utilReadDataSkip(gzFile, flashSaveData2);
  } else {
    utilReadDataSkip(gzFile, flashSaveData3);
  }
}

void flashSetSize(int size)
{
    if (flashSizeOverride == false) {
        SLOG("Only allow to setting FlashSize only once!! SKIP!!");
        return;
    }

	if(size == 0x10000) {
		SLOG("64KB Flash size");
		flashDeviceID       = FLASH_DEVICE_MACRONIX_64KB;
		flashManufacturerID = FLASH_MANUFACTURER_MACRONIX;
	} else {
		SLOG("Other flash size: %d", size);
		flashDeviceID       = 0x13; //0x09;
		flashManufacturerID = 0x62; //0xc2;
	}
	// Added to make 64k saves compatible with 128k ones
	// (allow wrongfuly set 64k saves to work for Pokemon games)
	if ((size == 0x20000) && (flashSize == 0x10000))
		memcpy((u8 *)(flashSaveMemory+0x10000), (u8 *)(flashSaveMemory), 0x10000);
	flashSize = size;

	/*
	 * Now Flash size is set, no more updating is allowed, until a New Game is loaded
	 */
	flashSizeOverride = false;
}

u8 flashRead(u32 address)
{
	//  log("Reading %08x from %08x\n", address, reg[15].I);
	//  log("Current read state is %d\n", flashReadState);
	u8 value = 0;

	address &= 0xFFFF;

	switch(flashReadState) {
	case FLASH_READ_ARRAY:
		value = flashSaveMemory[(flashBank << 16) + address];
		break;
	case FLASH_AUTOSELECT:
		switch(address & 0xFF) {
		case 0:
			// manufacturer ID
			SLOG("Read flash Manufacturer ID");
			value= flashManufacturerID;
			break;
		case 1:
			// device ID
			SLOG("Read flash Device ID");
			value = flashDeviceID;
			break;
		}
		break;
	case FLASH_ERASE_COMPLETE:
		flashState = FLASH_READ_ARRAY;
		flashReadState = FLASH_READ_ARRAY;
		value = 0xFF;
		break;
	};

	return value;
}

void flashSaveDecide(u32 address, u8 byte)
{
	//  log("Deciding save type %08x\n", address);
	if(address == 0x0e005555) {
		saveType = 2;
		cpuSaveGameFunc = flashWrite;
	} else {
		saveType = 1;
		cpuSaveGameFunc = sramWrite;
	}

	(*cpuSaveGameFunc)(address, byte);
}

void flashDelayedWrite(u32 address, u8 byte)
{
	saveType = 2;
	cpuSaveGameFunc = flashWrite;
	flashWrite(address, byte);
}

void flashWrite(u32 address, u8 byte)
{
	//  log("Writing %02x at %08x\n", byte, address);
	//  log("Current state is %d\n", flashState);
	address &= 0xFFFF;
	switch(flashState) {
	case FLASH_READ_ARRAY:
		if(address == 0x5555 && byte == 0xAA)
			flashState = FLASH_CMD_1;
		break;
	case FLASH_CMD_1:
		if(address == 0x2AAA && byte == 0x55)
			flashState = FLASH_CMD_2;
		else
			flashState = FLASH_READ_ARRAY;
		break;
	case FLASH_CMD_2:
		if(address == 0x5555) {
			if(byte == 0x90) {
				flashState = FLASH_AUTOSELECT;
				flashReadState = FLASH_AUTOSELECT;
			} else if(byte == 0x80) {
				flashState = FLASH_CMD_3;
			} else if(byte == 0xF0) {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			} else if(byte == 0xA0) {
				flashState = FLASH_PROGRAM;
			} else if(byte == 0xB0 && flashSize == 0x20000) {
				flashState = FLASH_SETBANK;
			} else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
		} else {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		}
		break;
	case FLASH_CMD_3:
		if(address == 0x5555 && byte == 0xAA) {
			flashState = FLASH_CMD_4;
		} else {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		}
		break;
	case FLASH_CMD_4:
		if(address == 0x2AAA && byte == 0x55) {
			flashState = FLASH_CMD_5;
		} else {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		}
		break;
	case FLASH_CMD_5:
		if(byte == 0x30) {
			// SECTOR ERASE
			memset(&flashSaveMemory[(flashBank << 16) + (address & 0xF000)],
					0,
					0x1000);
			systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
			flashReadState = FLASH_ERASE_COMPLETE;
		} else if(byte == 0x10) {
			// CHIP ERASE
			memset(flashSaveMemory, 0, flashSize);
			systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
			flashReadState = FLASH_ERASE_COMPLETE;
		} else {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		}
		break;
	case FLASH_AUTOSELECT:
		if(byte == 0xF0) {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		} else if(address == 0x5555 && byte == 0xAA)
			flashState = FLASH_CMD_1;
		else {
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
		}
		break;
	case FLASH_PROGRAM:
		flashSaveMemory[(flashBank<<16)+address] = byte;
		systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
		flashState = FLASH_READ_ARRAY;
		flashReadState = FLASH_READ_ARRAY;
		break;
	case FLASH_SETBANK:
		if(address == 0) {
			flashBank = (byte & 1);
		}
		flashState = FLASH_READ_ARRAY;
		flashReadState = FLASH_READ_ARRAY;
		break;
	}
}
