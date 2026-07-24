@echo off
setlocal
cd /d "%~dp0"
echo ============================================
echo  GoblinSiege: VibeUE install + build
echo ============================================
echo.
if exist "Plugins\VibeUE\VibeUE.uplugin" goto build

where git >nul 2>nul
if errorlevel 1 goto nogit

echo Cloning VibeUE, branch 5-8 ...
git clone -b 5-8 https://github.com/kevinpbuckley/VibeUE.git "Plugins\VibeUE"
if errorlevel 1 goto clonefail

:build
echo.
echo Building the plugin now. Make sure the Unreal editor is CLOSED.
echo This can take a few minutes - close browsers and VS to free RAM.
echo.
call "Plugins\VibeUE\BuildPlugin.bat"
echo.
echo ============================================
echo  Done. Next steps:
echo   1. Open the project in the editor
echo   2. Edit - Plugins - search VibeUE in the Project category - Enable - restart
echo   3. Tools - VibeUE - AI Chat - gear icon - paste API key from vibeue.com/login
echo   4. Run console command: VibeUE.GenerateAgentConfig
echo ============================================
pause
exit /b 0

:nogit
echo ERROR: git not found in PATH.
echo Either install Git for Windows, or copy an existing Plugins\VibeUE folder here manually.
pause
exit /b 1

:clonefail
echo ERROR: clone failed. Check your internet connection and try again.
pause
exit /b 1
