@echo off
setlocal

pushd "%~dp0"
if errorlevel 1 goto failed_without_pop

echo Configuring NoWave...
cmake --preset windows-release
if errorlevel 1 goto failed

echo Building NoWave VST3 and Standalone...
cmake --build --preset windows-release --parallel 8
if errorlevel 1 goto failed

echo.
echo Build completed successfully.
echo VST3: build\release\NoWave_artefacts\Release\VST3\NoWave.vst3
echo EXE:  build\release\NoWave_artefacts\Release\Standalone\NoWave.exe
popd
pause
exit /b 0

:failed
echo.
echo Build failed. Check the CMake output above.
popd
pause
exit /b 1

:failed_without_pop
echo Could not access the NoWave project directory.
pause
exit /b 1
