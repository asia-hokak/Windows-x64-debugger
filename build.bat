@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "CONFIG=Release"
set "BUILD_DIR=%ROOT%\build"
set "PYI_WORK_DIR=%ROOT%\build\pyinstaller"
set "DIST_DIR=%ROOT%\dist"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "BACKEND_DLL=%ROOT%\src\frontend\dbg\lib\dbg_backend.dll"
set "ICON_FILE=%ROOT%\icon.png"

echo [1/3] Initialize MSVC environment...
if not exist "%VSWHERE%" (
    echo [ERROR] Cannot find vswhere: "%VSWHERE%"
    echo Please install Visual Studio Build Tools or run from Developer PowerShell for VS.
    exit /b 1
)

set "VCVARS_BAT="
for /f "delims=" %%I in ('""!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat"') do (
    set "VCVARS_BAT=%%I"
)

if "%VCVARS_BAT%"=="" (
    echo [ERROR] Cannot find vcvars64.bat from Visual Studio.
    exit /b 1
)

call "%VCVARS_BAT%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment.
    exit /b 1
)

echo [2/3] Build backend DLL (%CONFIG%)...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] CMake build failed.
    exit /b 1
)

if not exist "%BACKEND_DLL%" (
    echo [ERROR] Backend DLL not found after build: "%BACKEND_DLL%"
    exit /b 1
)

if not exist "%ICON_FILE%" (
    echo [ERROR] Icon file not found: "%ICON_FILE%"
    exit /b 1
)

echo [3/3] Package frontend with PyInstaller...
python -m PyInstaller ^
    --noconfirm ^
    --clean ^
    --onefile ^
    --name dbg ^
    --distpath "%DIST_DIR%" ^
    --workpath "%PYI_WORK_DIR%" ^
    --specpath "%PYI_WORK_DIR%" ^
    --paths "%ROOT%\src\frontend" ^
    --add-data "%ROOT%\src\frontend\dbg\tui\style;dbg\tui\style" ^
    --add-binary "%BACKEND_DLL%;dbg/lib" ^
    --icon "%ICON_FILE%" ^
    "%ROOT%\src\frontend\main.py"
if errorlevel 1 (
    echo [ERROR] PyInstaller packaging failed.
    exit /b 1
)

echo [OK] Build complete.
echo [OK] Output: "%DIST_DIR%\dbg"
exit /b 0
