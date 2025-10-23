@echo off

rem Trying to kill any running instance of win32_handmade.exe
taskkill /IM win32_handmade.exe /F >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Failed to kill win32_handmade.exe or no process running.
)

rem Run cmake to configure the build
cmake -S . -B build
if %ERRORLEVEL% NEQ 0 (
    echo cmake configuration failed.
    exit /b 1
)

rem Build the project
cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b 1
)

rem Change current working directory to Debug
cd build\Debug

rem Start the application
start "" win32_handmade.exe
if %ERRORLEVEL% NEQ 0 (
    echo Failed to start win32_handmade.exe.
    exit /b 1
)

rem Go back to old directory
cd ../..

rem Exit the script
exit /b 0
