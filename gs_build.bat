@echo off
set UBT="D:\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
if not exist %UBT% (
  echo UBT_NOT_FOUND at %UBT% > "D:\goblinRaid\build3.log"
  exit /b 1
)
%UBT% MyProjectEditor Win64 Development -Project="D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject" -WaitMutex -NoHotReload > "D:\goblinRaid\build3.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "D:\goblinRaid\build3.log"
