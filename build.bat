@echo off
cd /d "%~dp0"

echo ========================================================
echo   ScreenScanner - C++ Native Build (MSVC + WinRT)
echo ========================================================

:: Laufende Instanz beenden falls aktiv
powershell -NoProfile -Command "Stop-Process -Name ScreenScanner -Force -ErrorAction SilentlyContinue" >nul 2>&1

:: 1. Developer Environment initialisieren falls noch nicht gesetzt
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto COMPILE

echo [*] Lade Visual Studio C++ Compiler Umgebung...

set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" goto SETUP_VS

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" goto SETUP_VS

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%" goto SETUP_VS

echo [!] FEHLER: Visual Studio C++ Compiler wurde nicht gefunden!
exit /b 1

:SETUP_VS
call "%VS_PATH%"
if %ERRORLEVEL% neq 0 (
    echo [!] FEHLER: vcvars64.bat konnte nicht ausgefuehrt werden!
    exit /b 1
)

:COMPILE
if not exist "bin" mkdir "bin"
if not exist "obj" mkdir "obj"

echo [*] Kompiliere ScreenScanner.exe (C++17, Release, WinRT)...

cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /utf-8 /DUNICODE /D_UNICODE /await /I src /Fo:obj\ src\main.cpp src\overlay.cpp src\capture.cpp src\ocr.cpp src\clipboard.cpp src\sanitizer.cpp src\query_builder.cpp src\browser.cpp /Fe:bin\ScreenScanner.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib windowsapp.lib winhttp.lib advapi32.lib

set "BUILD_ERR=%ERRORLEVEL%"

:: Zwischenordner immer aufraeumen
if exist "obj" rmdir /s /q "obj" >nul 2>&1

if %BUILD_ERR% equ 0 (
    echo.
    echo ========================================================
    echo   [OK] Build erfolgreich!
    echo   Ausgabe: bin\ScreenScanner.exe
    echo ========================================================
) else (
    echo.
    echo [!] FEHLER beim Kompilieren oder Linken!
    exit /b 1
)