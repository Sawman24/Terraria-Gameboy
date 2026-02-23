@echo off
set DEVKITPRO=c:\devkitPro
set DEVKITARM=c:\devkitPro\devkitARM
set PATH=%DEVKITARM%\bin;%PATH%

echo === Building Terraria GBA Core (Combined) ===

echo [1/3] Compiling...
arm-none-eabi-gcc -mthumb -mthumb-interwork -mcpu=arm7tdmi -O2 -c world_gen.c -o world_gen.o
if errorlevel 1 goto fail
arm-none-eabi-gcc -mthumb -mthumb-interwork -mcpu=arm7tdmi -O2 -c main.c -o main.o
if errorlevel 1 goto fail

echo [2/3] Linking...
arm-none-eabi-gcc -mthumb -mthumb-interwork -specs=gba.specs main.o world_gen.o -o terraria_game.elf
if errorlevel 1 goto fail

echo [3/3] Creating ROM...
arm-none-eabi-objcopy -O binary terraria_game.elf terraria_game_smartmine.gba
if errorlevel 1 goto fail

REM Fix GBA header
c:\devkitPro\tools\bin\gbafix.exe terraria_game_smartmine.gba -tTERRARIA -cTRRA -mSW
if errorlevel 1 echo Warning: gbafix not found, ROM may not have valid header

echo.
echo === SUCCESS! ===
echo Output: terraria_game_smartmine.gba
goto end

:fail
echo.
echo === BUILD FAILED ===
:end
