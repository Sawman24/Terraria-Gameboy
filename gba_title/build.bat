@echo off
set DEVKITARM=c:\devkitPro\devkitARM
set PATH=%DEVKITARM%\bin;%PATH%

echo === Building Terraria GBA Title Screen ===

echo [1/3] Compiling...
arm-none-eabi-gcc -mthumb -mthumb-interwork -mcpu=arm7tdmi -O2 -c main.c -o main.o
if errorlevel 1 goto fail

echo [2/3] Linking...
arm-none-eabi-gcc -mthumb -mthumb-interwork -specs=gba.specs main.o -o terraria_title.elf
if errorlevel 1 goto fail

echo [3/3] Creating ROM...
arm-none-eabi-objcopy -O binary terraria_title.elf terraria_title.gba
if errorlevel 1 goto fail

REM Fix GBA header
c:\devkitPro\tools\bin\gbafix.exe terraria_title.gba -tTERRARIA -cTRRA -mSW
if errorlevel 1 echo Warning: gbafix not found, ROM may not have valid header

echo.
echo === SUCCESS! ===
echo ROM: terraria_title.gba
echo Load it in mGBA, VisualBoyAdvance, or any GBA emulator!
goto end

:fail
echo.
echo === BUILD FAILED ===
:end
