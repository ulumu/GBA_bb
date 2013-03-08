VBAMpb BB10 Port of VBA-M GBA/GB/GBC emulator

misc\roms\gba        place unzipped .gba games in here
                     place gba_bios.bin file here if you wish to play games...
misc\gbaemu          configuration, saves and other stuff.

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
