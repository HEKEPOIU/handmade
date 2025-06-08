@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

cl /std:c++20 /Zi /FC "..\code\win32_handmade.cpp" User32.lib Gdi32.lib 
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    popd
    exit /b 1
)

echo [INFO] Compilation succeeded.
popd
