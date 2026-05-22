@echo off
setlocal

cd /d "%~dp0"

set "BUILD_DIR=build-ninja"
set "TARGET=TibiaMapEditor"

echo Configuring %BUILD_DIR% for Release...
cmake -S . -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :fail

echo Building %TARGET%...
cmake --build "%BUILD_DIR%" --target %TARGET%
if errorlevel 1 goto :fail

echo.
echo Build complete:
echo   %CD%\%BUILD_DIR%\%TARGET%.exe
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1
