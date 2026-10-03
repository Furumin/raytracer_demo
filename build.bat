@echo off

if "%1" == "dev" (
   echo Dev mode, no optimizations enabled.
   set "flags=-g -O0 -DFLAG_GCC_DEBUG -Wall"   
) else (
   set "flags=-O3 -g -mfma"
   echo Release mode, Heavy optimizations.

)
set "extraflags="
if "%2" == "spectral" (
set "extraflags=-DTRACER_SPECTRAL_MODE"
)
REM set these variables to whatever matches your machine's configuration
set vulkanlibpath=lib 

set supresslinkerwarning=-Wl,--disable-stdcall-fixup -Wl,--enable-stdcall-fixup
echo Compiling...

g++ %flags% %extraflags% %sanitizecompileflags% -march=native -std=c++17 -c src/main.cpp -finput-charset=UTF-8 -mwindows -o out/app.o %tracypathinclude%
echo Compiled. Now linking...
g++ out/app.o out/bvh.o -mwindows -o app.exe -L%vulkanlibpath% -lvulkan-1 -lWinmm -lkernel32 %sanitizelinkflags%
echo Done.

