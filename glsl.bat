@echo off
REM usage: glsl [name in dat folder] [vert/frag/comp]

set glslsrc=C:\VulkanSDK\1.4.350.0\Bin\glslc.exe

REM add -g for debug info
REM add -O for optimization
set "compileflags=-O"

%glslsrc% %compileflags% --target-env=vulkan1.3 --target-spv=spv1.4 dat/%1.%2 %3 -o dat/bin/%2_%1.spv
if %ERRORLEVEL% EQU 0 (
echo Created dat/%2_%1.spv
) else (
echo "[glsl.bat] Compilation failed!"
exit /b 1
)

