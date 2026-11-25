@echo off
setlocal

pushd "%~dp0"
if errorlevel 1 goto failed_without_pop

set "EXE=build\release\NoWave_artefacts\Release\Standalone\NoWave.exe"
set "BUILD=0"

if /I "%~1"=="--build" set "BUILD=1"
if not exist "%EXE%" set "BUILD=1"

if "%BUILD%"=="1" (
    echo Configuring NoWave...
    cmake --preset windows-release
    if errorlevel 1 goto failed

    echo Building NoWave Standalone...
    cmake --build --preset windows-release --target NoWave_Standalone
    if errorlevel 1 goto failed
)

if not exist "%EXE%" (
    echo Build completed, but the Standalone executable was not found:
    echo %EXE%
    goto failed
)

echo Starting NoWave...
start "" "%EXE%"
popd
exit /b 0

:failed
popd
:failed_without_pop
echo NoWave could not be built or started.
pause
exit /b 1
