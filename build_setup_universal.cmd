@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Tigirl - Universal Installer Build

pushd "%~dp0"
if errorlevel 1 (
    echo ERROR: Cannot open the project directory.
    pause
    exit /b 1
)

set "TIGIRL_BUILD_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "TIGIRL_BUILD_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"

echo Tigirl universal installer build: x64 / ARM64 / x86
echo Project: "%CD%"
echo Progress and errors will appear in this window.
echo Detailed logs: "%CD%\build\setup-runs"
echo.

"%TIGIRL_BUILD_PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_setup_universal.ps1" %*
set "TIGIRL_BUILD_EXIT=%ERRORLEVEL%"

echo.
if "%TIGIRL_BUILD_EXIT%"=="0" (
    echo Command completed successfully.
) else (
    echo Command failed. Exit code: %TIGIRL_BUILD_EXIT%
)
echo Build status and logs: "%CD%\build\setup-runs\latest-universal.json"
echo Latest successful installer: "%CD%\build\packages\latest-universal.json"
echo.
popd
pause
exit /b %TIGIRL_BUILD_EXIT%
