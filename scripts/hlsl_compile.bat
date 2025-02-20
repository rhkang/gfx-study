@echo off
setlocal enabledelayedexpansion

:: Get script directory
set "script=%~f0"
set "script_dir=%~dp0"
for %%I in ("%script_dir%\..") do set "project_root=%%~fI"
for %%I in ("%project_root%\tool\dxc") do set "dxc_root=%%~fI"

:: Determine the correct dxc executable based on CPU architecture
set "arch="
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set "arch=x86"
) else if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "arch=x64"
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set "arch=arm64"
) else (
    echo Not Supported CPU architecture: %PROCESSOR_ARCHITECTURE%
    exit
)

set "dxc=%dxc_root%\win\bin\%arch%\dxc.exe"

if not exist "%dxc%" (
    echo Compiler: %dxc% does not exist.
    exit /b 1
) else (
    echo Compiler: %dxc%
)

set "root=%project_root%\shaders"

if not exist "%root%" (
    echo Root directory does not exist: %root%
    exit /b 1
)

echo Root Directory: %root%
echo.

:: Ensure the output directory exists
if not exist "%root%\out" mkdir "%root%\out"

:: Function to compile HLSL files
:compile_hlsl
set "file=%~1"
for %%I in ("%file%") do set "file=%%~fI"
if "%file%"=="" goto find_files

echo Processing : %file%

:: Extract relative path properly
set "relative_path=%file:*%root%\=%"
set "relative_path=!relative_path:%root%\=!"
set "output_dir=%root%\out\!relative_path:%%~nxj=\!"
for %%I in ("%output_dir%") do set "output_dir=%%~dpI"

if not exist "%output_dir%" mkdir "%output_dir%"

set "base_name=%~n1"

"%dxc%" -spirv -T vs_6_0 -E vert "%file%" -Fo "%output_dir%\%base_name%.vert.spv"
"%dxc%" -spirv -T ps_6_0 -E frag "%file%" -Fo "%output_dir%\%base_name%.frag.spv"

shift
if not "%~1"=="" goto compile_hlsl
exit /b 0

:find_files
for /r "%root%" %%F in (*.hlsl) do call :compile_hlsl "%%F"
exit /b 0

:: Start compiling
if "%~1"=="" (
    call :find_files
) else (
    call :compile_hlsl %*
)
