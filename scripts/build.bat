@echo off

set "root=%~dp0.."
echo %root%

:: specifying options
cmake -S %root% -B %root%\build

:: cmake --build build --config Release
cmake --build %root%\build

@REM pause