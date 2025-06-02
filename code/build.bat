@echo off

mkdir ..\build
pushd ..\build

cl /std:c++20 /Zi /FC "..\code\win32_handmade.cpp" User32.lib Gdi32.lib 
popd
