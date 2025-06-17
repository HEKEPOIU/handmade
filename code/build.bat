@echo off


set CommonCompilerFlags=/MT /nologo /EHa- /GR- /Gm- /Od /Oi /WX /W4 /wd4201 /wd4100 /wd4189^
          -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_WIN32=1 /std:c++20 /Z7 /FC /Fm
set CommonLinkerFlags= /OPT:REF User32.lib Gdi32.lib Winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
:: '-' means disable previous option
:: CRT = C RunTime Library
:: MT = Multithreaded, link CRT statically,
::      others have MTd(statically link DEBUG CRT ),
::      MD(dynamic link CRT), MDd(dynamic link DEBUG CRT).
:: EHa = EH means Error Handleing ,a means Enable C++ Exceptions, other have s c r.
:: GR = Enable RTTI, other have GR- Disable RTTI.(it enable RTTI by default)
::      dynamic_cast, typeid depend on RTTI. 
:: Gm = Enable minimal rebuild, other have Gm- Disable minimal rebuild, but it deprecated.
:: Oi = Enable Intrinsic Functions, other have Oi- Disable Intrinsic Functions.
::      (Intrinsic just like inline function, but used asm instead of c/c++ code.)
:: Od = Disable Optimization, other have Od- Enable Optimization.
:: WX = Treat Warning As Error, other have WX- Treat Warning As Warning.
:: W4 = Warning Level 4, other have W3 W2 W1 W0 -W4 larger level means more warnings.
::      and Displays all /W4 Warning and all other warnings that /W4 doesn't include.
::      but msvc it self can't compile with /Wall. 
:: wdxxxx = Disable Warning number xxx.
::          /wd4201: disable nonstandard extension used warning.
::          /wd4100: disable unreferenced formal parameter warning.
::          /wd4189: disable local variable is initialized but not referenced warning.
:: Fma = Create Map File, other have Fm- Don't Create Map File.
:: OPT:REF = Remove unused function or data,
::           other have OPT:NOREF Don't remove unused function or data.

cl %CommonCompilerFlags% "..\code\win32_handmade.cpp" /link %CommonLinkerFlags%

if errorlevel 1 (
    echo [ERROR] Compilation failed.
    popd
    exit /b 1
)

echo [INFO] Compilation succeeded.
popd
