@echo off
set "imguisrc=src/ext/imgui/imgui.cpp src/ext/imgui/imgui_draw.cpp src/ext/imgui/backends/imgui_impl_vulkan.cpp src/ext/imgui/backends/imgui_impl_win32.cpp src/ext/imgui/imgui_tables.cpp src/ext/imgui/imgui_widgets.cpp"
if exist out\*.obj del /Q out\*.obj
cl /O2 /arch:AVX2 /std:c++20 /utf-8 /EHsc src/main.cpp %imguisrc% /I src/ext/imgui /Fo:out/  /Fe:app.exe /link /LIBPATH:lib vulkan-1.lib winmm.lib kernel32.lib dwmapi.lib /SUBSYSTEM:WINDOWS
