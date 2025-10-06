@echo off
taskkill /IM HandmadeHero.exe /F >nul 2>&1
cmake -S . -B build
cmake --build build
start "" build\Debug\HandmadeHero.exe
exit /b 0
