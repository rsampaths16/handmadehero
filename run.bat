@echo off

rem Trying to kill any running instance of HandmadeHero.exe
taskkill /IM HandmadeHero.exe /F >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Failed to kill HandmadeHero.exe or no process running.
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

rem Start the application
start "" build\Debug\HandmadeHero.exe
if %ERRORLEVEL% NEQ 0 (
    echo Failed to start HandmadeHero.exe.
    exit /b 1
)

rem Exit the script
exit /b 0
