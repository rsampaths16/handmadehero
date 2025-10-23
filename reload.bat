@echo off

rem Run cmake to configure the build
cmake -S . -B build
if %ERRORLEVEL% NEQ 0 (
    echo cmake configuration failed.
    exit /b 1
)

rem Build the project
cmake --build build --target handmade
if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b 1
)

rem Exit the script
exit /b 0
