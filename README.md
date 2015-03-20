VBAMpb BB10 Port of VBA-M GBA/GB/GBC emulator

misc\roms\gba        place unzipped .gba or .gb games in here (sdcard or phone)
misc\gbaemu          configuration, saves and other stuff.

v1.2.0.5

- Fix black screen during startup in OS 10.3.1 (dialog box not showing up)
- Fix Game saving issue in Golden Sun

v1.2.0.4

- Fix GB borderless bug. Now default BorderOn option in vbam.cfg is set to 0
- Fix crash in Summon Night Swordcraft Story 2, and other games that using the drawRotationScreen feature

v1.2.0.3

- New virtual memory model
- Optimized memory access based on new virtual memory model
- Optimized internal GBA Bios functions, eliminates external GBA BIOS file
- Cheat Support
- Screen resize
- Fast Forward
- Joypad button mapping through vbam.cfg

v1.0.5.4

- Bug fix directory read problem on SD card. Need to use readdir64 instead.

v1.0.5.3

- Bug fix optimization code
- Add Dropdown menu support when swipe down from Top of screen

v1.0.5.2

- Add Q10 support
- Remap keyboard keys, both Q10 and Z10 based on same keyboard mapping
- GBA arm instruction optimization using assembler
- GBA gfx BKG line drawing optimization
- Change frame skipping logic calculation

v1.0.4.4

- Bug fix SGB frame corruption
- Bug fix STL100-1 and dev-alpha device black screen problem
- Bug fix slow frame rate on some of the GBA games, especially pokemon
- Bug fix GB/GBC game loading problem if first game loading is GBC
- Bug fix GBA state loading/state saving problem if ROM is not first loading ROM
- Known issue: for dev-alpha device, openGl option must be 1, otherwise there is corruption

v1.0.4.3

- Add log to file option
- Add custom fragment shader code loading.

v1.0.4.2

- Add sd card ROM loading support
- Fix random Touchscreen Button disappering issue
- Improve Audio quality
- Add GBC and GB ROM support
- Add Touchscreen overlay transparency user control


v1.0.2.1

- Use OpenGL ES2.0 for rendering, adding shader smooth filtering from FXAA.
- Add Bluetooth Gamepad support (only Wiimote is tested)
- Support hiding Touch Overlay icon when Bluetooth Gamepad is used
- rom directory is renamed to misc/roms/gba, bios is also placed here
- save games and states is renamed to misc/gbaemu/savegames

v1.0.1.1

- Visual Rom Selector, tap top left
- Save files now work and load.
- Fixed L + R (was backwards)
- App now automatically creates and reads in vbam-over.ini
- App now automatially creates vbam.cfg
- Bios Dir was changed to misc/vbampb/bios
- Savegames and savestates save into misc/vbampb/savegames
- Gameboy Color functionality REMOVED -- Available in Emulator GBColor-pb


v1.0.0.5

- rom cycling, tap top left
- FEX is disabled for now, so no zip etc
- effects are disabled for now like blur etc but not the main filters 
- audio buffer is 300ms/11khz for now... 
- Some games stutter with sound, performance issues vary wildly but for
  the most part they are playable.
