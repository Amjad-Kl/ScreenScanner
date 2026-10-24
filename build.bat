@echo off

echo ========================================================
echo   ScreenScanner - C++ Native Build (MSVC + WinRT)
echo ========================================================

:: 1. Developer Environment initialisieren falls noch nicht gesetzt
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto COMPILE

echo [*] Lade Visual Studio C++ Compiler Umgebung...

set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" (
    call "%VS_PATH%"
    goto COMPILE
)

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" (
    call "%VS_PATH%"
    goto COMPILE
)

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" (
    call "%VS_PATH%"
    goto COMPILE
)

echo [!] FEHLER: Visual Studio C++ Compiler wurde nicht gefunden!
exit /b 1

:COMPILE
:: Laufende Instanz beenden, damit der Linker die exe überschreiben darf
taskkill /f /im ScreenScanner.exe >nul 2>&1
if not exist "bin" mkdir "bin"

echo [*] Kompiliere ScreenScanner.exe (C++17, Release, WinRT)...

cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /utf-8 /DUNICODE /D_UNICODE /await /I src src\main.cpp src\overlay.cpp src\capture.cpp src\ocr.cpp src\clipboard.cpp /Fe:bin\ScreenScanner.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib windowsapp.lib

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo   [OK] Build erfolgreich!
    echo   Ausgabe: bin\ScreenScanner.exe
    echo ========================================================
    if exist main.obj del *.obj >nul 2>&1
) else (
    echo.
    echo [!] FEHLER beim Kompilieren!
    exit /b 1
)
