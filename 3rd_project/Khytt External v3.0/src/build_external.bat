@echo off
setlocal enabledelayedexpansion

:: Define paths
set "VS_PATH=C:\Program Files\Microsoft Visual Studio"
set "VC_VARS="

:: Check common locations for vcvars64.bat
if exist "%VS_PATH%\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VC_VARS=%VS_PATH%\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VC_VARS=%VS_PATH%\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VC_VARS=%VS_PATH%\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_PATH%\18\Community\VC\Auxiliary\Build\vcvars64.bat" set "VC_VARS=%VS_PATH%\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if not defined VC_VARS (
    echo [!] Could not find vcvars64.bat automatically.
    exit /b 1
)

echo [*] Found VS Environment: "%VC_VARS%"
call "%VC_VARS%"

echo [*] Building Khytt CS2 External...
msbuild "Khytt CS2 External.sln" /p:Configuration=Release /p:Platform=x64 /t:Rebuild
if %errorlevel% neq 0 (
    echo [!] Build failed with error code %errorlevel%
    exit /b %errorlevel%
)

echo [+] Build successful!
endlocal
