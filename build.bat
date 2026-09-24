@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  Compiling Vibe Loader (Manual Map Core + DX11 UI)
echo ========================================================

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Professional"
if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    echo Error: Visual Studio 2026 x64 environment not found!
    exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if not exist "bin" mkdir bin

set INCLUDES=/Isrc /Iimgui /Iimgui\backends
set SOURCES=src\main.cpp src\ui.cpp src\injection.cpp src\updater.cpp src\config.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\backends\imgui_impl_win32.cpp imgui\backends\imgui_impl_dx11.cpp
set LIBS=d3d11.lib dxgi.lib dwmapi.lib user32.lib gdi32.lib comctl32.lib ole32.lib shell32.lib wininet.lib advapi32.lib
set CFLAGS=/nologo /O2 /MD /utf-8 /std:c++17 /EHsc /D UNICODE /D _UNICODE /D NDEBUG
set LFLAGS=/link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /OUT:bin\NeoNirvana.exe

echo Compiling sources...
cl %CFLAGS% %INCLUDES% %SOURCES% %LIBS% %LFLAGS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo  BUILD SUCCESSFUL: bin\NeoNirvana.exe
    echo ========================================================
    del *.obj >nul 2>&1
    exit /b 0
) else (
    echo.
    echo [!] Build failed with error %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
