// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef __AUTOBUILD_H__
#define __AUTOBUILD_H__
#include "version.h"
//change the FALSE to TRUE for autoincrement of build number
#define INCREMENT_VERSION FALSE
#define FILEVER        1,8,0,600
#define PRODUCTVER     1,8,0,600
#define STRFILEVER     "1, 8, 0, 600\0"
#define STRPRODUCTVER  "1, 8, 0, 600\0"
#define SYSCONFDIR     "/accounts/1000/shared/misc/gbaemu"
#define SYSROMDIR      "/accounts/1000/shared/misc/roms/gba/"
#define SDCARDROMDIR   "/accounts/1000/removable/sdcard/misc/roms/gba/"

#ifdef STL100_1
#define GBA_VERSION        "1.2.1.3"
#else
#define GBA_VERSION        "1.2.0.3"
#endif

#endif //__AUTOBUILD_H__
