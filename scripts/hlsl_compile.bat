@echo off
setlocal enabledelayedexpansion

set "dxc_root=%~dp0..\tool\dxc"
for %%i in ("%dxc_root%") do set "dxc_root=%%~fi"

:: Determine the correct dxc executable based on CPU architecture
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set "dxc=%dxc_root%\bin\x86\dxc.exe"
) else if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "dxc=%dxc_root%\bin\x64\dxc.exe"
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set "dxc=%dxc_root%\bin\arm64\dxc.exe"
) else (
    echo Not Supported CPU architecture: %PROCESSOR_ARCHITECTURE%
    exit
)

if not exist "%dxc%" (
    echo Compiler : !dxc! not exists.
    exit
) else (
    echo Compiler : !dxc!
)

:: Get argument from PowerShell or use default
set "root=%~1"
if "%root%"=="" (
    set "root=%~dp0../shaders"
)

:: Convert root to an absolute path
for %%i in ("%root%") do set "root=%%~fi"

if not exist "%root%" (
    echo Root directory does not exist: %root%
    exit /b 1
)

echo Root Directory : %root%
echo.

:: Call the process_directory function for root
call :process_directory "%root%"

:: Process all subdirectories by calling the function for each one
for /d %%i in ("%root%\*") do (
    call :process_directory "%%i"
)

:: =============================
:: Function to Process a Directory
:: =============================
:process_directory
set "dir=%~1"
echo.
echo Processing directory: !dir!

for %%j in (!dir!\*.hlsl) do (
    set "file=%%j"

    echo "Compile %%j -> !dir!\%%~nj.vert.spv"
    %dxc% -spirv -T vs_6_0 -E vert %%j -Fo !dir!\out\%%~nj.vert.spv

    echo "Compile %%j -> !dir!\%%~nj.frag.spv"
    %dxc% -spirv -T ps_6_0 -E frag %%j -Fo !dir!\out\%%~nj.frag.spv
)

::echo Compilation completed successfully.
echo.
echo Shader Compilation Done.
exit /b 0