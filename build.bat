@echo off

if "%1" == "dev" (
   echo Dev mode, no optimizations enabled.
   set "flags=-g -O0 -DFLAG_GCC_DEBUG -Wall"   
) else (
   set "flags=-O3 -g -Wall"
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
set "imguilink=out/imgui/imgui.o out/imgui/imgui_draw.o out/imgui/imgui_impl_vulkan.o out/imgui/imgui_impl_win32.o out/imgui/imgui_tables.o out/imgui/imgui_widgets.o"
g++ %flags% -mavx2 -mfma -std=c++17 src/main.cpp out/bvh.o %imguilink% -finput-charset=UTF-8 -fexec-charset=UTF-8 -mwindows -o app.exe -Isrc/ext/imgui/ -L%vulkanlibpath% -lvulkan-1 -lWinmm -lkernel32 -ldwmapi
echo Done.

