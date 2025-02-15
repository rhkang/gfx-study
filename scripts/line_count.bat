@echo off
setlocal enabledelayedexpansion

echo -- Line Count Script
echo.

REM Set the root directory here
set "srcDir=%~dp0..\src"
set "shaderDir=%~dp0..\shaders"

REM Initialize total line count
set /a totalLines=0

REM Create a temporary file to store the results
set "tempFile=%temp%\linecount.txt"
if exist "%tempFile%" del "%tempFile%"

REM For each file with target extension under the root directory, count the lines
for /r "%srcDir%" %%f in (*.cpp *.h *.hpp) do (
    for /f %%l in ('find /v /c "" ^< "%%f"') do (
        echo   %%f: %%l lines >> "%tempFile%"
        set /a totalLines+=%%l
    )
)

for /r "%shaderDir%" %%f in (*.hlsl *.glsl) do (
    for /f %%l in ('find /v /c "" ^< "%%f"') do (
        echo   %%f: %%l lines >> "%tempFile%"
        set /a totalLines+=%%l
    )
)

REM Sort the results by file name and display them
sort "%tempFile%" /o "%tempFile%.sorted"
type "%tempFile%.sorted"

REM Display the total line count
echo.
echo Total : %totalLines% lines
echo.

REM Clean up
del "%tempFile%"
del "%tempFile%.sorted"

endlocal
@REM pause
